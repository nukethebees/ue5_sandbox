#pragma once

#include "EditorUtilityWidget.h"
#include "Input/Reply.h"
#include "SbxMeshGenLab/MeshGenerationRequest.h"

#include "SbxMeshGenLabWidget.generated.h"

class FAssetThumbnail;
class FAssetThumbnailPool;
class IDetailsView;
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
    auto save_generated_mesh() -> FReply;
    void update_preview();
    void set_preview_mesh(UStaticMesh* static_mesh);

    UPROPERTY(Transient)
    TObjectPtr<USbxMeshGenLabSettings> settings_;

    UPROPERTY(Transient)
    TObjectPtr<UStaticMesh> preview_mesh_;

    ESbxMeshShape last_shape_{ESbxMeshShape::Box};
    TSharedPtr<IDetailsView> details_view_;
    TSharedPtr<FAssetThumbnailPool> thumbnail_pool_;
    TSharedPtr<FAssetThumbnail> thumbnail_;
    TSharedPtr<STextBlock> status_text_;
};
