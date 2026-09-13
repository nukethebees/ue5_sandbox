#pragma once
#include "../support/simulation_test_support.h"

namespace ml {

enum class ECollisionUniformGridTraceScenario : std::uint8_t {
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
    StaticGeometry,
};

void run_worldless_collision_uniform_grid_membership(
    ml::simulation_tests::SimulationFixture const& config);
void run_collision_uniform_grid_trace(ml::simulation_tests::SimulationFixture const& config,
                                      ECollisionUniformGridTraceScenario scenario);
}
