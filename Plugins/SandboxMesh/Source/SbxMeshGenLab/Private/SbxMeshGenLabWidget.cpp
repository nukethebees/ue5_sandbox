#include "SbxMeshGenLab/SbxMeshGenLabWidget.h"

#include "Generation/MeshAssetWriter.h"

#include "AssetThumbnail.h"
#include "Engine/StaticMesh.h"
#include "Styling/AppStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

namespace {
FString const generated_cube_object_path{
    TEXT("/SandboxMesh/MeshGenLab/Generated/SM_GeneratedCube.SM_GeneratedCube")};
constexpr uint32 thumbnail_size{256};
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
    set_preview_mesh(LoadObject<UStaticMesh>(nullptr, *generated_cube_object_path));

    return SNew(SBorder)
        .BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
        .Padding(
            12.0f)[SNew(SVerticalBox) +
                   SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 4.0f)
                       [SNew(STextBlock)
                            .Text(NSLOCTEXT("SbxMeshGenLab", "Title", "Mesh Generation Lab"))
                            .Font(FAppStyle::Get().GetFontStyle("HeadingExtraSmall"))] +
                   SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 10.0f)
                       [SNew(STextBlock)
                            .Text(NSLOCTEXT("SbxMeshGenLab",
                                            "Description",
                                            "Generate a disposable procedural static mesh."))] +
                   SVerticalBox::Slot().AutoHeight()
                       [SNew(SButton)
                            .Text(NSLOCTEXT("SbxMeshGenLab", "GenerateCube", "Generate Cube"))
                            .ToolTipText(NSLOCTEXT("SbxMeshGenLab",
                                                   "GenerateCubeTooltip",
                                                   "Create or replace SM_GeneratedCube in the "
                                                   "plugin's generated-content directory."))
                            .OnClicked_UObject(this, &ThisClass::generate_cube)] +
                   SVerticalBox::Slot().AutoHeight().Padding(
                       0.0f, 8.0f, 0.0f, 0.0f)[SAssignNew(status_text_, STextBlock)
                                                   .Text(NSLOCTEXT("SbxMeshGenLab",
                                                                   "InitialStatus",
                                                                   "No mesh generated yet."))] +
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

auto USbxMeshGenLabWidget::generate_cube() -> FReply {
    auto* const static_mesh{SandboxMesh::generate_cube_asset()};
    if (static_mesh == nullptr) {
        status_text_->SetText(NSLOCTEXT(
            "SbxMeshGenLab", "GenerationFailed", "Cube generation failed; see Output Log."));
        return FReply::Handled();
    }

    status_text_->SetText(
        FText::Format(NSLOCTEXT("SbxMeshGenLab", "GenerationSucceeded", "Generated {0}."),
                      FText::FromString(static_mesh->GetPathName())));
    set_preview_mesh(static_mesh);
    return FReply::Handled();
}

void USbxMeshGenLabWidget::set_preview_mesh(UStaticMesh* const static_mesh) {
    if (static_mesh == nullptr || !thumbnail_.IsValid()) {
        return;
    }

    thumbnail_->SetAsset(static_mesh);
    thumbnail_->RefreshThumbnail();
}
