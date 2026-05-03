#pragma once

#include "TestUniformFieldSource.h"

#include "CoreMinimal.h"
#include "Containers/Array.h"
#include "GameFramework/Actor.h"
#include "HAL/Platform.h"
#include "Math/MathFwd.h"
#include "UObject/ObjectMacros.h"
#include "UObject/ObjectPtr.h"

#include "TestUniformField.generated.h"

class UHierarchicalInstancedStaticMeshComponent;

USTRUCT()
struct FTestUniformFieldCell {
    GENERATED_BODY()

    UPROPERTY()
    FVector potential{FVector::ZeroVector};

    void reset();
};

UCLASS()
class ATestUniformField : public AActor {
    GENERATED_BODY()
  public:
    ATestUniformField();

    void Tick(float dt) override;

    void add_source(FTestUniformFieldPointSourceData const& source);
    auto get_coord(FVector const& pos) const -> FIntVector;
    auto get_index(FIntVector const& pos) const -> int32;
    auto get_index(FVector const& pos) const -> int32;

    auto get_num_cells() const -> int32;
    auto get_grid_coords_sum() const -> int32;

    auto get_grid_centre() const -> FVector;
    auto get_grid_origin() const -> FVector;
    auto get_origin_cell_centre() const -> FVector;

    auto get_grid_coordinate(int32 i) const -> FIntVector;
    auto get_position_from_origin_cell_centre(int32 i) const -> FVector;

    auto get_cell_extent() const -> FVector;
    auto get_cell_dimensions() const -> FVector;

    auto get_grid_extent() const -> FVector;
    auto get_grid_dimensions() const -> FVector;
  protected:
    void BeginPlay() override;
    void OnConstruction(FTransform const& transform) override;

    void construct_grid();
    void update_cells();
    void reset_cells();
    void configure_visualisation_component();
    void update_visualisation();

    UPROPERTY(EditDefaultsOnly, Category = "Grid")
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> vector_meshes{nullptr};

    UPROPERTY(EditAnywhere, Category = "Grid")
    FIntVector grid_dimensions{1, 1, 1};

    UPROPERTY(EditAnywhere, Category = "Grid")
    FVector cell_extent{50.f};

    UPROPERTY()
    TArray<FTestUniformFieldCell> cells{};

    UPROPERTY(VisibleAnywhere, Category = "Grid")
    TArray<FTestUniformFieldPointSourceData> point_sources{};

    UPROPERTY(VisibleAnywhere, Category = "Grid")
    bool display_vectors{true};

    TArray<FTransform> vector_transforms;

#if WITH_EDITOR
    auto can_log() const { return dbg_log_timer >= dbg_log_cooldown; }
#endif

#if WITH_EDITORONLY_DATA
    UPROPERTY(VisibleAnywhere, Category = "Grid")
    float dbg_log_cooldown{1.f};
    UPROPERTY(VisibleAnywhere, Category = "Grid")
    float dbg_log_timer{0};
#endif
};
