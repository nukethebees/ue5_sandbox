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
    Cone,
    HexFrame,
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
    float cone_radius_{50.0f};
    float cone_height_{100.0f};
    int32 cone_radial_segments_{32};
    float hex_frame_outer_radius_{50.0f};
    float hex_frame_wall_thickness_{10.0f};
    float hex_frame_depth_{20.0f};
    bool hex_frame_pointy_top_{false};
    TSharedPtr<FAssetThumbnailPool> thumbnail_pool_;
    TSharedPtr<FAssetThumbnail> thumbnail_;
    TSharedPtr<STextBlock> status_text_;
};
