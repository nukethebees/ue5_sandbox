#include "Editor/SMeshGenLabViewport.h"

#include "Components/StaticMeshComponent.h"
#include "EditorViewportClient.h"
#include "Engine/StaticMesh.h"
#include "PreviewScene.h"
#include "Slate/SceneViewport.h"

class FMeshGenLabViewportClient final : public FEditorViewportClient {
  public:
    FMeshGenLabViewportClient(FPreviewScene& preview_scene,
                              TSharedRef<SMeshGenLabViewport> const& viewport)
        : FEditorViewportClient{
              nullptr, &preview_scene, StaticCastSharedRef<SEditorViewport>(viewport)} {
        DrawHelper.bDrawPivot = false;
        DrawHelper.bDrawWorldBox = false;
        DrawHelper.bDrawKillZ = false;
        DrawHelper.bDrawGrid = false;
        ShowWidget(false);

        SetViewMode(VMI_Lit);
        SetViewRotation(FRotator{-20.0f, -135.0f, 0.0f});
        SetViewLocationForOrbiting(FVector::ZeroVector, 200.0f);
        SetRealtime(true);
        bSetListenerPosition = false;

        EngineShowFlags.EnableAdvancedFeatures();
        EngineShowFlags.SetGrid(false);
        EngineShowFlags.SetLighting(true);
        EngineShowFlags.SetPostProcessing(true);
    }

    auto ShouldOrbitCamera() const -> bool override { return true; }
    auto CanSetWidgetMode(UE::Widget::EWidgetMode) const -> bool override { return false; }
    auto CanCycleWidgetMode() const -> bool override { return false; }
    auto GetBackgroundColor() const -> FLinearColor override {
        return FLinearColor{0.025f, 0.025f, 0.025f};
    }
};

void SMeshGenLabViewport::Construct(FArguments const&) {
    preview_scene_ = MakeShared<FPreviewScene>(FPreviewScene::ConstructionValues{}
                                                   .SetCreateDefaultLighting(true)
                                                   .SetLightRotation(FRotator{-35.0f, -45.0f, 0.0f})
                                                   .SetSkyBrightness(0.8f));
    preview_component_ =
        NewObject<UStaticMeshComponent>(GetTransientPackage(), NAME_None, RF_Transient);
    preview_component_->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    preview_component_->SetMobility(EComponentMobility::Movable);
    preview_scene_->AddComponent(preview_component_, FTransform::Identity);

    SEditorViewport::Construct(SEditorViewport::FArguments{});
}

void SMeshGenLabViewport::set_mesh(UStaticMesh* const static_mesh) {
    preview_component_->SetStaticMesh(static_mesh);
    preview_component_->UpdateBounds();
    preview_component_->MarkRenderStateDirty();
    if (SceneViewport.IsValid()) {
        SceneViewport->Invalidate();
    }
}

void SMeshGenLabViewport::focus_mesh() {
    auto* const static_mesh{preview_component_->GetStaticMesh().Get()};
    if (static_mesh == nullptr || !viewport_client_.IsValid()) {
        return;
    }

    viewport_client_->FocusViewportOnBox(static_mesh->GetBounds().GetBox(), true);
    viewport_client_->Invalidate();
}

auto SMeshGenLabViewport::MakeEditorViewportClient() -> TSharedRef<FEditorViewportClient> {
    viewport_client_ = MakeShared<FMeshGenLabViewportClient>(*preview_scene_, SharedThis(this));
    return viewport_client_.ToSharedRef();
}

auto SMeshGenLabViewport::BuildViewportToolbar() -> TSharedPtr<SWidget> {
    return nullptr;
}

void SMeshGenLabViewport::OnFocusViewportToSelection() {
    focus_mesh();
}
