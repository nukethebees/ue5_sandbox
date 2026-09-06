#pragma once

#include "SbxMeshGenLab/MeshAssembly.h"
#include "UObject/Object.h"

#include "MeshAssemblyRecipe.generated.h"

USTRUCT()
struct SBXMESHGENLAB_API FSbxMeshAssemblyRecipePart {
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, Category = "Part")
    FGuid id;

    UPROPERTY(VisibleAnywhere, Category = "Part")
    FGuid parent_id;

    UPROPERTY(EditAnywhere, Category = "Part")
    ESbxMeshShape shape{ESbxMeshShape::Box};

    UPROPERTY(EditAnywhere, Category = "Part")
    FVector translation{FVector::ZeroVector};

    UPROPERTY(EditAnywhere, Category = "Part")
    FRotator rotation{FRotator::ZeroRotator};

    UPROPERTY(EditAnywhere, Category = "Part")
    FVector scale{FVector::OneVector};

    UPROPERTY(EditAnywhere, Category = "Box")
    FVector box_dimensions{100.0, 100.0, 100.0};

    UPROPERTY(EditAnywhere, Category = "Beveled Box")
    FVector beveled_box_dimensions{100.0, 100.0, 100.0};

    UPROPERTY(EditAnywhere, Category = "Beveled Box")
    float beveled_box_bevel_width{10.0f};

    UPROPERTY(EditAnywhere, Category = "Wedge")
    FVector wedge_dimensions{100.0, 100.0, 50.0};

    UPROPERTY(EditAnywhere, Category = "Wedge")
    float wedge_top_length{50.0f};

    UPROPERTY(EditAnywhere, Category = "Wedge")
    float wedge_top_offset{};

    UPROPERTY(EditAnywhere, Category = "Cylinder")
    float cylinder_radius{50.0f};

    UPROPERTY(EditAnywhere, Category = "Cylinder")
    float cylinder_height{100.0f};

    UPROPERTY(EditAnywhere, Category = "Cylinder")
    int32 cylinder_radial_segments{32};

    UPROPERTY(EditAnywhere, Category = "Sphere")
    float sphere_radius{50.0f};

    UPROPERTY(EditAnywhere, Category = "Sphere")
    int32 sphere_longitude_segments{32};

    UPROPERTY(EditAnywhere, Category = "Sphere")
    int32 sphere_latitude_segments{16};

    UPROPERTY(EditAnywhere, Category = "Cone")
    float cone_radius{50.0f};

    UPROPERTY(EditAnywhere, Category = "Cone")
    float cone_height{100.0f};

    UPROPERTY(EditAnywhere, Category = "Cone")
    int32 cone_radial_segments{32};

    UPROPERTY(EditAnywhere, Category = "Hex Tile")
    float hex_tile_outer_radius{50.0f};

    UPROPERTY(EditAnywhere, Category = "Hex Tile")
    float hex_tile_depth{20.0f};

    UPROPERTY(EditAnywhere, Category = "Hex Tile")
    float hex_tile_bevel_width{5.0f};

    UPROPERTY(EditAnywhere, Category = "Hex Tile")
    bool hex_tile_pointy_top{};

    UPROPERTY(EditAnywhere, Category = "Hex Frame")
    float hex_frame_outer_radius{50.0f};

    UPROPERTY(EditAnywhere, Category = "Hex Frame")
    float hex_frame_wall_thickness{10.0f};

    UPROPERTY(EditAnywhere, Category = "Hex Frame")
    float hex_frame_depth{20.0f};

    UPROPERTY(EditAnywhere, Category = "Hex Frame")
    bool hex_frame_pointy_top{};

    UPROPERTY(EditAnywhere, Category = "Honeycomb Panel")
    int32 honeycomb_rows{3};

    UPROPERTY(EditAnywhere, Category = "Honeycomb Panel")
    int32 honeycomb_columns{4};

    UPROPERTY(EditAnywhere, Category = "Honeycomb Panel")
    float honeycomb_cell_radius{50.0f};

    UPROPERTY(EditAnywhere, Category = "Honeycomb Panel")
    float honeycomb_wall_thickness{8.0f};

    UPROPERTY(EditAnywhere, Category = "Honeycomb Panel")
    float honeycomb_depth{20.0f};

    UPROPERTY(EditAnywhere, Category = "Honeycomb Panel")
    bool honeycomb_pointy_top{};

    [[nodiscard]] static auto from_part(FSbxMeshAssemblyPart const& part,
                                        FGuid id = FGuid::NewGuid(),
                                        FGuid parent_id = {}) -> FSbxMeshAssemblyRecipePart;
    [[nodiscard]] auto to_part(FName output_asset_name) const -> FSbxMeshAssemblyPart;
};

USTRUCT()
struct SBXMESHGENLAB_API FSbxMeshAssemblyConnector {
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, Category = "Connector")
    FName name{TEXT("Connector")};

    UPROPERTY(EditAnywhere, Category = "Connector")
    FVector translation{FVector::ZeroVector};

    UPROPERTY(EditAnywhere, Category = "Connector")
    FRotator rotation{FRotator::ZeroRotator};

    [[nodiscard]] auto to_transform() const -> FTransform;
};

USTRUCT()
struct SBXMESHGENLAB_API FSbxMeshAssemblyRecipeGroup {
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, Category = "Group")
    FGuid id;

    UPROPERTY(VisibleAnywhere, Category = "Group")
    FGuid parent_id;

    UPROPERTY(EditAnywhere, Category = "Group")
    FName name{TEXT("Group")};

    UPROPERTY(EditAnywhere, Category = "Transform")
    FVector translation{FVector::ZeroVector};

    UPROPERTY(EditAnywhere, Category = "Transform")
    FRotator rotation{FRotator::ZeroRotator};

    UPROPERTY(EditAnywhere, Category = "Transform", meta = (ClampMin = "0.001"))
    FVector scale{FVector::OneVector};

    UPROPERTY(EditAnywhere, Category = "Connectors")
    TArray<FSbxMeshAssemblyConnector> connectors;

    [[nodiscard]] auto to_transform() const -> FTransform;
    void set_transform(FTransform const& transform);
};

namespace SandboxMesh {

[[nodiscard]] SBXMESHGENLAB_API auto
    is_legacy_mesh_assembly_recipe(TArray<FSbxMeshAssemblyRecipePart> const& parts,
                                   TArray<FSbxMeshAssemblyRecipeGroup> const& groups,
                                   int32 format_version) -> bool;
[[nodiscard]] SBXMESHGENLAB_API auto
    validate_mesh_assembly_hierarchy(TArray<FSbxMeshAssemblyRecipePart> const& parts,
                                     TArray<FSbxMeshAssemblyRecipeGroup> const& groups) -> FString;
[[nodiscard]] SBXMESHGENLAB_API auto
    resolve_mesh_assembly_hierarchy(TArray<FSbxMeshAssemblyRecipePart> const& parts,
                                    TArray<FSbxMeshAssemblyRecipeGroup> const& groups,
                                    FName output_asset_name) -> TArray<FSbxMeshAssemblyPart>;

}

UCLASS(BlueprintType)
class SBXMESHGENLAB_API USbxMeshAssemblyRecipe final : public UObject {
    GENERATED_BODY()
  public:
    UPROPERTY(VisibleAnywhere, Category = "Recipe")
    int32 format_version{3};

    UPROPERTY(EditAnywhere, Category = "Recipe")
    FName output_asset_name{TEXT("SM_GeneratedAssembly")};

    UPROPERTY(EditAnywhere, Category = "Recipe")
    TArray<FSbxMeshAssemblyRecipePart> parts;

    UPROPERTY(EditAnywhere, Category = "Recipe")
    TArray<FSbxMeshAssemblyRecipeGroup> groups;

    void set_hierarchy(FName asset_name,
                       TArray<FSbxMeshAssemblyRecipePart> const& assembly_parts,
                       TArray<FSbxMeshAssemblyRecipeGroup> const& assembly_groups);
    [[nodiscard]] auto to_assembly() const -> TArray<FSbxMeshAssemblyPart>;
};
