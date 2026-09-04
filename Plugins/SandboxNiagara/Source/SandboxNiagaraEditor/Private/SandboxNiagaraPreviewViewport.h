#pragma once

#include "SEditorViewport.h"
#include "UObject/GCObject.h"

class FAdvancedPreviewScene;
class FSandboxNiagaraPreviewViewportClient;
class UNiagaraComponent;
class UNiagaraSystem;

class SSandboxNiagaraPreviewViewport final : public SEditorViewport, public FGCObject {
  public:
    SLATE_BEGIN_ARGS(SSandboxNiagaraPreviewViewport) {}
    SLATE_END_ARGS()

    void Construct(FArguments const& arguments);
    ~SSandboxNiagaraPreviewViewport() override;

    void set_system(UNiagaraSystem* system);
    void restart();
    void frame_system();
    void toggle_paused();
    auto is_paused() const -> bool;

    void AddReferencedObjects(FReferenceCollector& collector) override;
    auto GetReferencerName() const -> FString override;

  protected:
    auto MakeEditorViewportClient() -> TSharedRef<FEditorViewportClient> override;
    void OnFocusViewportToSelection() override;

  private:
    TSharedPtr<FAdvancedPreviewScene> preview_scene_{};
    TSharedPtr<FSandboxNiagaraPreviewViewportClient> viewport_client_{};
    TObjectPtr<UNiagaraComponent> preview_component_{nullptr};
};
