#pragma once
#include "../support/simulation_test_support.h"

namespace ioj::sim {

enum class CollisionUniformGridTraceScenario : std::uint8_t {
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

void run_worldless_collision_uniform_grid_membership(tests::SimulationFixture const& config);
void run_collision_uniform_grid_trace(tests::SimulationFixture const& config,
                                      CollisionUniformGridTraceScenario scenario);
}
