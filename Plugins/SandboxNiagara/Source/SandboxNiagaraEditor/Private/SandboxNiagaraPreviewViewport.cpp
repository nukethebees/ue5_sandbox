#include "SandboxNiagaraPreviewViewport.h"

#include "NiagaraComponent.h"
#include "NiagaraSystem.h"

#include "AdvancedPreviewScene.h"
#include "EditorViewportClient.h"
#include "Engine/World.h"
#include "SceneInterface.h"

class FSandboxNiagaraPreviewViewportClient final : public FEditorViewportClient {
  public:
    FSandboxNiagaraPreviewViewportClient(
        FAdvancedPreviewScene& preview_scene,
        TSharedRef<SSandboxNiagaraPreviewViewport> const& viewport)
        : FEditorViewportClient(
              nullptr, &preview_scene, StaticCastSharedRef<SEditorViewport>(viewport)) {
        bUsesDrawHelper = true;
        DrawHelper.bDrawPivot = false;
        DrawHelper.bDrawWorldBox = false;
        DrawHelper.bDrawKillZ = false;
        DrawHelper.bDrawGrid = true;
        DrawHelper.GridColorAxis = FColor{80, 80, 80};
        DrawHelper.GridColorMajor = FColor{72, 72, 72};
        DrawHelper.GridColorMinor = FColor{64, 64, 64};
        DrawHelper.PerspectiveGridSize = static_cast<float>(HALF_WORLD_MAX1);

        SetViewportType(ELevelViewportType::LVT_Perspective);
        SetViewMode(VMI_Lit);
        SetViewLocation(FVector{-1200.0, -1200.0, 900.0});
        SetViewRotation(FRotator{-25.0, 45.0, 0.0});
        SetRealtime(true);
        SetIsSimulateInEditorViewport(true);
        SetAllowCinematicControl(false);
        bSetListenerPosition = false;
        EngineShowFlags.SetSnap(false);
        EngineShowFlags.SetSeparateTranslucency(true);
        OverrideNearClipPlane(1.0f);
    }

    void Tick(float const delta_seconds) override {
        FEditorViewportClient::Tick(delta_seconds);
        if (!paused_ && !GIntraFrameDebuggingGameThread && PreviewScene != nullptr &&
            PreviewScene->GetWorld() != nullptr) {
            PreviewScene->GetWorld()->Tick(LEVELTICK_All, delta_seconds);
        }
    }

    void toggle_paused() {
        paused_ = !paused_;
    }

    auto is_paused() const -> bool {
        return paused_;
    }

  private:
    bool paused_{false};
};

void SSandboxNiagaraPreviewViewport::Construct(FArguments const& arguments) {
    preview_scene_ = MakeShared<FAdvancedPreviewScene>(FPreviewScene::ConstructionValues());
    preview_scene_->SetFloorVisibility(false);
    preview_scene_->SetLightDirection(FRotator{-40.0, 128.0, 0.0});

    SEditorViewport::Construct(SEditorViewport::FArguments{});

    auto* const preview_world{preview_scene_->GetWorld()};
    if (preview_world != nullptr && GWorld != nullptr && GWorld->Scene != nullptr) {
        preview_world->ShaderPlatformChanged(GWorld->Scene->GetShaderPlatform());
    }
}

SSandboxNiagaraPreviewViewport::~SSandboxNiagaraPreviewViewport() {
    if (preview_component_ != nullptr) {
        preview_component_->DeactivateImmediate();
        if (preview_scene_.IsValid()) {
            preview_scene_->RemoveComponent(preview_component_);
        }
        preview_component_->DestroyComponent();
        preview_component_ = nullptr;
    }
}

void SSandboxNiagaraPreviewViewport::set_system(UNiagaraSystem* const system) {
    if (preview_component_ != nullptr) {
        preview_component_->DeactivateImmediate();
        preview_scene_->RemoveComponent(preview_component_);
        preview_component_->DestroyComponent();
        preview_component_ = nullptr;
    }

    if (!IsValid(system)) {
        if (viewport_client_.IsValid()) {
            viewport_client_->Invalidate();
        }
        return;
    }

    preview_component_ =
        NewObject<UNiagaraComponent>(GetTransientPackage(), NAME_None, RF_Transient);
    preview_component_->CastShadow = true;
    preview_component_->bCastDynamicShadow = true;
    preview_component_->SetAllowScalability(false);
    preview_component_->SetAsset(system);
    preview_component_->SetForceSolo(true);
    preview_component_->SetAgeUpdateMode(ENiagaraAgeUpdateMode::TickDeltaTime);
    preview_component_->SetCanRenderWhileSeeking(true);
    preview_component_->Activate(true);
    preview_scene_->AddComponent(preview_component_, FTransform::Identity);

    frame_system();
    viewport_client_->Invalidate();
}

void SSandboxNiagaraPreviewViewport::restart() {
    if (preview_component_ != nullptr) {
        preview_component_->ReinitializeSystem();
        viewport_client_->Invalidate();
    }
}

void SSandboxNiagaraPreviewViewport::frame_system() {
    if (!viewport_client_.IsValid()) {
        return;
    }

    auto bounds{preview_component_ != nullptr ? preview_component_->Bounds.GetBox()
                                              : FBox{ForceInit}};
    if (!bounds.IsValid || bounds.GetExtent().IsNearlyZero()) {
        bounds = FBox{FVector{-750.0}, FVector{750.0}};
    }
    viewport_client_->FocusViewportOnBox(bounds, true);
    viewport_client_->Invalidate();
}

void SSandboxNiagaraPreviewViewport::toggle_paused() {
    if (viewport_client_.IsValid()) {
        viewport_client_->toggle_paused();
    }
}

auto SSandboxNiagaraPreviewViewport::is_paused() const -> bool {
    return viewport_client_.IsValid() && viewport_client_->is_paused();
}

void SSandboxNiagaraPreviewViewport::AddReferencedObjects(FReferenceCollector& collector) {
    collector.AddReferencedObject(preview_component_);
}

auto SSandboxNiagaraPreviewViewport::GetReferencerName() const -> FString {
    return TEXT("SSandboxNiagaraPreviewViewport");
}

auto SSandboxNiagaraPreviewViewport::MakeEditorViewportClient()
    -> TSharedRef<FEditorViewportClient> {
    viewport_client_ = MakeShared<FSandboxNiagaraPreviewViewportClient>(
        *preview_scene_, SharedThis(this));
    viewport_client_->VisibilityDelegate.BindSP(
        this, &SSandboxNiagaraPreviewViewport::IsVisible);
    return viewport_client_.ToSharedRef();
}

void SSandboxNiagaraPreviewViewport::OnFocusViewportToSelection() {
    frame_system();
}
