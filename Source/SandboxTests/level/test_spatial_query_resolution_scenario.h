#pragma once

#include <SandboxTests/support/SimulationTestScenario.h>
#include <SandboxTests/support/TestSimulationDriver.h>

#include <SpaceGame/simulation/SpatialQueryHit.h>
#include <SpaceGame/entities/TestEntityType.h>

class ATestCapitalShipFighters;
class ATestCapitalShips;
class UInstancedStaticMeshComponent;
class UPrimitiveComponent;

namespace ml {
class FSpatialQueryResolutionScenario final : public FSimulationTestScenario {
    using time_type = TestSimulationDriver::time_type;
    static constexpr time_type query_time{0.2};
    inline static FTimespan const timeout{0, 0, 4};
  public:
    explicit FSpatialQueryResolutionScenario(FSimulationTestContext& context);
    void run() override;
    void tear_down() override;
  private:
    static auto make_hit(UPrimitiveComponent const& component, int32 item) -> FSpatialQueryHit;
    static void sort_hits_by_component(TArray<FSpatialQueryHit>& hits);
    auto get_expected_type(UPrimitiveComponent const* component) const -> ETestEntityType;
    void spawn_fixture();
    void initial_setup();
    void resolve_hits();
    void on_end_tick(ATestBatchOrchestrator& orchestrator);
    void run_checks();

    TOptional<TestSimulationDriver> test_driver{NullOpt};
    ATestCapitalShips* capitals{nullptr};
    ATestCapitalShipFighters* fighters{nullptr};
    UInstancedStaticMeshComponent* capital_instances{nullptr};
    UInstancedStaticMeshComponent* fighter_instances{nullptr};
    bool queried_hits{false};
};
}
