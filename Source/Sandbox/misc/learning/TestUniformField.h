#pragma once

#include "TestUniformFieldSource.h"

#include "CoreMinimal.h"
#include "Containers/Array.h"
#include "Delegates/IDelegateInstance.h"
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

namespace ml {
struct QuantisedPotential {
    using Value = uint8;

    inline static constexpr Value strength_levels{10};
    inline static constexpr Value max_strength_coord{strength_levels - 1u};

    inline static constexpr Value grid_side_length{3};
    inline static constexpr Value max_grid_side_coord{grid_side_length - 1u};
    inline static constexpr Value grid_cells{grid_side_length * grid_side_length *
                                             grid_side_length};

    bool operator==(QuantisedPotential const&) const = default;

    static auto get_strength(float strength) -> Value;
    // Requires normalised vector
    static auto get_axis_section(float coord) -> Value;
    static auto get_direction_index(Value x, Value y, Value z) -> Value;

    // Default to invalid values
    Value strength{255};
    Value direction{255};
};
}

USTRUCT()
struct FTestUniformFieldCell {
    GENERATED_BODY()

    UPROPERTY()
    FVector3f potential{FVector3f::ZeroVector};
    ml::QuantisedPotential old_quantised_potential{};
    ml::QuantisedPotential new_quantised_potential{};

    bool quantised_changed() const;
    void reset();
};

UCLASS()
class ATestUniformField : public AActor {
    GENERATED_BODY()
  public:
    struct DirtyRun {
        int32 offset;
        int32 length;
    };

    ATestUniformField();

    void Tick(float dt) override;

    auto sample_field(FVector const& position) const -> FTestUniformFieldCell;

    [[nodiscard]]
    auto add_source(FTestUniformFieldPointSourceData const& source) -> int32;
    [[nodiscard]]
    auto add_sources(TArrayView<FTestUniformFieldPointSourceData const> sources) -> int32;
    void update_source(FTestUniformFieldPointSourceData const& source, int32 i);
    void update_sources(TArrayView<FTestUniformFieldPointSourceData const> sources, int32 offset);

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

    DECLARE_MULTICAST_DELEGATE_OneParam(FOnFieldPreConstruction, ATestUniformField&);
    FOnFieldPreConstruction on_field_pre_construction;
    DECLARE_MULTICAST_DELEGATE_OneParam(FOnFieldPostConstruction, ATestUniformField&);
    FOnFieldPostConstruction on_field_post_construction;
  protected:
    void OnConstruction(FTransform const& transform) override;
    void BeginPlay() override;
    void EndPlay(EEndPlayReason::Type const reason);

    void on_world_match_starting();

    void reset();
    void default_construct();

    void construct_grid();
    void update_cells();
    void reset_cells_to_default();
    void reset_sources();

    void configure_visualisation_component(UStaticMeshComponent& mc);
    void configure_box_mesh();
    void configure_hism();

    void update_visualisation();
    void initialise_hism_visualisation();
    void update_hism_visualisation();
    void update_hism_visibility();
    void update_hism_data(FVector const& origin, int32 offset, int32 length);
    void update_hism_instance(FVector const& origin, int32 index);

    void mark_all_dirty();
    void mark_grid_dirty();
    void mark_visualisation_dirty();

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
    TArray<int32> dirty_cells{};
    TArray<DirtyRun> dirty_runs;

    FDelegateHandle on_world_match_starting_handle;

#if WITH_EDITOR
    auto can_log() const { return dbg_log_timer >= dbg_log_cooldown; }
#endif

#if WITH_EDITORONLY_DATA
    UPROPERTY(EditAnywhere, Category = "Grid")
    float dbg_log_cooldown{2.f};
    UPROPERTY(VisibleAnywhere, Category = "Grid")
    float dbg_log_timer{0};
    UPROPERTY(VisibleAnywhere, Category = "Grid")
    int32 dbg_num_cells{0};
#endif
};
