#pragma once

#include <SandboxTests/support/SimulationTestScenario.h>

#include <SandboxCore/time_series_data.h>
#include <SandboxNative/RegistryEntityHandle.h>
#include <SpaceGame/entities/ProxyEntityMap.h>

namespace ml {
enum class ECollisionUniformGridTraceScenario : uint8 {
    HitsAndMisses,
    StopsAtEndpoint,
    ReturnsNearestHit,
    HandlesZeroLengthTraces,
    IncludesNegativeEndpointBoundary,
    AppliesAABBCentre,
    AxisParallelAndOrigin,
    SurfaceContacts,
    GridBoundaryTraversal,
    ShortAndNearParallelSegments,
    ClipsToGridBounds,
    DegenerateAABBs,
    CrossCellNearestHit,
    VariedGridGeometry,
    BoundaryPrecision,
    RebuildLifecycle,
    DeterministicReferenceSweep,
    InvarianceProperties,
    EmptyBatchesAndOutputReuse,
    DenseAndWideAABBs,
    ProductionScale,
};

class FCollisionUniformGridScenario final : public FSimulationTestScenario {
    struct FSample {
        TArray<int32> expected_cell_counts;
        TArray<int32> found_cell_counts;
    };

    static constexpr time_type sample_time{0.1};
    inline static FTimespan const timeout{0, 0, 4};
  public:
    explicit FCollisionUniformGridScenario(FSimulationTestContext& context);
    void run() override;
  private:
    void on_tear_down() override;
    void spawn_fixture();
    void bind_proxy_entities(FProxyEntityMap const& proxies);
    void initialise_simulation();
    void sample_grid();
    void on_end_tick(ATestBatchOrchestrator& orchestrator);
    void check_results();

    TStaticArray<FRegistryEntityHandle, 5> expected_handles_{};
    TimeSeriesData<FSample> samples_;
};

class FCollisionUniformGridTraceScenario final : public FSimulationTestScenario {
  public:
    FCollisionUniformGridTraceScenario(FSimulationTestContext& context,
                                       ECollisionUniformGridTraceScenario scenario);
    void run() override;
  private:
    void test_hits_and_misses();
    void test_stops_at_endpoint();
    void test_returns_nearest_hit();
    void test_handles_zero_length_traces();
    void test_includes_negative_endpoint_boundary();
    void test_applies_aabb_centre();
    void test_axis_parallel_and_origin();
    void test_surface_contacts();
    void test_grid_boundary_traversal();
    void test_short_and_near_parallel_segments();
    void test_clips_to_grid_bounds();
    void test_degenerate_aabbs();
    void test_cross_cell_nearest_hit();
    void test_varied_grid_geometry();
    void test_boundary_precision();
    void test_rebuild_lifecycle();
    void test_deterministic_reference_sweep();
    void test_invariance_properties();
    void test_empty_batches_and_output_reuse();
    void test_dense_and_wide_aabbs();
    void test_production_scale();

    ECollisionUniformGridTraceScenario scenario_;
};
}
