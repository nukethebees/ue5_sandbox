#include <SandboxTests/support/PlayerControllerTestAccess.h>
#include <SandboxTests/support/SimulationTestAssets.h>
#include <SandboxTests/support/test_setup.h>
#include <SandboxTests/support/TestActorSpawning.h>
#include <SandboxTests/support/TestEnhancedInputSubsystem.h>

#include <SpaceGame/ships/player/TestSpaceShip.h>
#include <SpaceGame/simulation/SpaceGameLevelConfig.h>

#include <Camera/CameraActor.h>
#include <CQTest.h>
#include <EnhancedInputComponent.h>
#include <InputAction.h>
#include <UObject/UnrealType.h>

TEST_CLASS(PlayerController, "Sandbox.UnitTests")
{
    TEST_METHOD(EndPlayReleasesBoundInputAndModalState)
    {
        auto const world_result{ml::get_editor_world()};
        if (!TestRunner->TestTrue(TEXT("Editor world is available"), world_result.has_value())) {
            return;
        }
        auto* const controller{world_result.value()->SpawnActor<ASpaceGamePlayerController>()};
        auto* const component{NewObject<UEnhancedInputComponent>(controller)};
        auto* const subsystem{NewObject<USandboxTestEnhancedInputSubsystem>(controller)};
        subsystem->initialise();
        FSpaceShipControllerInputs input;
        input.mapping_context = NewObject<UInputMappingContext>(controller);
        input.move = NewObject<UInputAction>(controller);
        FPlayerControllerTestAccess::prepare_input(*controller, *component, *subsystem, input);
        TestRunner->TestTrue(TEXT("Benchmark binds without possession"),
                             FPlayerControllerTestAccess::select_context(
                                 *controller, EPlayerControlContext::Benchmark));
        auto const* const benchmark_property{
            FindFProperty<FStructProperty>(controller->GetClass(), TEXT("benchmark_input"))};
        auto const& benchmark{
            *benchmark_property->ContainerPtrToValuePtr<FBenchmarkControlInputs>(controller)};
        TestRunner->TestTrue(TEXT("Benchmark mapping is active before EndPlay"),
                             subsystem->HasMappingContext(benchmark.mapping_context));
        FPlayerControllerTestAccess::prepare_completion(*controller);
        FPlayerControllerTestAccess::end_play(*controller);
        TestRunner->TestTrue(TEXT("EndPlay leaves None"),
                             controller->get_active_control_context() ==
                                 EPlayerControlContext::None);
        TestRunner->TestFalse(TEXT("EndPlay releases modal state"),
                              FPlayerControllerTestAccess::has_modal(*controller));
        TestRunner->TestFalse(TEXT("EndPlay removes the context mapping"),
                              subsystem->HasMappingContext(benchmark.mapping_context));
        TestRunner->TestEqual(TEXT("EndPlay removes global and gameplay handlers"),
                              component->GetActionEventBindings().Num(),
                              0);
        controller->Destroy();
    }

    TEST_METHOD(MainMenuBeforeBeginPlayRejectsGameplayActivation)
    {
        auto const world_result{ml::get_editor_world()};
        if (!TestRunner->TestTrue(TEXT("Editor world is available"), world_result.has_value())) {
            return;
        }
        auto* const controller{world_result.value()->SpawnActor<ASpaceGamePlayerController>()};
        auto* const camera{world_result.value()->SpawnActor<ACameraActor>()};
        controller->show_main_menu();
        controller->SetupInputComponent();
        TestRunner->TestTrue(TEXT("Main menu has no gameplay context"),
                             controller->get_active_control_context() ==
                                 EPlayerControlContext::None);
        TestRunner->TestFalse(
            TEXT("Main menu rejects Observer activation"),
            controller->activate_playerless_camera(*camera, EPlayerControlContext::Observer));
        TestRunner->TestTrue(TEXT("Main menu does not create a simulation HUD"),
                             controller->get_active_hud() == nullptr);
        TestRunner->TestTrue(TEXT("Main menu does not create a benchmark HUD"),
                             controller->get_benchmark_hud() == nullptr);
        camera->Destroy();
        controller->Destroy();
    }

    TEST_METHOD(UnpossessionClearsPendingShipRestore)
    {
        auto const world_result{ml::get_editor_world()};
        if (!TestRunner->TestTrue(TEXT("Editor world is available"), world_result.has_value())) {
            return;
        }
        auto* const controller{world_result.value()->SpawnActor<ASpaceGamePlayerController>()};
        auto const* const config{ml::load_default_level_config()};
        if (!TestRunner->TestNotNull(TEXT("Level config loads"), config)) {
            controller->Destroy();
            return;
        }
        auto* const ship{ml::spawn_player_ship(
            *world_result.value(), config->classes.player_ship_class, &config->player_ship)};
        controller->Possess(ship);
        FPlayerControllerTestAccess::prepare_completion(*controller);
        controller->UnPossess();
        TestRunner->TestTrue(TEXT("A lost ship is not required to close the modal"),
                             FPlayerControllerTestAccess::restore_context(*controller) ==
                                 EPlayerControlContext::None);
        ship->Destroy();
        controller->Destroy();
    }

    TEST_METHOD(UnboundShipCachesFlightModelForSimulationSpawn)
    {
        auto const world_result{ml::get_editor_world()};
        auto const* const config{ml::load_default_level_config()};
        if (!TestRunner->TestTrue(TEXT("Editor world is available"), world_result.has_value()) ||
            !TestRunner->TestTrue(TEXT("Level config loads"), IsValid(config))) {
            return;
        }

        auto* const ship{ml::spawn_player_ship(
            *world_result.value(), config->classes.player_ship_class, &config->player_ship)};
        auto profile{::ioj::sim::player::make_flight_model_profile(
            ::ioj::sim::player::FlightModelPreset::Fighter)};
        profile.config.translation.forward.passive_drag = 321.f;

        TestRunner->TestFalse(TEXT("New ship is not yet bound to native simulation"),
                              ship->has_simulation());
        TestRunner->TestTrue(
            TEXT("A valid profile can be selected before native simulation binding"),
            ship->set_flight_model_slot_profile(::ioj::sim::player::FlightModelSlot::Right,
                                                profile));
        ship->select_flight_model_slot(::ioj::sim::player::FlightModelSlot::Right);

        auto const spawn{ship->make_spawn_data()};
        auto const& cached{::ioj::sim::player::flight_model_profile(
            spawn.flight_models, ::ioj::sim::player::FlightModelSlot::Right)};
        TestRunner->TestTrue(
            TEXT("Pre-bind flight-model selection is included in native spawn data"),
            spawn.flight_models.initial_slot == ::ioj::sim::player::FlightModelSlot::Right &&
                cached == profile && ship->get_active_flight_model_profile() == profile);

        ship->Destroy();
    }

    TEST_METHOD(PossessionDuringCompletionDoesNotEnableShipInput)
    {
        auto const world_result{ml::get_editor_world()};
        auto const* const config{ml::load_default_level_config()};
        if (!TestRunner->TestTrue(TEXT("Editor world is available"), world_result.has_value()) ||
            !TestRunner->TestTrue(TEXT("Level config loads"), IsValid(config))) {
            return;
        }
        auto& world{*world_result.value()};
        auto* const controller{world.SpawnActor<ASpaceGamePlayerController>()};
        auto* const ship{
            ml::spawn_player_ship(world, config->classes.player_ship_class, &config->player_ship)};
        auto* const component{NewObject<UEnhancedInputComponent>(controller)};
        auto* const subsystem{NewObject<USandboxTestEnhancedInputSubsystem>(controller)};
        subsystem->initialise();
        auto const* const defaults{config->classes.player_controller_class.GetDefaultObject()};
        auto const* const property{
            FindFProperty<FStructProperty>(defaults->GetClass(), TEXT("input"))};
        auto const& input{*property->ContainerPtrToValuePtr<FSpaceShipControllerInputs>(defaults)};
        FPlayerControllerTestAccess::prepare_input(*controller, *component, *subsystem, input);
        FPlayerControllerTestAccess::prepare_completion(*controller);
        controller->Possess(ship);
        TestRunner->TestTrue(TEXT("Completion keeps gameplay control disabled"),
                             controller->get_active_control_context() ==
                                 EPlayerControlContext::None);
        TestRunner->TestFalse(TEXT("Completion does not install the ship mapping"),
                              subsystem->HasMappingContext(input.get_mapping_context()));
        TestRunner->TestTrue(TEXT("Replacement ship is restored when the modal closes"),
                             FPlayerControllerTestAccess::restore_context(*controller) ==
                                 EPlayerControlContext::Player);
        controller->UnPossess();
        FPlayerControllerTestAccess::shutdown_input(*controller);
        ship->Destroy();
        controller->Destroy();
    }
};
