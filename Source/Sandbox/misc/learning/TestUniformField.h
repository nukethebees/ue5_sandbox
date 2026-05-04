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
class UWorld;
class UStaticMesh;
class UStaticMeshComponent;

USTRUCT()
struct FTestUniformFieldCell {
    GENERATED_BODY()

    UPROPERTY()
    FVector3f potential{FVector3f::ZeroVector};

    void reset();
};

UCLASS()
class ATestUniformField : public AActor {
    GENERATED_BODY()
  public:
    ATestUniformField();

    void Tick(float dt) override;

    static auto find_field(UWorld& world) -> TWeakObjectPtr<ATestUniformField>;
    auto sample_field(FVector const& position) const -> FTestUniformFieldCell;

    void add_source(FTestUniformFieldPointSourceData const& source);
    auto get_coord(FVector const& pos) const -> FIntVector;
    auto get_index(FIntVector const& coord) const -> int32;
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

    auto get_field_extent() const -> FVector;
    auto get_field_dimensions() const -> FVector;

    void mark_all_dirty();
    void mark_grid_dirty();
    void mark_visualisation_dirty();
  protected:
    void BeginPlay() override;
    void OnConstruction(FTransform const& transform) override;
    void PostActorCreated() override;

    void construct_grid();
    void update_cells();
    void reset_cells();
    void reset_sources();

    void configure_visualisation_component(UStaticMeshComponent& mc);
    void configure_box_mesh();
    void configure_hism();

    void update_visualisation();
    void initialise_hism_visualisation();
    void update_hism_visualisation();
    void update_hism_data(FVector const& origin);
    void update_hism_visibility();

    UPROPERTY(EditAnywhere, Category = "Grid")
    TObjectPtr<UStaticMeshComponent> box_mesh{nullptr};
    UPROPERTY(EditDefaultsOnly, Category = "Grid")
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> vector_meshes{nullptr};
    UPROPERTY(VisibleAnywhere, Category = "Grid")
    FIntVector grid_dimensions{1, 1, 1};
    UPROPERTY(EditAnywhere, Category = "Grid")
    FVector box_extent{500.f};
    UPROPERTY(EditAnywhere, Category = "Grid")
    FVector cell_extent{50.f};
    UPROPERTY()
    TArray<FTestUniformFieldCell> cells{};
    UPROPERTY(VisibleAnywhere, Category = "Grid")
    TArray<FTestUniformFieldPointSourceData> point_sources{};
    UPROPERTY(VisibleAnywhere, Category = "Grid")
    bool grid_dirty{false};
    UPROPERTY(VisibleAnywhere, Category = "Grid")
    bool visualisation_dirty{false};

    // Visualisation
    UPROPERTY(VisibleAnywhere, Category = "Field")
    float max_abs_strength{};
    UPROPERTY(EditAnywhere, Category = "Grid")
    bool display_box{true};
    UPROPERTY(EditAnywhere, Category = "Grid")
    bool display_vectors{true};
    UPROPERTY(EditAnywhere, Category = "Grid")
    float min_length_scale{0.2};
    UPROPERTY(EditAnywhere, Category = "Grid")
    FVector vector_base_scale{FVector::OneVector};

    TArray<FTransform> vector_transforms;
    TArray<float> vector_intensities;

#if WITH_EDITOR
    auto can_log() const { return dbg_log_timer >= dbg_log_cooldown; }
#endif

#if WITH_EDITORONLY_DATA
    UPROPERTY(EditAnywhere, Category = "Grid")
    float dbg_log_cooldown{2.f};
    UPROPERTY(VisibleAnywhere, Category = "Grid")
    float dbg_log_timer{0};
#endif
};
