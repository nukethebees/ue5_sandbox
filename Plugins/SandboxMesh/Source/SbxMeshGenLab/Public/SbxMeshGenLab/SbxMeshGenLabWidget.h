#pragma once

#include "EditorUtilityWidget.h"
#include "Input/Reply.h"
#include "SbxMeshGenLab/MeshAssembly.h"
#include "SbxMeshGenLab/MeshGenerationRequest.h"

#include "SbxMeshGenLabWidget.generated.h"

class IDetailsView;
class SMeshGenLabViewport;
class SBox;
class STextBlock;
class UStaticMesh;
class USbxMeshGenLabSettings;
struct FPropertyChangedEvent;
template <typename ItemType>
class SListView;

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
    void select_part(TSharedPtr<FSbxMeshAssemblyPart> const& part);
    void select_shape(ESbxMeshShape shape);
    auto add_part() -> FReply;
    auto duplicate_part() -> FReply;
    auto remove_part() -> FReply;
    auto save_generated_mesh() -> FReply;
    auto focus_preview() -> FReply;
    void sync_selected_part_from_settings();
    void refresh_shape_selector();
    void refresh_parts_list();
    void update_preview();
    void set_preview_mesh(UStaticMesh* static_mesh);

    UPROPERTY(Transient)
    TObjectPtr<USbxMeshGenLabSettings> settings_;

    UPROPERTY(Transient)
    TObjectPtr<UStaticMesh> preview_mesh_;

    TArray<TSharedPtr<FSbxMeshAssemblyPart>> parts_;
    TSharedPtr<FSbxMeshAssemblyPart> selected_part_;
    ESbxMeshShape last_shape_{ESbxMeshShape::Box};
    TSharedPtr<IDetailsView> details_view_;
    TSharedPtr<SBox> shape_selector_container_;
    TSharedPtr<SListView<TSharedPtr<FSbxMeshAssemblyPart>>> parts_list_;
    TSharedPtr<SMeshGenLabViewport> preview_viewport_;
    TSharedPtr<STextBlock> status_text_;
};
