#pragma once

#include "EditorUtilityWidget.h"
#include "Input/Reply.h"

#include "SbxMeshGenLabWidget.generated.h"

class STextBlock;
class FAssetThumbnail;
class FAssetThumbnailPool;
class UStaticMesh;

enum class ESbxMeshShape : uint8 {
    Box,
    Cylinder,
    Sphere,
};

UCLASS()
class SBXMESHGENLAB_API USbxMeshGenLabWidget final : public UEditorUtilityWidget {
    GENERATED_BODY()
  public:
    USbxMeshGenLabWidget();
  protected:
    TSharedRef<SWidget> RebuildWidget() override;
    void ReleaseSlateResources(bool release_children) override;
  private:
    auto generate_selected_shape() -> FReply;
    void select_shape(ESbxMeshShape shape);
    [[nodiscard]] auto generated_asset_name() const -> FName;
    [[nodiscard]] auto selected_shape_text() const -> FText;
    [[nodiscard]] auto generate_button_text() const -> FText;
    void set_preview_mesh(UStaticMesh* static_mesh);

    ESbxMeshShape selected_shape_{ESbxMeshShape::Box};
    FVector3f box_dimensions_{100.0f, 100.0f, 100.0f};
    float cylinder_radius_{50.0f};
    float cylinder_height_{100.0f};
    int32 cylinder_radial_segments_{32};
    float sphere_radius_{50.0f};
    int32 sphere_longitude_segments_{32};
    int32 sphere_latitude_segments_{16};
    TSharedPtr<FAssetThumbnailPool> thumbnail_pool_;
    TSharedPtr<FAssetThumbnail> thumbnail_;
    TSharedPtr<STextBlock> status_text_;
};
