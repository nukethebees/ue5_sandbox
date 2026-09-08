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
#include <SpaceGame/ui/main_menu/ControlChordCapture.h>
#include <SpaceGame/ui/main_menu/MainMenuGameMode.h>

#include <CQTest.h>
#include <Engine/Engine.h>
#include <Engine/LocalPlayer.h>
#include <EnhancedInputComponent.h>
#include <InputAction.h>
#include <InputMappingContext.h>
#include <Kismet/GameplayStatics.h>
#include <Misc/Guid.h>
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

        auto const* const chord_profile{settings->GetKeyProfileWithId(profiles.Last().id)};
        auto chorded_mapping_count{0};
        if (IsValid(chord_profile)) {
            for (auto const& row : chord_profile->GetPlayerMappingRows()) {
                for (auto const& mapping : row.Value.Mappings) {
                    if (settings->chord_mapping_for_mapping(profiles.Last().id, mapping) !=
                        nullptr) {
                        ++chorded_mapping_count;
                    }
                }
            }
        }
        TestRunner->TestTrue(TEXT("Chorded mappings resolve their activator keys"),
                             chorded_mapping_count >= 4);

        FPlayerMappableKeyProfileCreationArgs custom_arguments{};
        custom_arguments.ProfileStringIdentifier = TEXT("SpaceGame.Controls.Custom.ChordTest");
        custom_arguments.DisplayName = INVTEXT("Chord test");
        auto* const custom_chord_profile{
            settings->create_custom_key_profile(custom_arguments, profiles.Last().id)};
        auto custom_chorded_mapping_count{0};
        if (TestRunner->TestTrue(TEXT("Chord test custom profile is created"),
                                 IsValid(custom_chord_profile))) {
            for (auto const& row : custom_chord_profile->GetPlayerMappingRows()) {
                for (auto const& mapping : row.Value.Mappings) {
                    auto const* const chord_mapping{settings->chord_mapping_for_mapping(
                        custom_arguments.ProfileStringIdentifier, mapping)};
                    if (chord_mapping != nullptr) {
                        ++custom_chorded_mapping_count;
                        TestRunner->TestTrue(TEXT("Chord activator has a mapping name"),
                                             chord_mapping->GetMappingName().IsValid());
                        TestRunner->TestEqual(TEXT("Chord activator uses the same device"),
                                              chord_mapping->GetPrimaryDeviceType(),
                                              mapping.GetPrimaryDeviceType());
                    }
                }
            }
        }
        TestRunner->TestEqual(TEXT("Custom profile retains authored chord relationships"),
                              custom_chorded_mapping_count,
                              chorded_mapping_count);
    }

    TEST_METHOD(ControlProfileOverridesMatchTheirSourceContexts)
    {
        auto* const generated{LoadObject<UInputMappingContext>(
            nullptr, TEXT("/SpaceGame/Input/SpaceShip/IMC_SpaceShip_Base.IMC_SpaceShip_Base"))};
        TArray<FString> const source_paths{
            TEXT("/SpaceGame/Input/SpaceShip/IMC_space_ship_twinstick_aim_move."
                 "IMC_space_ship_twinstick_aim_move"),
            TEXT("/SpaceGame/Input/SpaceShip/IMC_space_ship_twinstick_move_aim."
                 "IMC_space_ship_twinstick_move_aim"),
            TEXT("/SpaceGame/Input/SpaceShip/IMC_space_ship_twinstick_z-roll_aim."
                 "IMC_space_ship_twinstick_z-roll_aim"),
        };
        if (!TestRunner->TestTrue(TEXT("Generated mapping context loads"), IsValid(generated))) {
            return;
        }

        auto const profiles{ml::ioj::control_profile_definitions()};
        for (int32 source_index{}; source_index < source_paths.Num(); ++source_index) {
            auto* const source{
                LoadObject<UInputMappingContext>(nullptr, *source_paths[source_index])};
            if (!TestRunner->TestTrue(
                    *FString::Printf(TEXT("Source mapping context %d loads"), source_index),
                    IsValid(source))) {
                continue;
            }

            auto const& expected{source->GetMappings()};
            if (source_index == 2) {
                auto vertical_move_mappings{0};
                auto move_mappings{0};
                for (auto const& mapping : expected) {
                    if (!IsValid(mapping.Action)) {
                        continue;
                    }
                    vertical_move_mappings +=
                        mapping.Action->GetName() == TEXT("IA_ship_vertical_move") ? 1 : 0;
                    move_mappings += mapping.Action->GetName() == TEXT("IA_ship_move") ? 1 : 0;
                }
                TestRunner->TestEqual(TEXT("Z/Roll/Aim has keyboard and gamepad vertical input"),
                                      vertical_move_mappings,
                                      3);
                TestRunner->TestEqual(
                    TEXT("Z/Roll/Aim does not apply unchorded planar movement"), move_mappings, 0);
            }
            auto const& actual{generated->GetMappingsForProfile(profiles[source_index + 1].id)};
            TestRunner->TestEqual(
                *FString::Printf(TEXT("Profile %d has the source mapping count"), source_index),
                actual.Num(),
                expected.Num());
            TArray<int32> unmatched_actual;
            unmatched_actual.Reserve(actual.Num());
            for (int32 actual_index{}; actual_index < actual.Num(); ++actual_index) {
                unmatched_actual.Add(actual_index);
            }
            for (int32 expected_index{}; expected_index < expected.Num(); ++expected_index) {
                auto const unmatched_index{unmatched_actual.IndexOfByPredicate(
                    [&actual, &expected, expected_index](int32 const actual_index) {
                        if (actual[actual_index].Action != expected[expected_index].Action ||
                            actual[actual_index].Key != expected[expected_index].Key ||
                            actual[actual_index].Triggers.Num() !=
                                expected[expected_index].Triggers.Num()) {
                            return false;
                        }
                        for (int32 trigger_index{};
                             trigger_index < actual[actual_index].Triggers.Num();
                             ++trigger_index) {
                            if (actual[actual_index].Triggers[trigger_index]->GetClass() !=
                                expected[expected_index].Triggers[trigger_index]->GetClass()) {
                                return false;
                            }
                        }
                        return true;
                    })};
                auto const found{unmatched_index != INDEX_NONE};
                TestRunner->TestTrue(
                    *FString::Printf(TEXT("Profile %d source mapping %d is present"),
                                     source_index,
                                     expected_index),
                    found);
                if (found) {
                    unmatched_actual.RemoveAtSwap(unmatched_index, EAllowShrinking::No);
                }
            }
        }
    }

    TEST_METHOD(CustomControlProfileDeletionRemovesTheSavedProfile)
    {
        auto const* const config{ml::load_default_level_config()};
        auto const* const controller_default{
            IsValid(config) && IsValid(config->classes.player_controller_class)
                ? config->classes.player_controller_class.GetDefaultObject()
                : nullptr};
        auto const* const input_property{
            controller_default != nullptr
                ? FindFProperty<FStructProperty>(controller_default->GetClass(), TEXT("input"))
                : nullptr};
        if (!TestRunner->TestTrue(TEXT("Controller input property is available"),
                                  input_property != nullptr)) {
            return;
        }
        auto const* const input{
            input_property->ContainerPtrToValuePtr<FSpaceShipControllerInputs>(controller_default)};
        auto* const mapping_context{input->get_mapping_context()};
        auto* const local_player{NewObject<ULocalPlayer>(GEngine)};
        auto* const settings{NewObject<ml::ioj::USpaceGameInputUserSettings>(local_player)};
        settings->Initialize(local_player);
        if (!TestRunner->TestTrue(TEXT("Control profiles register"),
                                  IsValid(mapping_context) && ml::ioj::register_control_profiles(
                                                                  *settings, *mapping_context))) {
            return;
        }

        auto const custom_id{FString::Printf(TEXT("SpaceGame.Controls.Custom.Test%s"),
                                             *FGuid::NewGuid().ToString())};
        FPlayerMappableKeyProfileCreationArgs arguments{};
        arguments.ProfileStringIdentifier = custom_id;
        arguments.DisplayName = INVTEXT("Deletion test");
        arguments.bSetAsCurrentProfile = false;
        auto* const custom{settings->create_custom_key_profile(
            arguments, ml::ioj::control_profile_definitions()[0].id)};
        if (!TestRunner->TestTrue(TEXT("Custom profile is created"), IsValid(custom)) ||
            !TestRunner->TestTrue(TEXT("Custom profile becomes active"),
                                  settings->SetActiveKeyProfile(custom_id))) {
            return;
        }
        TestRunner->TestEqual(TEXT("Active custom profile uses its source IMC profile"),
                              custom->GetProfileIdString(),
                              ml::ioj::control_profile_definitions()[0].id);

        FMapPlayerKeyArgs clear_arguments{};
        FKey cleared_default_key;
        for (auto const& row : custom->GetPlayerMappingRows()) {
            for (auto const& mapping : row.Value.Mappings) {
                if (mapping.GetCurrentKey() != FKey{}) {
                    clear_arguments.MappingName = mapping.GetMappingName();
                    clear_arguments.Slot = mapping.GetSlot();
                    clear_arguments.HardwareDeviceId =
                        mapping.GetHardwareDeviceId().HardwareDeviceIdentifier;
                    clear_arguments.ProfileIdString = custom_id;
                    cleared_default_key = mapping.GetDefaultKey();
                    break;
                }
            }
            if (clear_arguments.MappingName != NAME_None) {
                break;
            }
        }
        if (TestRunner->TestTrue(TEXT("Custom profile has a binding to clear"),
                                 clear_arguments.MappingName != NAME_None)) {
            clear_arguments.NewKey = FKey{};
            FGameplayTagContainer failure_reason;
            settings->MapPlayerKey(clear_arguments, failure_reason);
            auto const* const cleared{custom->FindKeyMapping(clear_arguments)};
            TestRunner->TestTrue(TEXT("Binding clear succeeds"), failure_reason.IsEmpty());
            TestRunner->TestTrue(TEXT("Cleared binding is unbound"),
                                 cleared != nullptr && cleared->GetCurrentKey() == FKey{});

            FGameplayTagContainer reset_failure_reason;
            settings->UnMapPlayerKey(clear_arguments, reset_failure_reason);
            auto const* const reset{custom->FindKeyMapping(clear_arguments)};
            TestRunner->TestTrue(TEXT("Binding reset succeeds"), reset_failure_reason.IsEmpty());
            TestRunner->TestTrue(TEXT("Binding reset restores its default"),
                                 reset != nullptr && reset->GetCurrentKey() == cleared_default_key);
        }

        auto const slot_name{
            FString::Printf(TEXT("ControlProfileDeletionTest_%s"), *FGuid::NewGuid().ToString())};
        auto const user_index{0};
        TestRunner->TestTrue(TEXT("Custom profile saves before deletion"),
                             UGameplayStatics::SaveGameToSlot(settings, slot_name, user_index));
        auto* const loaded_before_delete{Cast<ml::ioj::USpaceGameInputUserSettings>(
            UGameplayStatics::LoadGameFromSlot(slot_name, user_index))};
        TestRunner->TestTrue(TEXT("Custom profile reloads before deletion"),
                             IsValid(loaded_before_delete));
        if (IsValid(loaded_before_delete)) {
            TestRunner->TestEqual(TEXT("Custom profile source persists"),
                                  loaded_before_delete->custom_key_profile_source_id(custom_id),
                                  ml::ioj::control_profile_definitions()[0].id);
            TestRunner->TestEqual(
                TEXT("Custom profile name persists"),
                loaded_before_delete->custom_key_profile_display_name(custom_id).ToString(),
                FString{TEXT("Deletion test")});
            auto const* const loaded_custom{loaded_before_delete->GetKeyProfileWithId(custom_id)};
            TestRunner->TestTrue(TEXT("Reloaded custom profile is present"),
                                 IsValid(loaded_custom));
            if (IsValid(loaded_custom)) {
                TestRunner->TestEqual(
                    TEXT("Reloaded active custom profile retains its source IMC profile"),
                    loaded_custom->GetProfileIdString(),
                    ml::ioj::control_profile_definitions()[0].id);
            }
        }

        TestRunner->TestTrue(TEXT("Custom profile deletion succeeds"),
                             settings->delete_custom_key_profile(custom_id));
        TestRunner->TestTrue(TEXT("Deleted profile is absent"),
                             settings->GetKeyProfileWithId(custom_id) == nullptr);
        TestRunner->TestEqual(TEXT("Deletion activates the default profile"),
                              settings->GetActiveKeyProfileId(),
                              ml::ioj::control_profile_definitions()[0].id);
        TestRunner->TestEqual(TEXT("Deleted custom profile restores its unique identity"),
                              custom->GetProfileIdString(),
                              custom_id);

        TestRunner->TestTrue(TEXT("Settings save after deletion"),
                             UGameplayStatics::SaveGameToSlot(settings, slot_name, user_index));
        auto* const loaded{Cast<ml::ioj::USpaceGameInputUserSettings>(
            UGameplayStatics::LoadGameFromSlot(slot_name, user_index))};
        TestRunner->TestTrue(TEXT("Settings reload after deletion"), IsValid(loaded));
        if (IsValid(loaded)) {
            TestRunner->TestTrue(TEXT("Deleted profile remains absent after reload"),
                                 loaded->GetKeyProfileWithId(custom_id) == nullptr);
        }
        UGameplayStatics::DeleteGameInSlot(slot_name, user_index);
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

    TEST_METHOD(ControlChordCapturePreservesOrderedCandidate)
    {
        ml::ioj::FControlChordCapture capture;
        TestRunner->TestFalse(TEXT("Non-holdable input cannot start a chord"),
                              capture.accept(EKeys::MouseScrollUp, false));
        TestRunner->TestFalse(TEXT("Chord remains empty"), capture.held_key().IsValid());

        TestRunner->TestFalse(TEXT("First held input starts capture"),
                              capture.accept(EKeys::ThumbMouseButton, true));
        TestRunner->TestEqual(
            TEXT("First input is held"), capture.held_key(), EKeys::ThumbMouseButton);
        capture.release(EKeys::ThumbMouseButton);
        TestRunner->TestFalse(TEXT("Released activator cancels partial capture"),
                              capture.held_key().IsValid());

        capture.accept(EKeys::ThumbMouseButton, true);
        TestRunner->TestTrue(TEXT("Second input completes capture"),
                             capture.accept(EKeys::W, true));
        TestRunner->TestEqual(TEXT("Earlier input is the activator"),
                              capture.activator_key(),
                              EKeys::ThumbMouseButton);
        TestRunner->TestEqual(TEXT("Last input is the action"), capture.action_key(), EKeys::W);

        capture.release(EKeys::W);
        capture.release(EKeys::ThumbMouseButton);
        TestRunner->TestTrue(TEXT("Completed candidate survives releases"), capture.is_complete());
        TestRunner->TestFalse(TEXT("Completed candidate is frozen"),
                              capture.accept(EKeys::D, true));
        TestRunner->TestEqual(
            TEXT("Frozen action remains unchanged"), capture.action_key(), EKeys::W);

        capture.clear();
        TestRunner->TestFalse(TEXT("Clear removes the candidate"), capture.is_complete());
        TestRunner->TestFalse(TEXT("Clear removes the held input"), capture.held_key().IsValid());
    }

    TEST_METHOD(NewSamplingSessionStartsWithNeutralControl)
    {
        ml::test_space_ship::Simulation simulation;
        simulation.start_sampling();
        simulation.set_ship_1d_control_y(1.0f);
        simulation.stop_sampling();
        TestRunner->TestEqual(TEXT("Released sample retains the committed direction"),
                              simulation.target_local_planar_velocity_scale,
                              FVector2D{0.0f, 1.0f});

        simulation.start_sampling();
        TestRunner->TestEqual(TEXT("New sample starts from neutral"),
                              simulation.target_local_planar_velocity_scale,
                              FVector2D::ZeroVector);

        simulation.set_ship_1d_control_x(1.0f);
        simulation.start_sampling();
        TestRunner->TestEqual(TEXT("Starting a second axis does not reset the active sample"),
                              simulation.target_local_planar_velocity_scale,
                              FVector2D{1.0f, 0.0f});
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
