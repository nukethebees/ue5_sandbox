#include <SandboxTests/support/SimulationTestAssets.h>
#include <SandboxTests/support/test_setup.h>
#include <SandboxTests/support/TestActorSpawning.h>
#include <SandboxTests/support/TestEnhancedInputSubsystem.h>

#include <SpaceGame/ships/common/LaserFiringState.h>
#include <SpaceGame/ships/player/ShipControlContext.h>
#include <SpaceGame/ships/player/TestSpaceShip.h>
#include <SpaceGame/ships/player/TestSpaceShipController.h>
#include <SpaceGame/simulation/SpaceGameLevelConfig.h>

#include <CQTest.h>
#include <EnhancedInputComponent.h>
#include <InputAction.h>
#include <InputMappingContext.h>
#include <UObject/UnrealType.h>

TEST_CLASS(PlayerControlContext, "Sandbox.UnitTests")
{
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
        auto* const controller{world.SpawnActorDeferred<ATestSpaceShipController>(
            config->classes.player_controller_class, FTransform::Identity)};
        auto* const ship{
            ml::spawn_player_ship(world, config->classes.player_ship_class, &config->player_ship)};
        if (!TestRunner->TestTrue(TEXT("Controller is spawned"), IsValid(controller)) ||
            !TestRunner->TestTrue(TEXT("Player ship is spawned"), IsValid(ship))) {
            return;
        }

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
        input.fire_bomb = move_action;
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
