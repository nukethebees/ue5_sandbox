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
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SGenLab"

namespace {
constexpr int32 maximum_preview_dimension{512};

auto scale_request_for_preview(SandboxImages::GenLab::FGenerationRequest request,
                               bool const tiled_preview)
    -> SandboxImages::GenLab::FGenerationRequest {
    auto const maximum_source_dimension{tiled_preview ? maximum_preview_dimension / 2
                                                      : maximum_preview_dimension};
    int32 width{};
    int32 height{};
    switch (request.generator) {
        case SandboxImages::GenLab::EGeneratorType::RadialGradient:
            width = request.radial_gradient.width;
            height = request.radial_gradient.height;
            break;
        case SandboxImages::GenLab::EGeneratorType::RingMask:
            width = request.ring_mask.width;
            height = request.ring_mask.height;
            break;
        case SandboxImages::GenLab::EGeneratorType::Starfield:
            width = request.starfield.width;
            height = request.starfield.height;
            break;
        case SandboxImages::GenLab::EGeneratorType::Noise:
            width = request.noise.width;
            height = request.noise.height;
            break;
        case SandboxImages::GenLab::EGeneratorType::HexGrid:
            width = request.hex_grid.width;
            height = request.hex_grid.height;
            break;
    }

    if (width <= 0 || height <= 0) {
        return request;
    }

    auto const scale{FMath::Min(1.0f,
                                static_cast<float>(maximum_source_dimension) /
                                    static_cast<float>(FMath::Max(width, height)))};
    auto const preview_width{FMath::Max(1, FMath::RoundToInt(static_cast<float>(width) * scale))};
    auto const preview_height{FMath::Max(1, FMath::RoundToInt(static_cast<float>(height) * scale))};
    switch (request.generator) {
        case SandboxImages::GenLab::EGeneratorType::RadialGradient:
            request.radial_gradient.width = preview_width;
            request.radial_gradient.height = preview_height;
            break;
        case SandboxImages::GenLab::EGeneratorType::RingMask:
            request.ring_mask.width = preview_width;
            request.ring_mask.height = preview_height;
            break;
        case SandboxImages::GenLab::EGeneratorType::Starfield:
            request.starfield.width = preview_width;
            request.starfield.height = preview_height;
            request.starfield.star_count =
                FMath::RoundToInt(static_cast<float>(request.starfield.star_count) * scale * scale);
            request.starfield.minimum_radius *= scale;
            request.starfield.maximum_radius *= scale;
            break;
        case SandboxImages::GenLab::EGeneratorType::Noise:
            request.noise.width = preview_width;
            request.noise.height = preview_height;
            request.noise.base_scale *= scale;
            break;
        case SandboxImages::GenLab::EGeneratorType::HexGrid:
            request.hex_grid.width = preview_width;
            request.hex_grid.height = preview_height;
            request.hex_grid.cell_radius *= scale;
            request.hex_grid.line_thickness *= scale;
            request.hex_grid.falloff *= scale;
            break;
    }
    return request;
}

auto preview_channel_value(FColor const pixel, EGenLabPreviewChannel const channel) -> uint8 {
    switch (channel) {
        case EGenLabPreviewChannel::Red:
            return pixel.R;
        case EGenLabPreviewChannel::Green:
            return pixel.G;
        case EGenLabPreviewChannel::Blue:
            return pixel.B;
        case EGenLabPreviewChannel::Alpha:
            return pixel.A;
        case EGenLabPreviewChannel::Color:
        case EGenLabPreviewChannel::RGB:
            break;
    }
    return 0;
}

auto make_display_image(SandboxImages::GenLab::FGeneratedImage const& source,
                        EGenLabPreviewChannel const channel,
                        bool const tiled) -> SandboxImages::GenLab::FGeneratedImage {
    auto const tile_count{tiled ? 2 : 1};
    SandboxImages::GenLab::FGeneratedImage display{.width = source.width * tile_count,
                                                   .height = source.height * tile_count};
    display.pixels.SetNumUninitialized(display.width * display.height);
    for (int32 y{0}; y < display.height; ++y) {
        for (int32 x{0}; x < display.width; ++x) {
            auto const source_pixel{
                source.pixels[(y % source.height) * source.width + x % source.width]};
            auto& display_pixel{display.pixels[y * display.width + x]};
            if (channel == EGenLabPreviewChannel::Color) {
                auto const checker_value{((x / 16 + y / 16) & 1) == 0 ? uint8{48} : uint8{80}};
                auto const alpha{static_cast<float>(source_pixel.A) / 255.0f};
                display_pixel = {static_cast<uint8>(FMath::RoundToInt(
                                     FMath::Lerp(static_cast<float>(checker_value),
                                                 static_cast<float>(source_pixel.R),
                                                 alpha))),
                                 static_cast<uint8>(FMath::RoundToInt(
                                     FMath::Lerp(static_cast<float>(checker_value),
                                                 static_cast<float>(source_pixel.G),
                                                 alpha))),
                                 static_cast<uint8>(FMath::RoundToInt(
                                     FMath::Lerp(static_cast<float>(checker_value),
                                                 static_cast<float>(source_pixel.B),
                                                 alpha))),
                                 255};
            } else if (channel == EGenLabPreviewChannel::RGB) {
                display_pixel = {source_pixel.R, source_pixel.G, source_pixel.B, 255};
            } else {
                auto const value{preview_channel_value(source_pixel, channel)};
                display_pixel = {value, value, value, 255};
            }
        }
    }
    return display;
}
}

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
    if (last_generator_ != settings_->generator) {
        last_generator_ = settings_->generator;
        settings_->load_generator_defaults();
        details_view_->ForceRefresh();
    }
    update_preview();
}

void SGenLab::update_preview() {
    auto const request{settings_->to_request()};
    auto const preview_request{scale_request_for_preview(request, settings_->tiled_preview)};
    auto const image{SandboxImages::GenLab::generate_image(preview_request)};
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
    auto const display_image{
        make_display_image(image, settings_->preview_channel, settings_->tiled_preview)};
    auto* const texture{FImageUtils::CreateTexture2D(display_image.width,
                                                     display_image.height,
                                                     display_image.pixels,
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
