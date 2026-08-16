#include <Sandbox/batch_game/SpatialQueryManager.h>
#include <Sandbox/batch_game/TestBatchOrchestrator.h>
#include <Sandbox/batch_game/TestCapitalShipFighters.h>
#include <Sandbox/batch_game/TestCapitalShipProxy.h>
#include <Sandbox/batch_game/TestCapitalShips.h>
#include <Sandbox/batch_game/TestEntity.h>
#include <Sandbox/batch_game/TestEntityType.h>

#include <SandboxTests/support/SoftTestAssertions.h>
#include <SandboxTests/support/test_setup.h>
#include <SandboxTests/support/TestActorSpawning.h>
#include <SandboxTests/support/TestSimulationDriver.h>

#include <Components/InstancedStaticMeshComponent.h>
#include <Components/MapTestSpawner.h>
#include <CQTest.h>
#include <Misc/Optional.h>

#include <functional>

TEST_CLASS(SpatialQueryManager, "Sandbox.LevelTests")
{
    using ThisClass = SpatialQueryManager;
    using time_type = ml::TestSimulationDriver::time_type;

    static constexpr time_type query_time{0.2};
    FTimespan const timeout{0, 0, 4};

    TUniquePtr<FMapTestSpawner> spawner{nullptr};
    ml::FSoftTestAssertions checks{};
    TOptional<ml::TestSimulationDriver> test_driver{NullOpt};
    ATestCapitalShips* capitals{nullptr};
    ATestCapitalShipFighters* fighters{nullptr};
    UInstancedStaticMeshComponent* capital_instances{nullptr};
    UInstancedStaticMeshComponent* fighter_instances{nullptr};
    bool queried_hits{false};

    BEFORE_EACH()
    { spawner = ml::level_test_setup(TEXT("FuncT_capital_fighter_handles"), TestRunner, checks); }
    AFTER_EACH()
    {
        if (test_driver.IsSet()) {
            test_driver->orchestrator.clear_end_tick_test_hook();
            test_driver->orchestrator.pause_simulation();
        }
    }
  private:
    static auto make_hit(UPrimitiveComponent const& component, int32 const item)
        -> ml::FSpatialQueryHit {
        return {&component, item};
    }

    static void sort_hits_by_component(TArray<ml::FSpatialQueryHit> & hits) {
        std::less<void const*> const pointer_less{};
        hits.Sort([pointer_less](ml::FSpatialQueryHit const& lhs, ml::FSpatialQueryHit const& rhs) {
            return pointer_less(lhs.component, rhs.component);
        });
    }

    auto get_expected_type(UPrimitiveComponent const* const component) const -> ETestEntityType {
        check(component == capital_instances || component == fighter_instances);
        return component == capital_instances ? ETestEntityType::CapitalShip
                                              : ETestEntityType::CapitalShipFighter;
    }

    void initial_setup() {
        test_driver = ml::TestSimulationDriver::from_world(spawner->GetWorld());
        auto& orchestrator{test_driver->orchestrator};
        capitals = const_cast<ATestCapitalShips*>(orchestrator.get_capital_ships());
        fighters = const_cast<ATestCapitalShipFighters*>(orchestrator.get_capital_ship_fighters());

        if (capitals) {
            capital_instances = capitals->FindComponentByClass<UInstancedStaticMeshComponent>();
        }
        if (fighters) {
            fighter_instances = fighters->FindComponentByClass<UInstancedStaticMeshComponent>();
        }

        orchestrator.set_end_tick_test_hook(
            FOrchestratorEndTickTestHook::CreateRaw(this, &ThisClass::on_end_tick));
        test_driver->timeline.at(query_time, [this] { resolve_hits(); }).finish_at(query_time);
        orchestrator.start_simulation();
    }

    void resolve_hits() {
        checks.is_valid(capitals, TEXT("Capital ship actor is available"));
        checks.is_valid(fighters, TEXT("Capital ship fighter actor is available"));
        checks.not_nullptr(capital_instances,
                           TEXT("Capital ship collision component is available"));
        checks.not_nullptr(fighter_instances,
                           TEXT("Capital ship fighter collision component is available"));
        if (!capitals || !fighters || !capital_instances || !fighter_instances) {
            return;
        }

        checks.is_greater_than(
            capitals->get_num_instances(), int32{1}, TEXT("At least two capital ship instances"));
        checks.is_greater_than(
            fighters->get_num_instances(), int32{1}, TEXT("At least two fighter instances"));
        if ((capitals->get_num_instances() < 2) || (fighters->get_num_instances() < 2)) {
            return;
        }

        TArray<ml::FSpatialQueryHit> hits{
            make_hit(*capital_instances, 0),
            make_hit(*capital_instances, 1),
            make_hit(*fighter_instances, 1),
            make_hit(*fighter_instances, 0),
        };
        sort_hits_by_component(hits);

        TArray<FRegistryEntityHandle> handles;
        handles.SetNumUninitialized(hits.Num());
        test_driver->orchestrator.get_spatial_query_manager().resolve_hits(hits, handles);

        checks.are_equal(hits.Num(), handles.Num(), TEXT("One handle is returned for every hit"));
        auto const n_hits{hits.Num()};
        for (int32 i{}; i < n_hits; ++i) {
            auto const handle{handles[i]};
            checks.is_true(!handle.is_null(), TEXT("Resolved handle is non-null"), i);
            if (!handle.is_null()) {
                checks.are_equal(get_expected_type(hits[i].component),
                                 test_driver->orchestrator.get_entity_type(handle),
                                 TEXT("Resolved handle has the expected entity type"),
                                 i);
            }
        }

        queried_hits = true;
    }

    void on_end_tick(ATestBatchOrchestrator&) {
        test_driver->timeline.tick(test_driver->get_time());
    }

    void run_checks() {
        checks.is_true(queried_hits,
                       TEXT("Hit resolution was run at the requested simulation time"));
    }

    TEST_METHOD(ResolvesHitBatches)
    {
        TestCommandBuilder.StartWhen([this] { return nullptr != spawner->FindFirstPlayerPawn(); })
            .Then([this] { initial_setup(); })
            .Until([this] { return test_driver->timeline.is_finished(); }, timeout)
            .Then([this] { run_checks(); });
    }
};

TEST_CLASS(SpatialQueryLineOfSight, "Sandbox.LevelTests")
{
    using ThisClass = SpatialQueryLineOfSight;

    static constexpr float distance{30000.f};
    TUniquePtr<FMapTestSpawner> spawner{nullptr};
    TOptional<ml::FTestBatchOrchestratorLevelSetup> setup{NullOpt};
    ml::FSoftTestAssertions checks{};
    TOptional<ml::TestSimulationDriver> driver{NullOpt};

    TArray<FName> names{
        TEXT("North"),
        TEXT("South"),
        TEXT("East"),
        TEXT("West"),
    };
    TArray<FVector3f> locations{
        {0.f, distance, 0.f},
        {0.f, -distance, 0.f},
        {distance, 0.f, 0.f},
        {-distance, 0.f, 0.f},
    };

    TArray<FRegistryEntityHandle> expected;

    static constexpr float spawn_cooldown{999.f};
  public:
    BEFORE_EACH()
    {
        checks.test_runner = TestRunner;
        checks.all_passed = true;

        spawner = FMapTestSpawner::CreateFromTempLevel(TestCommandBuilder);

        auto const n_names{names.Num()};
        setup.Emplace(*spawner, *TestRunner, checks);
        expected.Init({}, names.Num());
        setup->setup(TestCommandBuilder,
                     [this, n_names](UWorld& world, UTestSimulationConfig const& config) {
                         for (int32 i{}; i < n_names; ++i) {
                             auto* const proxy{ml::spawn_capital_proxy(
                                 world, config, checks, names[i], FVector{locations[i]})};
                             check(proxy);
                             proxy->set_initial_spawn_delay(spawn_cooldown);
                             proxy->set_spawn_cooldown(spawn_cooldown);
                         }
                         ATestBatchOrchestrator::on_proxy_entities_bound.AddRaw(this,
                                                                                &ThisClass::bind);
                     });
    }
    AFTER_EACH()
    {
        ATestBatchOrchestrator::on_proxy_entities_bound.RemoveAll(this);
        check(driver);
        driver->orchestrator.clear_end_tick_test_hook();
        driver->orchestrator.pause_simulation();
        setup->teardown();
    }
  private:
    void bind(FProxyEntityMap const& proxies) {
        check(proxies.Num() == names.Num());
        for (auto const& [actor, identifiers] : proxies) {
            auto const* const entity{Cast<ITestEntity>(actor)};
            check(entity);

            auto const i{names.Find(entity->get_test_name())};
            check(i != INDEX_NONE);

            expected[i] = entity->get_entity_handle();
            check(!expected[i].is_null());
        }
        ATestBatchOrchestrator::on_proxy_entities_bound.RemoveAll(this);
    }
    void run_queries() {
        FVectors3f starts;
        FVectors3f ends;

        TStaticArray<float, 3> scales{0.5f, 1.f, 2.f};
        auto const n_scales{scales.Num()};

        for (float const scale : scales) {
            for (auto const& location : locations) {
                starts.add(FVector3f::ZeroVector);
                ends.add(location * scale);
            }
        }

        TArray<FRegistryEntityHandle> results;
        results.SetNumUninitialized(ends.num());

        driver->orchestrator.get_spatial_query_manager().trace_line_of_sight(
            starts.get_const_view(), ends.get_const_view(), results);

        auto const n_names{names.Num()};
        FRegistryEntityHandle const null_handle{};

        for (int32 i{}; i < n_names; ++i) {
            checks.are_equal(null_handle, results[i], TEXT("Half-distance trace misses"), i);
        }

        for (int32 i{}; i < n_names; ++i) {
            checks.are_equal(
                expected[i], results[i + n_names], TEXT("Ship trace resolves handle"), i);
        }

        for (int32 i{}; i < n_names; ++i) {
            checks.are_equal(
                expected[i], results[i + 2 * n_names], TEXT("Past-ship trace resolves handle"), i);
        }
    }
    void on_end_tick(ATestBatchOrchestrator&) {
        driver->timeline.tick(driver->get_time());
    }
    TEST_METHOD(ResolvesLineOfSightBatches)
    {
        TestCommandBuilder
            .Do([this] {
                driver = ml::TestSimulationDriver::from_world(setup->get_world());
                driver->orchestrator.set_end_tick_test_hook(
                    FOrchestratorEndTickTestHook::CreateRaw(this, &ThisClass::on_end_tick));
                driver->timeline.at(0.1, [this] { run_queries(); }).finish_at(0.2);
                driver->orchestrator.start_simulation();
            })
            .Until([this] { return driver->timeline.is_finished(); }, FTimespan{0, 0, 2})
            .Then([this] { SANDBOX_TESTS_ASSERT_ALL_PASSED(checks); });
    }
};
