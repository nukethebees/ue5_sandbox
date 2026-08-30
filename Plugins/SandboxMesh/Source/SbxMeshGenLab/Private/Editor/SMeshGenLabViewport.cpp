#include "Editor/SMeshGenLabViewport.h"

#include "Components/StaticMeshComponent.h"
#include "EditorViewportClient.h"
#include "Engine/StaticMesh.h"
#include "InputCoreTypes.h"
#include "PreviewScene.h"
#include "Slate/SceneViewport.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/SBoxPanel.h"

namespace {

FRotator const default_light_rotation{-35.0f, -45.0f, 0.0f};
constexpr float light_rotation_sensitivity{0.2f};

} // namespace

class FMeshGenLabViewportClient final : public FEditorViewportClient {
  public:
    FMeshGenLabViewportClient(FPreviewScene& preview_scene,
                              TSharedRef<SMeshGenLabViewport> const& viewport)
        : FEditorViewportClient{nullptr,
                                &preview_scene,
                                StaticCastSharedRef<SEditorViewport>(viewport)}
        , preview_scene_{preview_scene} {
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

    void set_lit(bool const lit) {
        lit_ = lit;
        SetViewMode(lit_ ? VMI_Lit : VMI_Unlit);
        Invalidate();
    }

    auto is_lit() const -> bool { return lit_; }

    void reset_light() {
        light_rotation_ = default_light_rotation;
        preview_scene_.SetLightDirection(light_rotation_);
        Invalidate();
    }

    auto InputAxis(FInputKeyEventArgs const& arguments) -> bool override {
        auto* const viewport{arguments.Viewport};
        auto const shift_down{viewport != nullptr && (viewport->KeyState(EKeys::LeftShift) ||
                                                      viewport->KeyState(EKeys::RightShift))};
        auto const left_mouse_down{viewport != nullptr &&
                                   viewport->KeyState(EKeys::LeftMouseButton)};
        auto const is_mouse_axis{arguments.Key == EKeys::MouseX || arguments.Key == EKeys::MouseY};

        if (shift_down && left_mouse_down && is_mouse_axis) {
            if (arguments.Key == EKeys::MouseX) {
                light_rotation_.Yaw += arguments.AmountDepressed * light_rotation_sensitivity;
                light_rotation_.Yaw = FRotator::NormalizeAxis(light_rotation_.Yaw);
            } else {
                light_rotation_.Pitch = FMath::Clamp(
                    light_rotation_.Pitch - arguments.AmountDepressed * light_rotation_sensitivity,
                    -89.0f,
                    89.0f);
            }

            preview_scene_.SetLightDirection(light_rotation_);
            Invalidate();
            return true;
        }

        return FEditorViewportClient::InputAxis(arguments);
    }

    auto ShouldOrbitCamera() const -> bool override { return true; }
    auto CanSetWidgetMode(UE::Widget::EWidgetMode) const -> bool override { return false; }
    auto CanCycleWidgetMode() const -> bool override { return false; }
    auto GetBackgroundColor() const -> FLinearColor override {
        return FLinearColor{0.025f, 0.025f, 0.025f};
    }
  private:
    FPreviewScene& preview_scene_;
    FRotator light_rotation_{default_light_rotation};
    bool lit_{true};
};

void SMeshGenLabViewport::Construct(FArguments const&) {
    preview_scene_ = MakeShared<FPreviewScene>(FPreviewScene::ConstructionValues{}
                                                   .SetCreateDefaultLighting(true)
                                                   .SetLightRotation(default_light_rotation)
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
    return SNew(SHorizontalBox) +
           SHorizontalBox::Slot().AutoWidth().Padding(
               4.0f,
               2.0f,
               2.0f,
               2.0f)[SNew(SButton)
                         .Text_Lambda([this] {
                             auto const lit{viewport_client_.IsValid() &&
                                            viewport_client_->is_lit()};
                             return lit ? NSLOCTEXT("SbxMeshGenLab", "LitMode", "View: Lit")
                                        : NSLOCTEXT("SbxMeshGenLab", "UnlitMode", "View: Unlit");
                         })
                         .ToolTipText(NSLOCTEXT("SbxMeshGenLab",
                                                "LightingModeTooltip",
                                                "Switch between lit and unlit preview rendering."))
                         .OnClicked_Lambda([this] {
                             if (viewport_client_.IsValid()) {
                                 viewport_client_->set_lit(!viewport_client_->is_lit());
                             }
                             return FReply::Handled();
                         })] +
           SHorizontalBox::Slot().AutoWidth().Padding(
               2.0f,
               2.0f,
               4.0f,
               2.0f)[SNew(SButton)
                         .Text(NSLOCTEXT("SbxMeshGenLab", "ResetLight", "Reset Light"))
                         .ToolTipText(NSLOCTEXT("SbxMeshGenLab",
                                                "ResetLightTooltip",
                                                "Restore the default preview-light direction."))
                         .OnClicked_Lambda([this] {
                             if (viewport_client_.IsValid()) {
                                 viewport_client_->reset_light();
                             }
                             return FReply::Handled();
                         })];
}

void SMeshGenLabViewport::OnFocusViewportToSelection() {
    focus_mesh();
}
