#pragma once

#include "EditorUtilityWidget.h"
#include "Input/Reply.h"

#include "SbxMeshGenLabWidget.generated.h"

class STextBlock;
class FAssetThumbnail;
class FAssetThumbnailPool;
class UStaticMesh;

UCLASS()
class SBXMESHGENLAB_API USbxMeshGenLabWidget final : public UEditorUtilityWidget {
    GENERATED_BODY()
  public:
    USbxMeshGenLabWidget();
  protected:
    TSharedRef<SWidget> RebuildWidget() override;
    void ReleaseSlateResources(bool release_children) override;
  private:
    auto generate_box() -> FReply;
    void set_preview_mesh(UStaticMesh* static_mesh);

    FVector3f box_dimensions_{100.0f, 100.0f, 100.0f};
    TSharedPtr<FAssetThumbnailPool> thumbnail_pool_;
    TSharedPtr<FAssetThumbnail> thumbnail_;
    TSharedPtr<STextBlock> status_text_;
};
