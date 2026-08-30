#include "SbxMeshGenLab/SbxMeshGenLabWidget.h"

#include "Generation/MeshAssetWriter.h"

#include "Engine/StaticMesh.h"
#include "Styling/AppStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

USbxMeshGenLabWidget::USbxMeshGenLabWidget() {
    TabDisplayName = NSLOCTEXT("SbxMeshGenLab", "TabName", "Mesh Gen Lab");
    bAlwaysReregisterWithWindowsMenu = true;
}

auto USbxMeshGenLabWidget::RebuildWidget() -> TSharedRef<SWidget> {
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
                                                                   "No mesh generated yet."))]];
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
    return FReply::Handled();
}
