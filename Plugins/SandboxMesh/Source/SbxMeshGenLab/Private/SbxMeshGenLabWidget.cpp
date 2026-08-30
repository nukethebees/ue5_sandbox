#include "SbxMeshGenLab/SbxMeshGenLabWidget.h"

#include "Generation/MeshAssetWriter.h"
#include "SbxMeshGenLab/BoxGenerator.h"

#include "AssetThumbnail.h"
#include "Engine/StaticMesh.h"
#include "Styling/AppStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

namespace {
FName const generated_box_asset_name{TEXT("SM_GeneratedBox")};
constexpr uint32 thumbnail_size{256};
constexpr float minimum_dimension{1.0f};
constexpr float maximum_dimension{100000.0f};
constexpr float maximum_slider_dimension{1000.0f};
}

USbxMeshGenLabWidget::USbxMeshGenLabWidget() {
    TabDisplayName = NSLOCTEXT("SbxMeshGenLab", "TabName", "Mesh Gen Lab");
    bAlwaysReregisterWithWindowsMenu = true;
}

auto USbxMeshGenLabWidget::RebuildWidget() -> TSharedRef<SWidget> {
    thumbnail_pool_ = MakeShared<FAssetThumbnailPool>(1);
    thumbnail_ =
        MakeShared<FAssetThumbnail>(FAssetData{}, thumbnail_size, thumbnail_size, thumbnail_pool_);

    FAssetThumbnailConfig thumbnail_config{};
    thumbnail_config.ThumbnailLabel = EThumbnailLabel::NoLabel;
    thumbnail_config.ShowAssetColor = false;

    auto const preview_widget{thumbnail_->MakeThumbnailWidget(thumbnail_config)};
    auto const generated_box_object_path{
        SandboxMesh::get_generated_asset_object_path(generated_box_asset_name)};
    set_preview_mesh(LoadObject<UStaticMesh>(nullptr, *generated_box_object_path));

    return SNew(SBorder)
        .BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
        .Padding(12.0f)
            [SNew(SVerticalBox) +
             SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 4.0f)
                 [SNew(STextBlock)
                      .Text(NSLOCTEXT("SbxMeshGenLab", "Title", "Mesh Generation Lab"))
                      .Font(FAppStyle::Get().GetFontStyle("HeadingExtraSmall"))] +
             SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 10.0f)
                 [SNew(STextBlock)
                      .Text(NSLOCTEXT("SbxMeshGenLab",
                                      "Description",
                                      "Generate a disposable procedural box mesh."))] +
             SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 4.0f)
                 [SNew(STextBlock)
                      .Text(NSLOCTEXT("SbxMeshGenLab", "Dimensions", "Dimensions (cm)"))] +
             SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 4.0f)
                 [SNew(SHorizontalBox) +
                  SHorizontalBox::Slot()
                      .AutoWidth()
                      .VAlign(VAlign_Center)
                      .Padding(0.0f, 0.0f, 6.0f, 0.0f)
                          [SNew(STextBlock).Text(NSLOCTEXT("SbxMeshGenLab", "DimensionX", "X"))] +
                  SHorizontalBox::Slot().FillWidth(
                      1.0f)[SNew(SSpinBox<float>)
                                .Value_Lambda([this] { return box_dimensions_.X; })
                                .MinValue(minimum_dimension)
                                .MaxValue(maximum_dimension)
                                .MinSliderValue(minimum_dimension)
                                .MaxSliderValue(maximum_slider_dimension)
                                .Delta(1.0f)
                                .OnValueChanged_Lambda(
                                    [this](float const value) { box_dimensions_.X = value; })]] +
             SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 4.0f)
                 [SNew(SHorizontalBox) +
                  SHorizontalBox::Slot()
                      .AutoWidth()
                      .VAlign(VAlign_Center)
                      .Padding(0.0f, 0.0f, 6.0f, 0.0f)
                          [SNew(STextBlock).Text(NSLOCTEXT("SbxMeshGenLab", "DimensionY", "Y"))] +
                  SHorizontalBox::Slot().FillWidth(
                      1.0f)[SNew(SSpinBox<float>)
                                .Value_Lambda([this] { return box_dimensions_.Y; })
                                .MinValue(minimum_dimension)
                                .MaxValue(maximum_dimension)
                                .MinSliderValue(minimum_dimension)
                                .MaxSliderValue(maximum_slider_dimension)
                                .Delta(1.0f)
                                .OnValueChanged_Lambda(
                                    [this](float const value) { box_dimensions_.Y = value; })]] +
             SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 10.0f)
                 [SNew(SHorizontalBox) +
                  SHorizontalBox::Slot()
                      .AutoWidth()
                      .VAlign(VAlign_Center)
                      .Padding(0.0f, 0.0f, 6.0f, 0.0f)
                          [SNew(STextBlock).Text(NSLOCTEXT("SbxMeshGenLab", "DimensionZ", "Z"))] +
                  SHorizontalBox::Slot().FillWidth(
                      1.0f)[SNew(SSpinBox<float>)
                                .Value_Lambda([this] { return box_dimensions_.Z; })
                                .MinValue(minimum_dimension)
                                .MaxValue(maximum_dimension)
                                .MinSliderValue(minimum_dimension)
                                .MaxSliderValue(maximum_slider_dimension)
                                .Delta(1.0f)
                                .OnValueChanged_Lambda(
                                    [this](float const value) { box_dimensions_.Z = value; })]] +
             SVerticalBox::Slot().AutoHeight()
                 [SNew(SButton)
                      .Text(NSLOCTEXT("SbxMeshGenLab", "GenerateBox", "Generate Box"))
                      .ToolTipText(NSLOCTEXT("SbxMeshGenLab",
                                             "GenerateBoxTooltip",
                                             "Create or replace SM_GeneratedBox in the plugin's "
                                             "generated-content directory."))
                      .OnClicked_UObject(this, &ThisClass::generate_box)] +
             SVerticalBox::Slot().AutoHeight().Padding(0.0f, 8.0f, 0.0f, 0.0f)
                 [SAssignNew(status_text_, STextBlock)
                      .Text(
                          NSLOCTEXT("SbxMeshGenLab", "InitialStatus", "No mesh generated yet."))] +
             SVerticalBox::Slot().AutoHeight().Padding(0.0f, 12.0f, 0.0f, 0.0f)
                 [SNew(SBox)
                      .WidthOverride(static_cast<float>(thumbnail_size))
                      .HeightOverride(static_cast<float>(thumbnail_size))[preview_widget]]];
}

void USbxMeshGenLabWidget::ReleaseSlateResources(bool const release_children) {
    Super::ReleaseSlateResources(release_children);
    status_text_.Reset();
    thumbnail_.Reset();
    thumbnail_pool_.Reset();
}

auto USbxMeshGenLabWidget::generate_box() -> FReply {
    auto const mesh_data{SandboxMesh::generate_box(FSbxBoxParameters{box_dimensions_})};
    set_preview_mesh(nullptr);

    auto* const static_mesh{
        SandboxMesh::write_generated_static_mesh_asset(mesh_data, generated_box_asset_name)};
    if (static_mesh == nullptr) {
        status_text_->SetText(NSLOCTEXT(
            "SbxMeshGenLab", "GenerationFailed", "Box generation failed; see Output Log."));
        return FReply::Handled();
    }

    status_text_->SetText(
        FText::Format(NSLOCTEXT("SbxMeshGenLab", "GenerationSucceeded", "Generated {0}."),
                      FText::FromString(static_mesh->GetPathName())));
    set_preview_mesh(static_mesh);
    return FReply::Handled();
}

void USbxMeshGenLabWidget::set_preview_mesh(UStaticMesh* const static_mesh) {
    if (!thumbnail_.IsValid()) {
        return;
    }

    if (static_mesh == nullptr) {
        thumbnail_->SetAsset(FAssetData{});
        return;
    }

    thumbnail_->SetAsset(static_mesh);
    thumbnail_->RefreshThumbnail();
}
