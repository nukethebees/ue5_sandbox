#include <SandboxTests/support/SimulationTestAssets.h>
#include <SandboxTests/support/test_setup.h>
#include <SandboxTests/support/TestActorSpawning.h>
#include <SandboxTests/support/TestEnhancedInputSubsystem.h>

#include <SpaceGame/ships/player/PlayerControlContexts.h>
#include <SpaceGame/ships/player/SpaceGamePlayerController.h>
#include <SpaceGame/ships/player/TestSpaceShip.h>
#include <SpaceGame/simulation/SpaceGameLevelConfig.h>
#include <SpaceGameSimulation/combat/lasers/TestLasersSimulation.h>
#include <SpaceGameSimulation/entities/TestEntityRegistry.h>
#include <SpaceGameSimulation/simulation/SimulationClock.h>
#include <SpaceGameSimulation/simulation/SpatialQueryManager.h>

#include <SandboxCore/frame_memory_resource.h>

#include <Camera/CameraActor.h>
#include <CQTest.h>
#include <EnhancedInputComponent.h>
#include <InputAction.h>
#include <InputMappingContext.h>
#include <UObject/UnrealType.h>

struct FPlayerControlContextsTestAccess {
    static void fail_next_bind(FPlayerControlContexts& contexts, EPlayerControlContext context) {
        contexts.fail_bind_mask_ |= static_cast<uint8>(1u << static_cast<uint8>(context));
    }
};

TEST_CLASS(ControlContextTransitions, "Sandbox.UnitTests")
{
    ASpaceGamePlayerController* controller_{};
    ATestSpaceShip* ship_{};
    ACameraActor* camera_{};
    UEnhancedInputComponent* component_{};
    USandboxTestEnhancedInputSubsystem* subsystem_{};
    FSpaceShipControllerInputs ship_input_;
    FObserverControlInputs observer_input_;
    FBenchmarkControlInputs benchmark_input_;
    FGlobalControlInputs global_input_;
    FPlayerControlContexts contexts_;
    FSimulationClock clock_;
    FTestEntityRegistry registry_;
    ml::FSpatialQueryManager queries_{registry_};
    ml::FFrameMemoryResource frame_memory_{1024 * 1024};
    ml::test_lasers::Simulation lasers_{clock_, registry_, queries_, frame_memory_};
    ml::test_space_ship::Simulation ship_simulation_{clock_, registry_, queries_, lasers_};

    BEFORE_EACH()
    {
        auto const world_result{ml::get_editor_world()};
        auto const* const config{ml::load_default_level_config()};
        if (!TestRunner->TestTrue(TEXT("Editor world is available"), world_result.has_value()) ||
            !TestRunner->TestNotNull(TEXT("Level config loads"), config)) {
            return;
        }
        auto& world{*world_result.value()};
        controller_ = world.SpawnActor<ASpaceGamePlayerController>();
        ship_ =
            ml::spawn_player_ship(world, config->classes.player_ship_class, &config->player_ship);
        if (IsValid(ship_)) {
            ship_->bind_simulation(ship_simulation_);
        }
        camera_ = world.SpawnActor<ACameraActor>();
        component_ = NewObject<UEnhancedInputComponent>(controller_);
        subsystem_ = NewObject<USandboxTestEnhancedInputSubsystem>(controller_);
        subsystem_->initialise();
        auto const* const defaults{config->classes.player_controller_class.GetDefaultObject()};
        auto const* const input_property{
            FindFProperty<FStructProperty>(defaults->GetClass(), TEXT("input"))};
        ship_input_ = *input_property->ContainerPtrToValuePtr<FSpaceShipControllerInputs>(defaults);
        ship_input_.mapping_context = NewObject<UInputMappingContext>(controller_);
        auto* const action{NewObject<UInputAction>(controller_)};
        global_input_.mapping_context = NewObject<UInputMappingContext>(controller_);
        global_input_.toggle_menu = action;
        benchmark_input_.mapping_context = NewObject<UInputMappingContext>(controller_);
        benchmark_input_.exit = action;
        observer_input_.mapping_context = NewObject<UInputMappingContext>(controller_);
        observer_input_.move = action;
        observer_input_.vertical_move = action;
        observer_input_.look = action;
        observer_input_.engage_look = action;
        observer_input_.adjust_speed = action;
        observer_input_.boost = action;
        TestRunner->TestTrue(TEXT("Input owners initialise"), initialise());
    }

    AFTER_EACH()
    {
        contexts_.shutdown();
        if (IsValid(ship_)) {
            ship_->Destroy();
        }
        if (IsValid(camera_)) {
            camera_->Destroy();
        }
        if (IsValid(controller_)) {
            controller_->Destroy();
        }
    }

    auto initialise() -> bool {
        return contexts_.initialise(*controller_,
                                    *component_,
                                    *subsystem_,
                                    ship_input_,
                                    observer_input_,
                                    benchmark_input_,
                                    global_input_);
    }

    TEST_METHOD(ShipTransitionsPreserveGlobalAndUnrelatedBindings)
    {
        auto* const sentinel{NewObject<UInputAction>(controller_)};
        component_->BindActionValueLambda(
            sentinel, ETriggerEvent::Started, [](FInputActionValue const&) {});
        contexts_.set_ship(ship_);
        TestRunner->TestTrue(TEXT("None to Player succeeds"),
                             contexts_.set_control_context(EPlayerControlContext::Player));
        auto const count{component_->GetActionEventBindings().Num()};
        TestRunner->TestTrue(TEXT("Repeated Player succeeds"),
                             contexts_.set_control_context(EPlayerControlContext::Player));
        TestRunner->TestEqual(TEXT("Player handlers are not duplicated"),
                              component_->GetActionEventBindings().Num(),
                              count);
        TestRunner->TestTrue(TEXT("Player to None succeeds"),
                             contexts_.set_control_context(EPlayerControlContext::None));
        TestRunner->TestFalse(TEXT("Ship mapping is removed"),
                              subsystem_->HasMappingContext(ship_input_.mapping_context));
        TestRunner->TestTrue(TEXT("Global mapping remains in None"),
                             subsystem_->HasMappingContext(global_input_.mapping_context));
        TestRunner->TestEqual(TEXT("Global and unrelated handlers remain"),
                              component_->GetActionEventBindings().Num(),
                              2);
        contexts_.shutdown();
        contexts_.shutdown();
        TestRunner->TestFalse(TEXT("Shutdown removes global mapping"),
                              subsystem_->HasMappingContext(global_input_.mapping_context));
        TestRunner->TestEqual(TEXT("Shutdown retains unrelated handler"),
                              component_->GetActionEventBindings().Num(),
                              1);
    }

    TEST_METHOD(BenchmarkRepeatAndShutdownRemoveEveryOwnedBinding)
    {
        TestRunner->TestTrue(TEXT("Benchmark works without pawn or camera"),
                             contexts_.set_control_context(EPlayerControlContext::Benchmark));
        auto const count{component_->GetActionEventBindings().Num()};
        TestRunner->TestTrue(TEXT("Benchmark repeated request succeeds"),
                             contexts_.set_control_context(EPlayerControlContext::Benchmark));
        TestRunner->TestEqual(TEXT("Benchmark handler is not duplicated"),
                              component_->GetActionEventBindings().Num(),
                              count);
        TestRunner->TestFalse(TEXT("Benchmark disables global mapping"),
                              subsystem_->HasMappingContext(global_input_.mapping_context));
        auto* const registered_mapping{benchmark_input_.mapping_context.Get()};
        benchmark_input_.mapping_context = NewObject<UInputMappingContext>(controller_);
        contexts_.shutdown();
        TestRunner->TestFalse(TEXT("Cleanup removes the originally registered mapping"),
                              subsystem_->HasMappingContext(registered_mapping));
        TestRunner->TestEqual(TEXT("Shutdown removes all owned handlers"),
                              component_->GetActionEventBindings().Num(),
                              0);
    }

    TEST_METHOD(RejectedContextDoesNotDisturbActiveInput)
    {
        contexts_.set_control_context(EPlayerControlContext::Benchmark);
        auto const count{component_->GetActionEventBindings().Num()};
        TestRunner->AddExpectedError(
            TEXT("Requested context cannot bind"), EAutomationExpectedErrorFlags::Contains, 1);
        TestRunner->TestFalse(TEXT("Player without ship is rejected"),
                              contexts_.set_control_context(EPlayerControlContext::Player));
        TestRunner->TestTrue(TEXT("Benchmark remains active"),
                             contexts_.get_active_context() == EPlayerControlContext::Benchmark);
        TestRunner->TestEqual(TEXT("Preflight leaves bindings unchanged"),
                              component_->GetActionEventBindings().Num(),
                              count);
        TestRunner->TestTrue(TEXT("Benchmark mapping remains"),
                             subsystem_->HasMappingContext(benchmark_input_.mapping_context));
    }

    TEST_METHOD(FailedBindRestoresPreviousContextAndGlobalPolicy)
    {
        contexts_.set_camera(camera_);
        contexts_.set_control_context(EPlayerControlContext::Benchmark);
        FPlayerControlContextsTestAccess::fail_next_bind(contexts_,
                                                         EPlayerControlContext::Observer);
        TestRunner->AddExpectedError(
            TEXT("Failed to bind requested context"), EAutomationExpectedErrorFlags::Contains, 1);
        TestRunner->TestFalse(TEXT("Failed Observer bind is reported"),
                              contexts_.set_control_context(EPlayerControlContext::Observer));
        TestRunner->TestTrue(TEXT("Benchmark is restored"),
                             contexts_.get_active_context() == EPlayerControlContext::Benchmark);
        TestRunner->TestTrue(TEXT("Benchmark mapping is restored"),
                             subsystem_->HasMappingContext(benchmark_input_.mapping_context));
        TestRunner->TestFalse(TEXT("Failed Observer leaves no mapping"),
                              subsystem_->HasMappingContext(observer_input_.mapping_context));
        TestRunner->TestFalse(TEXT("Global mapping remains disabled"),
                              subsystem_->HasMappingContext(global_input_.mapping_context));
        TestRunner->TestEqual(TEXT("Only global and Benchmark bindings remain"),
                              component_->GetActionEventBindings().Num(),
                              2);
    }

    TEST_METHOD(FailedRollbackLeavesNoneWithNoGameplayBindings)
    {
        contexts_.set_camera(camera_);
        contexts_.set_control_context(EPlayerControlContext::Benchmark);
        FPlayerControlContextsTestAccess::fail_next_bind(contexts_,
                                                         EPlayerControlContext::Observer);
        FPlayerControlContextsTestAccess::fail_next_bind(contexts_,
                                                         EPlayerControlContext::Benchmark);
        TestRunner->AddExpectedError(
            TEXT("Failed to bind requested context"), EAutomationExpectedErrorFlags::Contains, 1);
        TestRunner->AddExpectedError(
            TEXT("Failed to restore previous context"), EAutomationExpectedErrorFlags::Contains, 1);
        TestRunner->TestFalse(TEXT("Rollback failure is reported"),
                              contexts_.set_control_context(EPlayerControlContext::Observer));
        TestRunner->TestTrue(TEXT("Failed rollback leaves None"),
                             contexts_.get_active_context() == EPlayerControlContext::None);
        TestRunner->TestFalse(TEXT("Benchmark mapping is removed"),
                              subsystem_->HasMappingContext(benchmark_input_.mapping_context));
        TestRunner->TestFalse(TEXT("Observer mapping is absent"),
                              subsystem_->HasMappingContext(observer_input_.mapping_context));
        TestRunner->TestEqual(
            TEXT("Only global handler remains"), component_->GetActionEventBindings().Num(), 1);
    }

    TEST_METHOD(FailedBindRestoresShipAndCleansRequestedBindings)
    {
        contexts_.set_ship(ship_);
        contexts_.set_camera(camera_);
        contexts_.set_control_context(EPlayerControlContext::Player);
        auto const count{component_->GetActionEventBindings().Num()};
        FPlayerControlContextsTestAccess::fail_next_bind(contexts_,
                                                         EPlayerControlContext::Observer);
        TestRunner->AddExpectedError(
            TEXT("Failed to bind requested context"), EAutomationExpectedErrorFlags::Contains, 1);
        TestRunner->TestFalse(TEXT("Failed Observer transition is reported"),
                              contexts_.set_control_context(EPlayerControlContext::Observer));
        TestRunner->TestTrue(TEXT("Ship context is restored"),
                             contexts_.get_active_context() == EPlayerControlContext::Player);
        TestRunner->TestTrue(TEXT("Ship mapping is restored"),
                             subsystem_->HasMappingContext(ship_input_.mapping_context));
        TestRunner->TestFalse(TEXT("Requested Observer mapping is cleaned up"),
                              subsystem_->HasMappingContext(observer_input_.mapping_context));
        TestRunner->TestEqual(TEXT("Ship binding count is restored"),
                              component_->GetActionEventBindings().Num(),
                              count);
    }

    TEST_METHOD(NoneRetainsGlobalPolicyAcrossFailedAndSuccessfulTransitions)
    {
        contexts_.set_camera(camera_);
        FPlayerControlContextsTestAccess::fail_next_bind(contexts_,
                                                         EPlayerControlContext::Benchmark);
        TestRunner->AddExpectedError(
            TEXT("Failed to bind requested context"), EAutomationExpectedErrorFlags::Contains, 1);
        TestRunner->TestFalse(TEXT("Failed bind from None is reported"),
                              contexts_.set_control_context(EPlayerControlContext::Benchmark));
        TestRunner->TestTrue(TEXT("None is restored"),
                             contexts_.get_active_context() == EPlayerControlContext::None);
        TestRunner->TestTrue(TEXT("None restores previously enabled global mapping"),
                             subsystem_->HasMappingContext(global_input_.mapping_context));
        contexts_.set_control_context(EPlayerControlContext::Benchmark);
        contexts_.set_control_context(EPlayerControlContext::None);
        TestRunner->TestFalse(TEXT("Successful Benchmark to None retains disabled global mapping"),
                              subsystem_->HasMappingContext(global_input_.mapping_context));
        TestRunner->TestFalse(TEXT("None removes Benchmark mapping"),
                              subsystem_->HasMappingContext(benchmark_input_.mapping_context));
        contexts_.set_control_context(EPlayerControlContext::Observer);
        TestRunner->TestTrue(TEXT("Observer re-enables global mapping"),
                             subsystem_->HasMappingContext(global_input_.mapping_context));
    }

    TEST_METHOD(ReinitialisationIsIdempotentAndReleasesOldComponent)
    {
        contexts_.set_control_context(EPlayerControlContext::Benchmark);
        auto const count{component_->GetActionEventBindings().Num()};
        TestRunner->TestTrue(TEXT("Repeated setup succeeds"), initialise());
        TestRunner->TestEqual(TEXT("Setup does not duplicate global or gameplay input"),
                              component_->GetActionEventBindings().Num(),
                              count);
        auto* const old_component{component_};
        auto* const old_subsystem{subsystem_};
        component_ = NewObject<UEnhancedInputComponent>(controller_);
        subsystem_ = NewObject<USandboxTestEnhancedInputSubsystem>(controller_);
        subsystem_->initialise();
        TestRunner->TestTrue(TEXT("Replacement dependencies initialise"), initialise());
        TestRunner->TestEqual(TEXT("Old component releases all owned handlers"),
                              old_component->GetActionEventBindings().Num(),
                              0);
        TestRunner->TestFalse(TEXT("Old subsystem releases Benchmark mapping"),
                              old_subsystem->HasMappingContext(benchmark_input_.mapping_context));
        TestRunner->TestTrue(TEXT("Benchmark is rebound on new subsystem"),
                             subsystem_->HasMappingContext(benchmark_input_.mapping_context));
        TestRunner->TestEqual(TEXT("New component has exactly one copy of each binding"),
                              component_->GetActionEventBindings().Num(),
                              count);
    }

    TEST_METHOD(InvalidGlobalConfigurationUnwindsSetupAndCanRetry)
    {
        contexts_.shutdown();
        auto* const action{global_input_.toggle_menu};
        global_input_.toggle_menu = nullptr;
        TestRunner->AddExpectedError(
            TEXT("FGlobalPlayerInput::initialise: Input dependencies are invalid"),
            EAutomationExpectedErrorFlags::Contains,
            1);
        TestRunner->TestFalse(TEXT("Invalid setup fails"), initialise());
        TestRunner->TestEqual(TEXT("Partial setup leaves no handlers"),
                              component_->GetActionEventBindings().Num(),
                              0);
        global_input_.toggle_menu = action;
        TestRunner->TestTrue(TEXT("Corrected configuration can retry"), initialise());
        TestRunner->TestEqual(
            TEXT("Retry binds global input once"), component_->GetActionEventBindings().Num(), 1);
    }
};
