#include "Editor/SGenLab.h"

#include "Engine/Texture2D.h"
#include "Generation/LabImageWriter.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
#include "IDetailsView.h"
#include "ImageUtils.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "UObject/Package.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SGenLab"

void SGenLab::Construct(FArguments const&) {
    settings_.Reset(NewObject<UGenLabSettings>());
    last_generator_ = settings_->generator;

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
              SSplitter::Slot().Value(
                  0.45f)[SNew(SVerticalBox) +
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
                              SHorizontalBox::Slot().AutoWidth()
                                  [SNew(SButton)
                                       .Text(LOCTEXT("OpenFolder", "Open Output Folder"))
                                       .OnClicked(this, &SGenLab::open_output_directory)]] +
                         SVerticalBox::Slot().AutoHeight().Padding(
                             0.0f, 8.0f, 0.0f, 0.0f)[SNew(STextBlock)
                                                         .Text_Lambda([this]() { return status_; })
                                                         .AutoWrapText(true)]] +
              SSplitter::Slot().Value(0.55f)[SNew(SBorder).Padding(
                  8.0f)[SNew(SBox)
                            .HAlign(HAlign_Center)
                            .VAlign(VAlign_Center)[SNew(SImage).Image(&preview_brush_)]]]]];

    update_preview();
}

SGenLab::~SGenLab() {
    preview_brush_.SetResourceObject(nullptr);
    preview_texture_.Reset();
    settings_.Reset();
}

void SGenLab::on_property_changed(FPropertyChangedEvent const&) {
    if (last_generator_ != settings_->generator) {
        last_generator_ = settings_->generator;
        settings_->load_generator_defaults();
        details_view_->ForceRefresh();
    }
    update_preview();
}

void SGenLab::update_preview() {
    auto const image{SandboxImages::GenLab::generate_image(settings_->to_request())};
    if (!image.is_valid()) {
        status_ = FText::FromString(image.error);
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
    auto* const texture{FImageUtils::CreateTexture2D(image.width,
                                                     image.height,
                                                     image.pixels,
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
    preview_brush_.ImageSize =
        FVector2D{static_cast<double>(image.width), static_cast<double>(image.height)};
    status_ = FText::Format(LOCTEXT("PreviewReady", "Preview: {0} x {1}"),
                            FText::AsNumber(image.width),
                            FText::AsNumber(image.height));
    Invalidate(EInvalidateWidgetReason::Paint);
}

auto SGenLab::generate_selected() -> FReply {
    auto const request{settings_->to_request()};
    auto const success{SandboxImages::GenLab::generate_and_import(request)};
    status_ = success
                ? FText::Format(LOCTEXT("SelectedSucceeded", "Generated and imported {0}."),
                                FText::FromString(request.output_name))
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
    auto const output_directory{SandboxImages::GenLab::get_output_directory()};
    if (output_directory.IsEmpty()) {
        status_ = LOCTEXT("NoOutputDirectory", "The SandboxImages plugin directory was not found.");
        return FReply::Handled();
    }

    IFileManager::Get().MakeDirectory(*output_directory, true);
    FPlatformProcess::ExploreFolder(*output_directory);
    return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE
