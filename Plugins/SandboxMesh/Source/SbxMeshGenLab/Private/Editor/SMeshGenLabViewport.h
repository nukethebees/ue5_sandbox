#pragma once

#include "SEditorViewport.h"

class FMeshGenLabViewportClient;
class FPreviewScene;
class UStaticMesh;
class UStaticMeshComponent;

class SMeshGenLabViewport final : public SEditorViewport {
  public:
    SLATE_BEGIN_ARGS(SMeshGenLabViewport) {}
    SLATE_END_ARGS()

    void Construct(FArguments const& arguments);

    void set_mesh(UStaticMesh* static_mesh);
    void focus_mesh();
  protected:
    auto MakeEditorViewportClient() -> TSharedRef<FEditorViewportClient> override;
    auto BuildViewportToolbar() -> TSharedPtr<SWidget> override;
    void OnFocusViewportToSelection() override;
  private:
    TSharedPtr<FPreviewScene> preview_scene_;
    TSharedPtr<FMeshGenLabViewportClient> viewport_client_;
    UStaticMeshComponent* preview_component_{};
};
