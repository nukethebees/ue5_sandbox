#pragma once

#include "SbxMeshGenLab/MeshAssembly.h"
#include "SbxMeshGenLab/MeshAssemblyRecipe.h"
#include "SbxMeshGenLab/MeshGenerationRequest.h"
#include "UObject/Object.h"

#include "SbxMeshGenLabSettings.generated.h"

UENUM()
enum class ESbxMeshSelectionPivot : uint8 {
    SelectionCenter UMETA(DisplayName = "Selection Center"),
    PrimaryPart UMETA(DisplayName = "Primary Part"),
};

UCLASS(Transient)
class SBXMESHGENLAB_API USbxMeshGenLabSettings final : public UObject {
    GENERATED_BODY()
  public:
    UPROPERTY(EditAnywhere, Category = "Recipe")
    FName recipe_name{TEXT("SMR_NewAssembly")};

    UPROPERTY(EditAnywhere, Category = "Recipe")
    TSoftObjectPtr<USbxMeshAssemblyRecipe> recipe;

    UPROPERTY(EditAnywhere, Category = "Output")
    ESbxMeshShape shape{ESbxMeshShape::Box};

    UPROPERTY(EditAnywhere, Category = "Output")
    FName asset_name{TEXT("SM_GeneratedBox")};

    UPROPERTY(EditAnywhere, Category = "Part")
    ESbxMeshMaterialRole material_role{ESbxMeshMaterialRole::Structure};

    UPROPERTY(EditAnywhere, Category = "Selection")
    ESbxMeshSelectionPivot selection_pivot{ESbxMeshSelectionPivot::SelectionCenter};

    UPROPERTY(EditAnywhere, Category = "Selected Group Connectors")
    TArray<FSbxMeshAssemblyConnector> group_connectors;

    UPROPERTY(EditAnywhere,
              Category = "Selected Group Connectors",
              meta = (ClampMin = "0", ArrayClamp = "group_connectors"))
    int32 active_connector_index{};

    UPROPERTY(EditAnywhere, Category = "Selected Group Connectors")
    bool connectors_face_to_face{true};

    UPROPERTY(EditAnywhere, Category = "Duplicate Selected")
    FVector duplicate_translation_step{25.0, 0.0, 0.0};

    UPROPERTY(EditAnywhere, Category = "Duplicate Selected")
    FRotator duplicate_rotation_step{FRotator::ZeroRotator};

    UPROPERTY(EditAnywhere,
              Category = "Duplicate Selected",
              meta = (ClampMin = "1", ClampMax = "64"))
    int32 duplicate_repeat_count{1};

    UPROPERTY(EditAnywhere, Category = "Part Transform")
    FVector part_translation{FVector::ZeroVector};

    UPROPERTY(EditAnywhere, Category = "Part Transform")
    FRotator part_rotation{FRotator::ZeroRotator};

    UPROPERTY(EditAnywhere, Category = "Part Transform", meta = (ClampMin = "0.001"))
    FVector part_scale{FVector::OneVector};

    UPROPERTY(EditAnywhere,
              Category = "Box",
              meta = (EditCondition = "shape == ESbxMeshShape::Box",
                      EditConditionHides,
                      ClampMin = "0.001"))
    FVector box_dimensions{100.0, 100.0, 100.0};

    UPROPERTY(EditAnywhere,
              Category = "Beveled Box",
              meta = (EditCondition = "shape == ESbxMeshShape::BeveledBox",
                      EditConditionHides,
                      ClampMin = "0.001"))
    FVector beveled_box_dimensions{100.0, 100.0, 100.0};

    UPROPERTY(EditAnywhere,
              Category = "Beveled Box",
              meta = (EditCondition = "shape == ESbxMeshShape::BeveledBox",
                      EditConditionHides,
                      ClampMin = "0.001"))
    float beveled_box_bevel_width{10.0f};

    UPROPERTY(EditAnywhere,
              Category = "Wedge",
              meta = (EditCondition = "shape == ESbxMeshShape::Wedge",
                      EditConditionHides,
                      ClampMin = "0.001"))
    FVector wedge_dimensions{100.0, 100.0, 50.0};

    UPROPERTY(EditAnywhere,
              Category = "Wedge",
              meta = (EditCondition = "shape == ESbxMeshShape::Wedge",
                      EditConditionHides,
                      ClampMin = "0.001"))
    float wedge_top_length{50.0f};

    UPROPERTY(EditAnywhere,
              Category = "Wedge",
              meta = (EditCondition = "shape == ESbxMeshShape::Wedge", EditConditionHides))
    float wedge_top_offset{};

    UPROPERTY(EditAnywhere,
              Category = "Cylinder",
              meta = (EditCondition = "shape == ESbxMeshShape::Cylinder",
                      EditConditionHides,
                      ClampMin = "0.001"))
    float cylinder_radius{50.0f};

    UPROPERTY(EditAnywhere,
              Category = "Cylinder",
              meta = (EditCondition = "shape == ESbxMeshShape::Cylinder",
                      EditConditionHides,
                      ClampMin = "0.001"))
    float cylinder_height{100.0f};

    UPROPERTY(EditAnywhere,
              Category = "Cylinder",
              meta = (EditCondition = "shape == ESbxMeshShape::Cylinder",
                      EditConditionHides,
                      ClampMin = "3",
                      ClampMax = "256"))
    int32 cylinder_radial_segments{32};

    UPROPERTY(EditAnywhere,
              Category = "Sphere",
              meta = (EditCondition = "shape == ESbxMeshShape::Sphere",
                      EditConditionHides,
                      ClampMin = "0.001"))
    float sphere_radius{50.0f};

    UPROPERTY(EditAnywhere,
              Category = "Sphere",
              meta = (EditCondition = "shape == ESbxMeshShape::Sphere",
                      EditConditionHides,
                      ClampMin = "3",
                      ClampMax = "256"))
    int32 sphere_longitude_segments{32};

    UPROPERTY(EditAnywhere,
              Category = "Sphere",
              meta = (EditCondition = "shape == ESbxMeshShape::Sphere",
                      EditConditionHides,
                      ClampMin = "2",
                      ClampMax = "256"))
    int32 sphere_latitude_segments{16};

    UPROPERTY(EditAnywhere,
              Category = "Cone",
              meta = (EditCondition = "shape == ESbxMeshShape::Cone",
                      EditConditionHides,
                      ClampMin = "0.001"))
    float cone_radius{50.0f};

    UPROPERTY(EditAnywhere,
              Category = "Cone",
              meta = (EditCondition = "shape == ESbxMeshShape::Cone",
                      EditConditionHides,
                      ClampMin = "0.001"))
    float cone_height{100.0f};

    UPROPERTY(EditAnywhere,
              Category = "Cone",
              meta = (EditCondition = "shape == ESbxMeshShape::Cone",
                      EditConditionHides,
                      ClampMin = "3",
                      ClampMax = "256"))
    int32 cone_radial_segments{32};

    UPROPERTY(EditAnywhere,
              Category = "Hex Tile",
              meta = (EditCondition = "shape == ESbxMeshShape::HexTile",
                      EditConditionHides,
                      ClampMin = "0.001"))
    float hex_tile_outer_radius{50.0f};

    UPROPERTY(EditAnywhere,
              Category = "Hex Tile",
              meta = (EditCondition = "shape == ESbxMeshShape::HexTile",
                      EditConditionHides,
                      ClampMin = "0.001"))
    float hex_tile_depth{20.0f};

    UPROPERTY(EditAnywhere,
              Category = "Hex Tile",
              meta = (EditCondition = "shape == ESbxMeshShape::HexTile",
                      EditConditionHides,
                      ClampMin = "0.001"))
    float hex_tile_bevel_width{5.0f};

    UPROPERTY(EditAnywhere,
              Category = "Hex Tile",
              meta = (EditCondition = "shape == ESbxMeshShape::HexTile", EditConditionHides))
    bool hex_tile_pointy_top{false};

    UPROPERTY(EditAnywhere,
              Category = "Hex Frame",
              meta = (EditCondition = "shape == ESbxMeshShape::HexFrame",
                      EditConditionHides,
                      ClampMin = "0.001"))
    float hex_frame_outer_radius{50.0f};

    UPROPERTY(EditAnywhere,
              Category = "Hex Frame",
              meta = (EditCondition = "shape == ESbxMeshShape::HexFrame",
                      EditConditionHides,
                      ClampMin = "0.001"))
    float hex_frame_wall_thickness{10.0f};

    UPROPERTY(EditAnywhere,
              Category = "Hex Frame",
              meta = (EditCondition = "shape == ESbxMeshShape::HexFrame",
                      EditConditionHides,
                      ClampMin = "0.001"))
    float hex_frame_depth{20.0f};

    UPROPERTY(EditAnywhere,
              Category = "Hex Frame",
              meta = (EditCondition = "shape == ESbxMeshShape::HexFrame", EditConditionHides))
    bool hex_frame_pointy_top{false};

    UPROPERTY(EditAnywhere,
              Category = "Honeycomb Panel",
              meta = (EditCondition = "shape == ESbxMeshShape::HoneycombPanel",
                      EditConditionHides,
                      ClampMin = "1",
                      ClampMax = "64"))
    int32 honeycomb_rows{3};

    UPROPERTY(EditAnywhere,
              Category = "Honeycomb Panel",
              meta = (EditCondition = "shape == ESbxMeshShape::HoneycombPanel",
                      EditConditionHides,
                      ClampMin = "1",
                      ClampMax = "64"))
    int32 honeycomb_columns{4};

    UPROPERTY(EditAnywhere,
              Category = "Honeycomb Panel",
              meta = (EditCondition = "shape == ESbxMeshShape::HoneycombPanel",
                      EditConditionHides,
                      ClampMin = "0.001"))
    float honeycomb_cell_radius{50.0f};

    UPROPERTY(EditAnywhere,
              Category = "Honeycomb Panel",
              meta = (EditCondition = "shape == ESbxMeshShape::HoneycombPanel",
                      EditConditionHides,
                      ClampMin = "0.001"))
    float honeycomb_wall_thickness{8.0f};

    UPROPERTY(EditAnywhere,
              Category = "Honeycomb Panel",
              meta = (EditCondition = "shape == ESbxMeshShape::HoneycombPanel",
                      EditConditionHides,
                      ClampMin = "0.001"))
    float honeycomb_depth{20.0f};

    UPROPERTY(EditAnywhere,
              Category = "Honeycomb Panel",
              meta = (EditCondition = "shape == ESbxMeshShape::HoneycombPanel", EditConditionHides))
    bool honeycomb_pointy_top{false};

    [[nodiscard]] auto to_request() const -> FSbxMeshGenerationRequest;
    [[nodiscard]] auto to_transform() const -> FSbxMeshTransform;
    void load_request(FSbxMeshGenerationRequest const& request);
    void load_transform(FSbxMeshTransform const& transform);
};
