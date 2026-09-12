#pragma once

#include <CoreMinimal.h>

class FAutomationTestBase;
class USpaceGameLevelConfig;

namespace ml {
struct FSoftTestAssertions;

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
    StaticGeometry,
    StaticHarvesting,
};

void run_worldless_collision_uniform_grid_membership(FAutomationTestBase& test,
                                                     FSoftTestAssertions& checks,
                                                     USpaceGameLevelConfig const& config);
void run_collision_uniform_grid_trace(FAutomationTestBase& test,
                                      FSoftTestAssertions& checks,
                                      USpaceGameLevelConfig const& config,
                                      ECollisionUniformGridTraceScenario scenario);
}
