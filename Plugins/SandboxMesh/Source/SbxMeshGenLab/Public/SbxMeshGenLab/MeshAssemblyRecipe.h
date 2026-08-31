#pragma once

#include "SbxMeshGenLab/MeshAssembly.h"
#include "UObject/Object.h"

#include "MeshAssemblyRecipe.generated.h"

USTRUCT()
struct SBXMESHGENLAB_API FSbxMeshAssemblyRecipePart {
    GENERATED_BODY()

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

    [[nodiscard]] static auto from_part(FSbxMeshAssemblyPart const& part)
        -> FSbxMeshAssemblyRecipePart;
    [[nodiscard]] auto to_part(FName output_asset_name) const -> FSbxMeshAssemblyPart;
};

UCLASS(BlueprintType)
class SBXMESHGENLAB_API USbxMeshAssemblyRecipe final : public UObject {
    GENERATED_BODY()
  public:
    UPROPERTY(VisibleAnywhere, Category = "Recipe")
    int32 format_version{1};

    UPROPERTY(EditAnywhere, Category = "Recipe")
    FName output_asset_name{TEXT("SM_GeneratedAssembly")};

    UPROPERTY(EditAnywhere, Category = "Recipe")
    TArray<FSbxMeshAssemblyRecipePart> parts;

    void set_assembly(FName asset_name, TArray<FSbxMeshAssemblyPart> const& assembly_parts);
    [[nodiscard]] auto to_assembly() const -> TArray<FSbxMeshAssemblyPart>;
};
