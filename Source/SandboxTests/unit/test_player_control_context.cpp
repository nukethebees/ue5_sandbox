#include <SandboxTests/support/SimulationTestAssets.h>
#include <SandboxTests/support/test_setup.h>
#include <SandboxTests/support/TestActorSpawning.h>
#include <SandboxTests/support/TestEnhancedInputSubsystem.h>

#include <SpaceGame/presentation/TestBatchGameUiData.h>
#include <SpaceGame/presentation/widgets/BenchmarkHudWidget.h>
#include <SpaceGame/ships/common/LaserFiringState.h>
#include <SpaceGame/ships/player/ObserverControlContext.h>
#include <SpaceGame/ships/player/ShipControlContext.h>
#include <SpaceGame/ships/player/SpaceGamePlayerController.h>
#include <SpaceGame/ships/player/TestSpaceShip.h>
#include <SpaceGame/simulation/SpaceGameLevelConfig.h>
#include <SpaceGame/ui/main_menu/MainMenuGameMode.h>

#include <Camera/CameraActor.h>
#include <CQTest.h>
#include <EnhancedInputComponent.h>
#include <InputAction.h>
#include <InputMappingContext.h>
#include <UObject/UnrealType.h>

TEST_CLASS(PlayerControlContext, "Sandbox.UnitTests")
{
    TEST_METHOD(ProductionModesUseCanonicalController)
    {
        auto* const config{
            LoadObject<USpaceGameLevelConfig>(nullptr,
                                              TEXT("/SpaceGame/Levels/DA_GameRuntimeLevelConfig."
                                                   "DA_GameRuntimeLevelConfig"))};
        if (!TestRunner->TestTrue(TEXT("Runtime level config loads"), IsValid(config)) ||
            !TestRunner->TestTrue(TEXT("Runtime controller class is configured"),
                                  config && IsValid(config->classes.player_controller_class))) {
            return;
        }

        auto* const controller_class{config->classes.player_controller_class.Get()};
        TestRunner->TestTrue(
            TEXT("Runtime config uses the canonical controller type"),
            controller_class->IsChildOf(ASpaceGamePlayerController::StaticClass()));
        TestRunner->TestTrue(
            TEXT("Runtime controller Blueprint belongs to SpaceGame"),
            controller_class->GetOutermost()->GetName().StartsWith(TEXT("/SpaceGame/")));

        auto* const runtime_mode_class{LoadClass<AGameModeBase>(
            nullptr, TEXT("/Game/GameModes/BP_SpaceShipGameMode.BP_SpaceShipGameMode_C"))};
        auto const* const runtime_mode{IsValid(runtime_mode_class)
                                           ? runtime_mode_class->GetDefaultObject<AGameModeBase>()
                                           : nullptr};
        TestRunner->TestTrue(TEXT("Runtime game mode uses the canonical controller Blueprint"),
                             IsValid(runtime_mode) &&
                                 runtime_mode->PlayerControllerClass == controller_class);

        auto const* const main_menu_mode{GetDefault<ml::ioj::AMainMenuGameMode>()};
        TestRunner->TestTrue(TEXT("Main menu uses the canonical controller type"),
                             IsValid(main_menu_mode->PlayerControllerClass) &&
                                 main_menu_mode->PlayerControllerClass->IsChildOf(
                                     ASpaceGamePlayerController::StaticClass()));
        TestRunner->TestTrue(TEXT("Main menu uses the canonical controller Blueprint"),
                             main_menu_mode->PlayerControllerClass == controller_class);
    }

    TEST_METHOD(ConfiguredShipMappingsAreCompleteAndPluginOwned)
    {
        auto const* const config{ml::load_default_level_config()};
        if (!TestRunner->TestTrue(TEXT("Level config loads"), IsValid(config)) ||
            !TestRunner->TestTrue(TEXT("Player controller class is configured"),
                                  config && IsValid(config->classes.player_controller_class))) {
            return;
        }

        auto const* const controller_default{
            config->classes.player_controller_class.GetDefaultObject()};
        auto const* const input_property{
            FindFProperty<FStructProperty>(controller_default->GetClass(), TEXT("input"))};
        if (!TestRunner->TestTrue(TEXT("Controller input property is available"),
                                  input_property != nullptr)) {
            return;
        }

        auto const* const input{
            input_property->ContainerPtrToValuePtr<FSpaceShipControllerInputs>(controller_default)};
        if (!TestRunner->TestTrue(TEXT("Turn action is configured"), IsValid(input->turn)) ||
            !TestRunner->TestTrue(TEXT("Mapping cycle action is configured"),
                                  IsValid(input->cycle_input_mapping_context))) {
            return;
        }

        auto const mapping_context_count{input->mapping_contexts.Num()};
        for (int32 mapping_context_index{0}; mapping_context_index < mapping_context_count;
             ++mapping_context_index) {
            auto const* const mapping_context{input->mapping_contexts[mapping_context_index]};
            auto const context_label{
                FString::Printf(TEXT("Mapping context %d is configured"), mapping_context_index)};
            if (!TestRunner->TestTrue(*context_label, IsValid(mapping_context))) {
                continue;
            }

            TestRunner->TestTrue(
                *FString::Printf(TEXT("Mapping context %d belongs to SpaceGame"),
                                 mapping_context_index),
                mapping_context->GetOutermost()->GetName().StartsWith(TEXT("/SpaceGame/")));
            TestRunner->TestTrue(*FString::Printf(TEXT("Mapping context %d contains turning"),
                                                  mapping_context_index),
                                 mapping_context->HasMappingForInputAction(input->turn));
            TestRunner->TestTrue(
                *FString::Printf(TEXT("Mapping context %d contains context cycling"),
                                 mapping_context_index),
                mapping_context->HasMappingForInputAction(input->cycle_input_mapping_context));

            auto const& mappings{mapping_context->GetMappings()};
            for (auto const& mapping : mappings) {
                TestRunner->TestTrue(
                    *FString::Printf(TEXT("Mapping context %d contains a valid action"),
                                     mapping_context_index),
                    IsValid(mapping.Action));
                if (IsValid(mapping.Action)) {
                    TestRunner->TestTrue(
                        *FString::Printf(TEXT("Mapping context %d action belongs to SpaceGame"),
                                         mapping_context_index),
                        mapping.Action->GetOutermost()->GetName().StartsWith(TEXT("/SpaceGame/")));
                }
            }
        }

        if (!TestRunner->TestTrue(
                TEXT("Initial mapping index is valid"),
                input->mapping_contexts.IsValidIndex(input->initial_mapping_context_index)) ||
            !TestRunner->TestTrue(
                TEXT("Initial mapping context is configured"),
                input->mapping_contexts.IsValidIndex(input->initial_mapping_context_index) &&
                    IsValid(input->mapping_contexts[input->initial_mapping_context_index]))) {
            return;
        }

        auto const* const mapping_context{
            input->mapping_contexts[input->initial_mapping_context_index]};
        TestRunner->TestTrue(TEXT("Initial mapping contains movement"),
                             mapping_context->HasMappingForInputAction(input->move));
        TestRunner->TestTrue(TEXT("Initial mapping contains turning"),
                             mapping_context->HasMappingForInputAction(input->turn));
    }

    TEST_METHOD(ConfiguredObserverAndBenchmarkMappingsAreComplete)
    {
        auto const* const config{ml::load_default_level_config()};
        if (!TestRunner->TestTrue(TEXT("Level config loads"), IsValid(config)) ||
            !TestRunner->TestTrue(TEXT("Player controller class is configured"),
                                  config && IsValid(config->classes.player_controller_class))) {
            return;
        }

        auto const* const controller_default{
            config->classes.player_controller_class.GetDefaultObject()};
        auto const* const observer_property{
            FindFProperty<FStructProperty>(controller_default->GetClass(), TEXT("observer_input"))};
        auto const* const benchmark_property{FindFProperty<FStructProperty>(
            controller_default->GetClass(), TEXT("benchmark_input"))};
        if (!TestRunner->TestTrue(TEXT("Observer input property is available"),
                                  observer_property != nullptr) ||
            !TestRunner->TestTrue(TEXT("Benchmark input property is available"),
                                  benchmark_property != nullptr)) {
            return;
        }

        auto const* const observer{
            observer_property->ContainerPtrToValuePtr<FObserverControlInputs>(controller_default)};
        auto const* const benchmark{
            benchmark_property->ContainerPtrToValuePtr<FBenchmarkControlInputs>(
                controller_default)};
        auto observer_valid{TestRunner->TestTrue(TEXT("Observer mapping is configured"),
                                                 IsValid(observer->mapping_context))};
        observer_valid &= TestRunner->TestTrue(TEXT("Observer move action is configured"),
                                               IsValid(observer->move));
        observer_valid &= TestRunner->TestTrue(TEXT("Observer vertical move action is configured"),
                                               IsValid(observer->vertical_move));
        observer_valid &= TestRunner->TestTrue(TEXT("Observer look action is configured"),
                                               IsValid(observer->look));
        observer_valid &= TestRunner->TestTrue(TEXT("Observer engage-look action is configured"),
                                               IsValid(observer->engage_look));
        observer_valid &= TestRunner->TestTrue(TEXT("Observer speed action is configured"),
                                               IsValid(observer->adjust_speed));
        observer_valid &= TestRunner->TestTrue(TEXT("Observer boost action is configured"),
                                               IsValid(observer->boost));
        auto benchmark_valid{TestRunner->TestTrue(TEXT("Benchmark mapping is configured"),
                                                  IsValid(benchmark->mapping_context))};
        benchmark_valid &= TestRunner->TestTrue(TEXT("Benchmark exit action is configured"),
                                                IsValid(benchmark->exit));
        if (!observer_valid || !benchmark_valid) {
            return;
        }

        TestRunner->TestTrue(TEXT("Observer mapping contains planar movement"),
                             observer->mapping_context->HasMappingForInputAction(observer->move));
        TestRunner->TestTrue(
            TEXT("Observer mapping contains vertical movement"),
            observer->mapping_context->HasMappingForInputAction(observer->vertical_move));
        TestRunner->TestTrue(TEXT("Observer mapping contains mouse look"),
                             observer->mapping_context->HasMappingForInputAction(observer->look));
        TestRunner->TestTrue(
            TEXT("Observer mapping contains speed adjustment"),
            observer->mapping_context->HasMappingForInputAction(observer->adjust_speed));
        TestRunner->TestTrue(TEXT("Observer mapping contains boost"),
                             observer->mapping_context->HasMappingForInputAction(observer->boost));
        TestRunner->TestTrue(
            TEXT("Benchmark mapping contains only its exit action"),
            benchmark->mapping_context->GetMappings().Num() == 1 &&
                benchmark->mapping_context->HasMappingForInputAction(benchmark->exit));
        TestRunner->TestTrue(
            TEXT("Observer mapping belongs to SpaceGame"),
            observer->mapping_context->GetOutermost()->GetName().StartsWith(TEXT("/SpaceGame/")));
        TestRunner->TestTrue(
            TEXT("Benchmark mapping belongs to SpaceGame"),
            benchmark->mapping_context->GetOutermost()->GetName().StartsWith(TEXT("/SpaceGame/")));

        auto* const ui_data{ml::test_batch_game_ui_data::get_data_asset()};
        TestRunner->TestTrue(TEXT("Benchmark HUD class is configured"),
                             IsValid(ui_data) &&
                                 IsValid(ui_data->get_widget_class<UBenchmarkHudWidget>()));
    }

    TEST_METHOD(ObserverBindUnbindOwnsMappingsAndHandlers)
    {
        auto const world_result{ml::get_editor_world()};
        if (!TestRunner->TestTrue(TEXT("Editor world is available"), world_result.has_value())) {
            return;
        }

        auto& world{*world_result.value()};
        auto* const controller{world.SpawnActor<ASpaceGamePlayerController>(
            ASpaceGamePlayerController::StaticClass())};
        auto* const camera{world.SpawnActor<ACameraActor>()};
        auto* const input_component{NewObject<UEnhancedInputComponent>(controller)};
        auto* const input_subsystem{NewObject<USandboxTestEnhancedInputSubsystem>(controller)};
        auto* const mapping_context{NewObject<UInputMappingContext>(controller)};
        auto* const action{NewObject<UInputAction>(controller)};
        if (!TestRunner->TestTrue(TEXT("Controller is spawned"), IsValid(controller)) ||
            !TestRunner->TestTrue(TEXT("Camera is spawned"), IsValid(camera))) {
            return;
        }
        input_subsystem->initialise();

        FObserverControlInputs input;
        input.mapping_context = mapping_context;
        input.move = action;
        input.vertical_move = action;
        input.look = action;
        input.engage_look = action;
        input.adjust_speed = action;
        input.boost = action;

        FObserverControlContext context;
        TestRunner->TestTrue(
            TEXT("Observer context initializes"),
            context.initialise(*controller, *input_component, *input_subsystem, input));
        context.set_camera(camera);
        TestRunner->TestTrue(TEXT("Observer context binds"), context.bind());
        TestRunner->TestTrue(TEXT("Observer context reports bound"), context.is_bound());
        TestRunner->TestTrue(TEXT("Observer mapping is active"),
                             input_subsystem->HasMappingContext(mapping_context));

        auto const bindings_after_bind{input_component->GetActionEventBindings().Num()};
        TestRunner->TestTrue(TEXT("Repeated bind is idempotent"), context.bind());
        TestRunner->TestEqual(TEXT("Repeated bind does not duplicate handlers"),
                              input_component->GetActionEventBindings().Num(),
                              bindings_after_bind);

        context.unbind();
        TestRunner->TestFalse(TEXT("Observer context reports unbound"), context.is_bound());
        TestRunner->TestFalse(TEXT("Observer mapping is removed"),
                              input_subsystem->HasMappingContext(mapping_context));
        TestRunner->TestEqual(TEXT("Observer handlers are removed"),
                              input_component->GetActionEventBindings().Num(),
                              0);

        context.shutdown();
        camera->Destroy();
        controller->Destroy();
    }

    TEST_METHOD(ShipBindUnbindOwnsMappingsAndHandlers)
    {
        auto const world_result{ml::get_editor_world()};
        if (!TestRunner->TestTrue(TEXT("Editor world is available"), world_result.has_value())) {
            return;
        }

        auto const* const config{ml::load_default_level_config()};
        if (!TestRunner->TestTrue(TEXT("Level config loads"), IsValid(config))) {
            return;
        }

        auto& world{*world_result.value()};
        auto* const controller{world.SpawnActorDeferred<ASpaceGamePlayerController>(
            config->classes.player_controller_class, FTransform::Identity)};
        auto* const ship{
            ml::spawn_player_ship(world, config->classes.player_ship_class, &config->player_ship)};
        if (!TestRunner->TestTrue(TEXT("Controller is spawned"), IsValid(controller)) ||
            !TestRunner->TestTrue(TEXT("Player ship is spawned"), IsValid(ship))) {
            return;
        }

        ml::test_space_ship::Simulation simulation;
        ship->bind_simulation(simulation);

        auto* const input_component{NewObject<UEnhancedInputComponent>(controller)};
        auto* const input_subsystem{NewObject<USandboxTestEnhancedInputSubsystem>(controller)};
        auto* const mapping_context{NewObject<UInputMappingContext>(controller)};
        auto* const move_action{NewObject<UInputAction>(controller)};
        auto* const sentinel_action{NewObject<UInputAction>(controller)};
        input_subsystem->initialise();

        FSpaceShipControllerInputs input;
        input.mapping_contexts.Add(mapping_context);
        input.move = move_action;
        input.turn = move_action;
        input.fire_laser = move_action;
        input.boost = move_action;
        input.brake = move_action;
        input.roll = move_action;
        input.cycle_next_fire_rate = move_action;
        input.cycle_prev_fire_rate = move_action;
        input.cycle_input_mapping_context = move_action;
        input.lateral_move = move_action;
        input.vertical_move = move_action;
        input.sample_and_hold = move_action;
        input.ship_2d_control = move_action;
        input.ship_1d_control_x = move_action;
        input.ship_1d_control_y = move_action;
        input.cycle_next_control_mode = move_action;
        input.cycle_previous_control_mode = move_action;

        auto& sentinel_binding{input_component->BindActionValueLambda(
            sentinel_action, ETriggerEvent::Started, [](FInputActionValue const&) {})};
        auto const sentinel_handle{sentinel_binding.GetHandle()};

        FShipControlContext context;
        TestRunner->TestTrue(
            TEXT("Ship context initializes"),
            context.initialise(*controller, *input_component, *input_subsystem, input));
        context.set_ship(ship);
        TestRunner->TestTrue(TEXT("Ship context binds"), context.bind());
        TestRunner->TestTrue(TEXT("Ship context reports bound"), context.is_bound());
        TestRunner->TestTrue(TEXT("Ship mapping is active"),
                             input_subsystem->HasMappingContext(mapping_context));

        auto const bindings_after_bind{input_component->GetActionEventBindings().Num()};
        TestRunner->TestTrue(TEXT("Repeated bind is idempotent"), context.bind());
        TestRunner->TestEqual(TEXT("Repeated bind does not duplicate handlers"),
                              input_component->GetActionEventBindings().Num(),
                              bindings_after_bind);

        ship->set_move_input(FVector2D{0.5f, -0.25f});
        ship->turn(FVector2D{0.25f, 0.75f});
        ship->start_fire_laser();

        context.unbind();

        TestRunner->TestFalse(TEXT("Ship context reports unbound"), context.is_bound());
        TestRunner->TestFalse(TEXT("Ship mapping is removed"),
                              input_subsystem->HasMappingContext(mapping_context));
        TestRunner->TestEqual(TEXT("Only the unrelated handler remains"),
                              input_component->GetActionEventBindings().Num(),
                              1);
        TestRunner->TestTrue(
            TEXT("Unrelated handler is preserved"),
            input_component->GetActionEventBindings().ContainsByPredicate(
                [sentinel_handle](TUniquePtr<FEnhancedInputActionEventBinding> const& binding) {
                    return binding->GetHandle() == sentinel_handle;
                }));
        TestRunner->TestTrue(TEXT("Movement is neutralized"),
                             ship->get_move_input().IsNearlyZero());
        TestRunner->TestTrue(TEXT("Turning is neutralized"), ship->get_turn_input().IsNearlyZero());
        TestRunner->TestTrue(TEXT("Laser firing is stopped"),
                             ship->get_laser_firing_mode() == ELaserFiringState::idle);

        context.shutdown();
        ship->Destroy();
        controller->Destroy();
    }
};
