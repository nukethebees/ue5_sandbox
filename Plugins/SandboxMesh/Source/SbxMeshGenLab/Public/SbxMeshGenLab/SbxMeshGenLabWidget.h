#pragma once

#include "EditorUtilityWidget.h"
#include "Input/Reply.h"
#include "SbxMeshGenLab/MeshGenerationRequest.h"

#include "SbxMeshGenLabWidget.generated.h"

class IDetailsView;
class SMeshGenLabViewport;
class STextBlock;
class UStaticMesh;
class USbxMeshGenLabSettings;
struct FPropertyChangedEvent;

UCLASS()
class SBXMESHGENLAB_API USbxMeshGenLabWidget final : public UEditorUtilityWidget {
    GENERATED_BODY()
  public:
    USbxMeshGenLabWidget();
  protected:
    TSharedRef<SWidget> RebuildWidget() override;
    void ReleaseSlateResources(bool release_children) override;
  private:
    void on_property_changed(FPropertyChangedEvent const& event);
    void select_shape(ESbxMeshShape shape);
    auto save_generated_mesh() -> FReply;
    auto focus_preview() -> FReply;
    void update_preview();
    void set_preview_mesh(UStaticMesh* static_mesh);

    UPROPERTY(Transient)
    TObjectPtr<USbxMeshGenLabSettings> settings_;

    UPROPERTY(Transient)
    TObjectPtr<UStaticMesh> preview_mesh_;

    ESbxMeshShape last_shape_{ESbxMeshShape::Box};
    TSharedPtr<IDetailsView> details_view_;
    TSharedPtr<SMeshGenLabViewport> preview_viewport_;
    TSharedPtr<STextBlock> status_text_;
};
