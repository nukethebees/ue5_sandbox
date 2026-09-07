#include <SandboxTests/support/SimulationTestAssets.h>
#include <SandboxTests/support/test_setup.h>
#include <SandboxTests/support/TestActorSpawning.h>
#include <SandboxTests/support/TestEnhancedInputSubsystem.h>

#include <SpaceGame/input/ControlProfiles.h>
#include <SpaceGame/input/SpaceGameInputUserSettings.h>
#include <SpaceGame/ships/common/LaserFiringState.h>
#include <SpaceGame/ships/player/ShipControlContext.h>
#include <SpaceGame/ships/player/SpaceGamePlayerController.h>
#include <SpaceGame/ships/player/TestSpaceShip.h>
#include <SpaceGame/simulation/SpaceGameLevelConfig.h>
#include <SpaceGame/ui/main_menu/MainMenuGameMode.h>

#include <CQTest.h>
#include <Engine/Engine.h>
#include <Engine/LocalPlayer.h>
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

        auto const* const mapping_context{input->get_mapping_context()};
        if (!TestRunner->TestTrue(TEXT("Mapping context is configured"),
                                  IsValid(mapping_context))) {
            return;
        }

        TestRunner->TestTrue(
            TEXT("Mapping context belongs to SpaceGame"),
            mapping_context->GetOutermost()->GetName().StartsWith(TEXT("/SpaceGame/")));
        TestRunner->TestTrue(TEXT("Mapping contains movement"),
                             mapping_context->HasMappingForInputAction(input->move));
        TestRunner->TestTrue(TEXT("Mapping contains turning"),
                             mapping_context->HasMappingForInputAction(input->turn));
        TestRunner->TestTrue(
            TEXT("Mapping contains profile cycling"),
            mapping_context->HasMappingForInputAction(input->cycle_input_mapping_context));

        mapping_context->ForEachKeyMapping([this](FEnhancedActionKeyMapping const& mapping) {
            TestRunner->TestTrue(TEXT("Mapping contains a valid action"), IsValid(mapping.Action));
            TestRunner->TestTrue(TEXT("Mapping is player mappable"), mapping.IsPlayerMappable());
        });
    }

    TEST_METHOD(ControlProfilesRegisterAndCycle)
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
        auto* const mapping_context{input->get_mapping_context()};
        if (!TestRunner->TestTrue(TEXT("Mapping context is configured"),
                                  IsValid(mapping_context))) {
            return;
        }

        auto* const local_player{NewObject<ULocalPlayer>(GEngine)};
        auto* const settings{NewObject<ml::ioj::USpaceGameInputUserSettings>(local_player)};
        settings->Initialize(local_player);
        TestRunner->TestTrue(TEXT("All control profiles register"),
                             ml::ioj::register_control_profiles(*settings, *mapping_context));

        auto const profiles{ml::ioj::control_profile_definitions()};
        TestRunner->TestTrue(TEXT("Control profiles are configured"), !profiles.IsEmpty());
        for (auto const& profile : profiles) {
            auto const* const registered{settings->GetKeyProfileWithId(profile.id)};
            TestRunner->TestTrue(*FString::Printf(TEXT("Profile '%s' is registered"), *profile.id),
                                 IsValid(registered));
            if (IsValid(registered)) {
                TestRunner->TestTrue(
                    *FString::Printf(TEXT("Profile '%s' contains mappings"), *profile.id),
                    !registered->GetPlayerMappingRows().IsEmpty());
            }
        }

        for (int32 index{1}; index < profiles.Num(); ++index) {
            TestRunner->TestTrue(TEXT("Profile cycling succeeds"),
                                 ml::ioj::cycle_control_profile(*settings));
            TestRunner->TestEqual(TEXT("Profile cycling follows the configured order"),
                                  settings->GetActiveKeyProfileId(),
                                  profiles[index].id);
        }
        TestRunner->TestTrue(TEXT("Profile cycling wraps"),
                             ml::ioj::cycle_control_profile(*settings));
        TestRunner->TestEqual(TEXT("Profile cycling returns to default"),
                              settings->GetActiveKeyProfileId(),
                              profiles[0].id);
    }

    TEST_METHOD(InputResponseSettingsClampInvalidValues)
    {
        auto* const settings{NewObject<ml::ioj::USpaceGameInputUserSettings>()};
        settings->set_mouse_turn_sensitivity(-1.0f);
        settings->set_gamepad_turn_sensitivity(-1.0f);
        settings->set_gamepad_turn_dead_zone(-1.0f);
        settings->set_gamepad_move_dead_zone(2.0f);

        TestRunner->TestEqual(
            TEXT("Mouse sensitivity is non-negative"), settings->mouse_turn_sensitivity(), 0.0f);
        TestRunner->TestEqual(TEXT("Gamepad sensitivity is non-negative"),
                              settings->gamepad_turn_sensitivity(),
                              0.0f);
        TestRunner->TestEqual(
            TEXT("Turn dead zone is non-negative"), settings->gamepad_turn_dead_zone(), 0.0f);
        TestRunner->TestEqual(TEXT("Move dead zone leaves usable axis range"),
                              settings->gamepad_move_dead_zone(),
                              0.95f);
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
        input.mapping_context = mapping_context;
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
