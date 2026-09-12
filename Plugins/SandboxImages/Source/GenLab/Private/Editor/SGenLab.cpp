#include "Editor/SGenLab.h"

#include "Engine/Texture2D.h"
#include "Generation/GeneratedImageAssetWriter.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
#include "IDetailsView.h"
#include "ImageUtils.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "UObject/Package.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SGenLab"

namespace {
constexpr int32 maximum_preview_dimension{512};

auto to_native_preview_channel(EGenLabPreviewChannel const channel)
    -> sandbox::image::PreviewChannel {
    switch (channel) {
        case EGenLabPreviewChannel::Red:
            return sandbox::image::PreviewChannel::Red;
        case EGenLabPreviewChannel::Green:
            return sandbox::image::PreviewChannel::Green;
        case EGenLabPreviewChannel::Blue:
            return sandbox::image::PreviewChannel::Blue;
        case EGenLabPreviewChannel::Alpha:
            return sandbox::image::PreviewChannel::Alpha;
        case EGenLabPreviewChannel::Color:
            return sandbox::image::PreviewChannel::Color;
        case EGenLabPreviewChannel::RGB:
            return sandbox::image::PreviewChannel::RGB;
    }
    return sandbox::image::PreviewChannel::Color;
}

auto to_unreal_pixels(sandbox::image::GeneratedImage const& image) -> TArray<FColor> {
    TArray<FColor> pixels;
    pixels.Reserve(static_cast<int32>(image.pixels.size()));
    for (auto const pixel : image.pixels) {
        pixels.Emplace(pixel.red, pixel.green, pixel.blue, pixel.alpha);
    }
    return pixels;
}
}

void SGenLab::Construct(FArguments const&) {
    settings_.Reset(NewObject<UGenLabSettings>());
    last_generator_ = settings_->generator;
    auto requests{sandbox::image::default_generation_requests()};
    presets_.Reserve(static_cast<int32>(requests.size()));
    for (auto& request : requests) {
        presets_.Add(MakeShared<sandbox::image::GenerationRequest>(MoveTemp(request)));
    }
    if (!presets_.IsEmpty()) {
        selected_preset_ = presets_[0];
        settings_->load_request(*selected_preset_);
        last_generator_ = settings_->generator;
    }

    FDetailsViewArgs details_arguments{};
    details_arguments.bAllowSearch = false;
    details_arguments.bHideSelectionTip = true;
    details_arguments.NameAreaSettings = FDetailsViewArgs::HideNameArea;
    auto& property_editor{
        FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor")};
    details_view_ = property_editor.CreateDetailView(details_arguments);
    details_view_->SetObject(settings_.Get());
    details_view_->OnFinishedChangingProperties().AddSP(this, &SGenLab::on_property_changed);

    preview_brush_.DrawAs = ESlateBrushDrawType::Image;
    preview_brush_.ImageType = ESlateBrushImageType::FullColor;
    preview_brush_.Tiling = ESlateBrushTileType::NoTile;
    preview_brush_.ImageSize = FVector2D{512.0, 512.0};

    ChildSlot[SNew(SBorder).Padding(
        8.0f)[SNew(SSplitter) +
              SSplitter::Slot().Value(0.45f)
                  [SNew(SVerticalBox) +
                   SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 8.0f)
                       [SNew(SHorizontalBox) +
                        SHorizontalBox::Slot()
                            .AutoWidth()
                            .VAlign(VAlign_Center)
                            .Padding(0.0f, 0.0f, 8.0f, 0.0f)
                                [SNew(STextBlock).Text(LOCTEXT("PresetLabel", "Preset"))] +
                        SHorizontalBox::Slot().FillWidth(1.0f)
                            [SAssignNew(
                                 preset_combo_,
                                 SComboBox<TSharedPtr<sandbox::image::GenerationRequest>>)
                                 .OptionsSource(&presets_)
                                 .InitiallySelectedItem(selected_preset_)
                                 .OnGenerateWidget(this, &SGenLab::make_preset_widget)
                                 .OnSelectionChanged(this, &SGenLab::on_preset_selected)
                                     [SNew(STextBlock).Text(this, &SGenLab::preset_text)]]] +
                   SVerticalBox::Slot().FillHeight(1.0f)[details_view_.ToSharedRef()] +
                   SVerticalBox::Slot().AutoHeight().Padding(0.0f, 8.0f, 0.0f, 0.0f)
                       [SNew(SHorizontalBox) +
                        SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 6.0f, 0.0f)
                            [SNew(SButton)
                                 .Text(LOCTEXT("GenerateSelected", "Generate Selected"))
                                 .OnClicked(this, &SGenLab::generate_selected)] +
                        SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 6.0f, 0.0f)
                            [SNew(SButton)
                                 .Text(LOCTEXT("GenerateAll", "Generate All Defaults"))
                                 .OnClicked(this, &SGenLab::generate_all)] +
                        SHorizontalBox::Slot()
                            .AutoWidth()[SNew(SButton)
                                             .Text(LOCTEXT("OpenFolder", "Open Output Folder"))
                                             .OnClicked(this, &SGenLab::open_output_directory)]] +
                   SVerticalBox::Slot().AutoHeight().Padding(
                       0.0f, 8.0f, 0.0f, 0.0f)[SNew(STextBlock)
                                                   .Text_Lambda([this]() { return status_; })
                                                   .AutoWrapText(true)]] +
              SSplitter::Slot().Value(0.55f)[SNew(SBorder).Padding(
                  8.0f)[SNew(SBox)
                            .WidthOverride(512.0f)
                            .HeightOverride(512.0f)
                            .HAlign(HAlign_Center)
                            .VAlign(VAlign_Center)[SNew(SScaleBox).Stretch(
                                EStretch::ScaleToFit)[SNew(SImage).Image(&preview_brush_)]]]]]];

    update_preview();
}

SGenLab::~SGenLab() {
    preview_brush_.SetResourceObject(nullptr);
    preview_texture_.Reset();
    settings_.Reset();
}

void SGenLab::on_property_changed(FPropertyChangedEvent const&) {
    selected_preset_.Reset();
    if (preset_combo_.IsValid()) {
        preset_combo_->ClearSelection();
    }
    if (last_generator_ != settings_->generator) {
        last_generator_ = settings_->generator;
        settings_->load_generator_defaults();
        details_view_->ForceRefresh();
    }
    update_preview();
}

void SGenLab::on_preset_selected(TSharedPtr<sandbox::image::GenerationRequest> const preset,
                                 ESelectInfo::Type const) {
    if (!preset.IsValid()) {
        return;
    }

    selected_preset_ = preset;
    settings_->load_request(*preset);
    last_generator_ = settings_->generator;
    details_view_->ForceRefresh();
    update_preview();
}

auto SGenLab::make_preset_widget(TSharedPtr<sandbox::image::GenerationRequest> const preset)
    const -> TSharedRef<SWidget> {
    return SNew(STextBlock)
        .Text(preset.IsValid()
                  ? FText::FromString(UTF8_TO_TCHAR(preset->output_name.c_str()))
                  : FText::GetEmpty());
}

auto SGenLab::preset_text() const -> FText {
    return selected_preset_.IsValid()
               ? FText::FromString(UTF8_TO_TCHAR(selected_preset_->output_name.c_str()))
                                      : LOCTEXT("CustomPreset", "Custom");
}

void SGenLab::update_preview() {
    auto const request{settings_->to_request()};
    auto const preview_request{sandbox::image::scale_request_for_preview(
        request, maximum_preview_dimension, settings_->tiled_preview)};
    auto const image{sandbox::image::generate_image(preview_request)};
    if (!image.is_valid()) {
        status_ = FText::FromString(UTF8_TO_TCHAR(image.error.c_str()));
        preview_brush_.SetResourceObject(nullptr);
        preview_texture_.Reset();
        Invalidate(EInvalidateWidgetReason::Paint);
        return;
    }

    FCreateTexture2DParameters texture_parameters{};
    texture_parameters.bSRGB = false;
    texture_parameters.CompressionSettings = TC_VectorDisplacementmap;
    texture_parameters.MipGenSettings = TMGS_NoMipmaps;
    static uint64 preview_index{0};
    auto const texture_name{
        FString::Printf(TEXT("SandboxImagesGenLabPreview_%llu"), ++preview_index)};
    auto const display_image{sandbox::image::make_preview_image(
        image, to_native_preview_channel(settings_->preview_channel), settings_->tiled_preview)};
    auto const display_pixels{to_unreal_pixels(display_image)};
    auto* const texture{FImageUtils::CreateTexture2D(display_image.width,
                                                     display_image.height,
                                                     display_pixels,
                                                     GetTransientPackage(),
                                                     texture_name,
                                                     RF_Transient,
                                                     texture_parameters)};
    if (texture == nullptr) {
        status_ = LOCTEXT("PreviewFailed", "Failed to create the transient preview texture.");
        return;
    }

    preview_texture_.Reset(texture);
    preview_brush_.SetResourceObject(texture);
    preview_brush_.ImageSize = FVector2D{static_cast<double>(display_image.width),
                                         static_cast<double>(display_image.height)};
    status_ = FText::Format(LOCTEXT("PreviewReady", "Output: {0} x {1} | Preview: {2} x {3}"),
                            FText::AsNumber(settings_->width),
                            FText::AsNumber(settings_->height),
                            FText::AsNumber(display_image.width),
                            FText::AsNumber(display_image.height));
    Invalidate(EInvalidateWidgetReason::Paint);
}

auto SGenLab::generate_selected() -> FReply {
    auto const request{settings_->to_request()};
    auto const success{
        SandboxImages::GenLab::generate_and_import(request, SandboxImages::GenLab::lab_content_path)};
    status_ = success
                ? FText::Format(LOCTEXT("SelectedSucceeded", "Generated and imported {0}."),
                                FText::FromString(UTF8_TO_TCHAR(request.output_name.c_str())))
                : LOCTEXT("SelectedFailed", "Generation failed. See the Output Log for details.");
    return FReply::Handled();
}

auto SGenLab::generate_all() -> FReply {
    auto const success{SandboxImages::GenLab::regenerate_all()};
    status_ = success ? LOCTEXT("AllSucceeded", "Generated and imported all default Lab images.")
                      : LOCTEXT("AllFailed",
                                "One or more images failed. See the Output Log for details.");
    return FReply::Handled();
}

auto SGenLab::open_output_directory() -> FReply {
    auto const output_directory{
        SandboxImages::GenLab::get_output_directory(SandboxImages::GenLab::lab_content_path)};
    if (output_directory.IsEmpty()) {
        status_ = LOCTEXT("NoOutputDirectory", "The SandboxImages plugin directory was not found.");
        return FReply::Handled();
    }

    IFileManager::Get().MakeDirectory(*output_directory, true);
    FPlatformProcess::ExploreFolder(*output_directory);
    return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE
