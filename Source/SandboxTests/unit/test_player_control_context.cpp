#include <ioj/sim/testing/player_sim_test_access.h>
#include <SandboxTests/support/SimulationTestAssets.h>
#include <SandboxTests/support/test_setup.h>
#include <SandboxTests/support/TestActorSpawning.h>
#include <SandboxTests/support/TestEnhancedInputSubsystem.h>
#include <SpaceGamePresentation/integration/TransformConversion.h>

#include <ioj/sim/agent_accessor.h>
#include <ioj/sim/combat_events.h>
#include <ioj/sim/entity_ledger.h>
#include <ioj/sim/lasers/sim.h>
#include <ioj/sim/player/laser_firing_state.h>
#include <ioj/sim/sim_clock.h>
#include <ioj/sim/spatial_query_manager.h>
#include <SpaceGame/input/ControlBindingMetadata.h>
#include <SpaceGame/input/ControlProfiles.h>
#include <SpaceGame/input/SpaceGameInputUserSettings.h>
#include <SpaceGame/presentation/TestBatchGameUiData.h>
#include <SpaceGame/ships/player/ObserverControlContext.h>
#include <SpaceGame/ships/player/ShipControlContext.h>
#include <SpaceGame/ships/player/SpaceGamePlayerController.h>
#include <SpaceGame/ships/player/TestSpaceShip.h>
#include <SpaceGame/simulation/SpaceGameLevelConfig.h>
#include <SpaceGame/ui/main_menu/ControlChordCapture.h>
#include <SpaceGame/ui/main_menu/MainMenuGameMode.h>
#include <SpaceGamePresentation/presentation/widgets/BenchmarkHudWidget.h>

#include <Camera/CameraActor.h>
#include <CQTest.h>
#include <Engine/Engine.h>
#include <Engine/LocalPlayer.h>
#include <EnhancedInputComponent.h>
#include <InputAction.h>
#include <InputMappingContext.h>
#include <InputModifiers.h>
#include <Kismet/GameplayStatics.h>
#include <Misc/Guid.h>
#include <PlayerMappableKeySettings.h>
#include <UObject/UnrealType.h>

struct FShipControlContextTestAccess {
    static void
        start_throttle(FShipControlContext& context, float const input, double const time_seconds) {
        context.start_throttle_at(input, time_seconds);
    }

    static void set_throttle(FShipControlContext& context, float const input) {
        context.set_throttle_value(input);
    }

    static void stop_throttle(FShipControlContext& context, double const time_seconds) {
        context.stop_throttle_at(time_seconds);
    }

    static void start_brake(FShipControlContext& context, double const time_seconds) {
        context.start_brake_at(time_seconds);
    }

    static void stop_brake(FShipControlContext& context, double const time_seconds) {
        context.stop_brake_at(time_seconds);
    }

    static void start_boost(FShipControlContext& context) { context.start_boost(); }

    static void stop_boost(FShipControlContext& context) { context.stop_boost(); }

    static void select_flight_model_up(FShipControlContext& context) {
        context.select_flight_model_up();
    }

    static void select_flight_model_right(FShipControlContext& context) {
        context.select_flight_model_right();
    }

    static void select_flight_model_down(FShipControlContext& context) {
        context.select_flight_model_down();
    }

    static void select_flight_model_left(FShipControlContext& context) {
        context.select_flight_model_left();
    }

    static auto throttle_press_active(FShipControlContext const& context) -> bool {
        return context.throttle_press_active_;
    }

    static auto brake_press_active(FShipControlContext const& context) -> bool {
        return context.brake_press_active_;
    }

    static auto boost_press_active(FShipControlContext const& context) -> bool {
        return context.boost_press_active_;
    }

    static auto throttle_boost_active(FShipControlContext const& context) -> bool {
        return context.throttle_boost_active_;
    }

    static void seed_throttle_tap(FShipControlContext& context) {
        context.throttle_gesture_.begin_press(1.0);
        context.throttle_gesture_.end_press(1.1);
    }

    static auto begin_throttle_press(FShipControlContext& context) -> bool {
        return context.throttle_gesture_.begin_press(1.2);
    }

    static void neutralise_ship_input(FShipControlContext& context) {
        context.neutralise_ship_input();
    }
};

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

    TEST_METHOD(RuntimePlayerProvidesDefaultFlightModelLoadout)
    {
        auto const* const config{ml::load_default_level_config()};
        auto const* const player{config && IsValid(config->classes.player_ship_class)
                                     ? config->classes.player_ship_class.GetDefaultObject()
                                     : nullptr};
        if (!TestRunner->TestTrue(TEXT("Runtime player ship class is configured"),
                                  IsValid(player))) {
            return;
        }

        auto const spawn{player->make_spawn_data()};
        TestRunner->TestTrue(
            TEXT("Runtime player provides the four default model slots"),
            ::ioj::sim::player::flight_model_profile(spawn.flight_models,
                                                     ::ioj::sim::player::FlightModelSlot::Up)
                        .base_preset == ::ioj::sim::player::FlightModelPreset::Starfox &&
                ::ioj::sim::player::flight_model_profile(spawn.flight_models,
                                                         ::ioj::sim::player::FlightModelSlot::Right)
                        .base_preset == ::ioj::sim::player::FlightModelPreset::Fighter &&
                ::ioj::sim::player::flight_model_profile(spawn.flight_models,
                                                         ::ioj::sim::player::FlightModelSlot::Down)
                        .base_preset == ::ioj::sim::player::FlightModelPreset::Skater &&
                ::ioj::sim::player::flight_model_profile(spawn.flight_models,
                                                         ::ioj::sim::player::FlightModelSlot::Left)
                        .base_preset == ::ioj::sim::player::FlightModelPreset::Gunship);
    }

    TEST_METHOD(RuntimeControllerInputsMatchAuthoredController)
    {
        auto* const runtime_config{
            LoadObject<USpaceGameLevelConfig>(nullptr,
                                              TEXT("/SpaceGame/Levels/DA_GameRuntimeLevelConfig."
                                                   "DA_GameRuntimeLevelConfig"))};
        auto* const source_controller_class{LoadClass<ASpaceGamePlayerController>(
            nullptr,
            TEXT("/Game/Levels/FeatureTests/FT_soa_turrets/BP_TestSpaceShipController."
                 "BP_TestSpaceShipController_C"))};
        auto const* const source_controller{IsValid(source_controller_class)
                                                ? source_controller_class->GetDefaultObject()
                                                : nullptr};
        auto const* const runtime_controller{
            IsValid(runtime_config) && IsValid(runtime_config->classes.player_controller_class)
                ? runtime_config->classes.player_controller_class.GetDefaultObject()
                : nullptr};
        if (!TestRunner->TestTrue(TEXT("Authored controller loads"), IsValid(source_controller)) ||
            !TestRunner->TestTrue(TEXT("Runtime controller loads"), IsValid(runtime_controller))) {
            return;
        }

        auto const* const source_input_property{
            FindFProperty<FStructProperty>(source_controller->GetClass(), TEXT("input"))};
        auto const* const runtime_input_property{
            FindFProperty<FStructProperty>(runtime_controller->GetClass(), TEXT("input"))};
        auto const* const source_global_property{
            FindFProperty<FStructProperty>(source_controller->GetClass(), TEXT("global_input"))};
        auto const* const runtime_global_property{
            FindFProperty<FStructProperty>(runtime_controller->GetClass(), TEXT("global_input"))};
        if (!TestRunner->TestTrue(TEXT("Authored input properties are available"),
                                  source_input_property && source_global_property) ||
            !TestRunner->TestTrue(TEXT("Runtime input properties are available"),
                                  runtime_input_property && runtime_global_property)) {
            return;
        }

        auto const* const source_input{
            source_input_property->ContainerPtrToValuePtr<FSpaceShipControllerInputs>(
                source_controller)};
        auto const* const runtime_input{
            runtime_input_property->ContainerPtrToValuePtr<FSpaceShipControllerInputs>(
                runtime_controller)};
        auto const* const source_global{
            source_global_property->ContainerPtrToValuePtr<FGlobalControlInputs>(
                source_controller)};
        auto const* const runtime_global{
            runtime_global_property->ContainerPtrToValuePtr<FGlobalControlInputs>(
                runtime_controller)};
        auto const matches = [this](TCHAR const* const name,
                                    UObject const* const runtime_value,
                                    UObject const* const source_value) {
            TestRunner->TestTrue(name, runtime_value == source_value);
        };
        matches(TEXT("Runtime move action matches the authored controller"),
                runtime_input->move,
                source_input->move);
        matches(TEXT("Runtime turn action matches the authored controller"),
                runtime_input->turn,
                source_input->turn);
        TestRunner->TestTrue(TEXT("Runtime pointer-turn delta action is generated"),
                             IsValid(runtime_input->turn_pointer_delta));
        TestRunner->TestTrue(TEXT("Runtime pointer-turn engagement action is generated"),
                             IsValid(runtime_input->engage_pointer_turn));
        TestRunner->TestTrue(TEXT("Runtime throttle action is generated"),
                             IsValid(runtime_input->throttle));
        TestRunner->TestEqual(TEXT("Throttle action is analog"),
                              runtime_input->throttle->ValueType,
                              EInputActionValueType::Axis1D);
        matches(TEXT("Runtime fire action matches the authored controller"),
                runtime_input->fire_laser,
                source_input->fire_laser);
        matches(TEXT("Runtime boost action matches the authored controller"),
                runtime_input->boost,
                source_input->boost);
        matches(TEXT("Runtime brake action matches the authored controller"),
                runtime_input->brake,
                source_input->brake);
        matches(TEXT("Runtime roll action matches the authored controller"),
                runtime_input->roll,
                source_input->roll);
        matches(TEXT("Runtime barrel-roll action matches the authored controller"),
                runtime_input->barrel_roll,
                source_input->barrel_roll);
        matches(TEXT("Runtime next fire-rate action matches the authored controller"),
                runtime_input->cycle_next_fire_rate,
                source_input->cycle_next_fire_rate);
        matches(TEXT("Runtime previous fire-rate action matches the authored controller"),
                runtime_input->cycle_prev_fire_rate,
                source_input->cycle_prev_fire_rate);
        matches(TEXT("Runtime profile action matches the authored controller"),
                runtime_input->cycle_input_mapping_context,
                source_input->cycle_input_mapping_context);
        matches(TEXT("Runtime lateral action matches the authored controller"),
                runtime_input->lateral_move,
                source_input->lateral_move);
        matches(TEXT("Runtime forward action matches the authored controller"),
                runtime_input->forward_move,
                source_input->forward_move);
        matches(TEXT("Runtime vertical action matches the authored controller"),
                runtime_input->vertical_move,
                source_input->vertical_move);
        matches(TEXT("Runtime sample-and-hold action matches the authored controller"),
                runtime_input->sample_and_hold,
                source_input->sample_and_hold);
        TestRunner->TestTrue(TEXT("Runtime increase-forward-velocity action is generated"),
                             IsValid(runtime_input->increase_desired_forward_velocity));
        TestRunner->TestTrue(TEXT("Runtime decrease-forward-velocity action is generated"),
                             IsValid(runtime_input->decrease_desired_forward_velocity));
        matches(TEXT("Runtime 2D sample action matches the authored controller"),
                runtime_input->ship_2d_control,
                source_input->ship_2d_control);
        matches(TEXT("Runtime X sample action matches the authored controller"),
                runtime_input->ship_1d_control_x,
                source_input->ship_1d_control_x);
        matches(TEXT("Runtime Y sample action matches the authored controller"),
                runtime_input->ship_1d_control_y,
                source_input->ship_1d_control_y);
        TestRunner->TestTrue(TEXT("Runtime D-pad Up flight-model action is generated"),
                             IsValid(runtime_input->select_flight_model_up));
        TestRunner->TestTrue(TEXT("Runtime D-pad Right flight-model action is generated"),
                             IsValid(runtime_input->select_flight_model_right));
        TestRunner->TestTrue(TEXT("Runtime D-pad Down flight-model action is generated"),
                             IsValid(runtime_input->select_flight_model_down));
        TestRunner->TestTrue(TEXT("Runtime D-pad Left flight-model action is generated"),
                             IsValid(runtime_input->select_flight_model_left));
        matches(TEXT("Runtime global mapping matches the authored controller"),
                runtime_global->mapping_context,
                source_global->mapping_context);
        matches(TEXT("Runtime pause action matches the authored controller"),
                runtime_global->toggle_menu,
                source_global->toggle_menu);

        auto* const canonical_mapping{LoadObject<UInputMappingContext>(
            nullptr, TEXT("/SpaceGame/Input/SpaceShip/IMC_SpaceShip_Base.IMC_SpaceShip_Base"))};
        TestRunner->TestTrue(TEXT("Runtime controller uses the canonical ship mapping"),
                             IsValid(canonical_mapping) &&
                                 runtime_input->mapping_context == canonical_mapping);
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
        TestRunner->TestTrue(TEXT("Mapping contains analog throttle"),
                             mapping_context->HasMappingForInputAction(input->throttle));
        TestRunner->TestTrue(
            TEXT("Mapping contains direct flight-model selection"),
            mapping_context->HasMappingForInputAction(input->select_flight_model_up) &&
                mapping_context->HasMappingForInputAction(input->select_flight_model_right) &&
                mapping_context->HasMappingForInputAction(input->select_flight_model_down) &&
                mapping_context->HasMappingForInputAction(input->select_flight_model_left));

        mapping_context->ForEachKeyMapping([this](FEnhancedActionKeyMapping const& mapping) {
            TestRunner->TestTrue(TEXT("Mapping contains a valid action"), IsValid(mapping.Action));
            TestRunner->TestTrue(*FString::Printf(TEXT("Mapping '%s' on '%s' is player mappable"),
                                                  IsValid(mapping.Action)
                                                      ? *mapping.Action->GetName()
                                                      : TEXT("Invalid"),
                                                  *mapping.Key.GetFName().ToString()),
                                 mapping.IsPlayerMappable());
        });
    }

    TEST_METHOD(DefaultProfileContainsFlightAndSamplingControls)
    {
        auto const* const config{ml::load_default_level_config()};
        auto const* const controller_default{
            IsValid(config) && IsValid(config->classes.player_controller_class)
                ? config->classes.player_controller_class.GetDefaultObject()
                : nullptr};
        auto const* const input_property{
            IsValid(controller_default)
                ? FindFProperty<FStructProperty>(controller_default->GetClass(), TEXT("input"))
                : nullptr};
        if (!TestRunner->TestTrue(TEXT("Controller input property is available"),
                                  input_property != nullptr)) {
            return;
        }

        auto const* const input{
            input_property->ContainerPtrToValuePtr<FSpaceShipControllerInputs>(controller_default)};
        auto const* const mapping_context{input->get_mapping_context()};
        if (!TestRunner->TestTrue(TEXT("Default mapping context is available"),
                                  IsValid(mapping_context))) {
            return;
        }
        auto const has_mapping = [mapping_context](UInputAction const* const action,
                                                   FKey const key) {
            return mapping_context->GetMappings().ContainsByPredicate(
                [action, key](FEnhancedActionKeyMapping const& mapping) {
                    return mapping.Action == action && mapping.Key == key;
                });
        };
        TestRunner->TestTrue(TEXT("W has forward movement"),
                             has_mapping(input->forward_move, EKeys::W));
        TestRunner->TestTrue(TEXT("W also publishes accelerator intent"),
                             has_mapping(input->throttle, EKeys::W));
        TestRunner->TestTrue(TEXT("S has backward movement"),
                             has_mapping(input->forward_move, EKeys::S));
        TestRunner->TestTrue(TEXT("Space has upward movement"),
                             has_mapping(input->vertical_move, EKeys::SpaceBar));
        TestRunner->TestTrue(TEXT("Left Control has downward movement"),
                             has_mapping(input->vertical_move, EKeys::LeftControl));
        TestRunner->TestTrue(TEXT("Gamepad face top has upward movement"),
                             has_mapping(input->vertical_move, EKeys::Gamepad_FaceButton_Top));
        TestRunner->TestTrue(TEXT("Gamepad face bottom has downward movement"),
                             has_mapping(input->vertical_move, EKeys::Gamepad_FaceButton_Bottom));
        for (auto const& profile : ml::ioj::control_profile_definitions()) {
            auto const mappings{mapping_context->GetMappingsForProfile(profile.id)};
            auto const has_vertical_button = [input, &mappings](FKey const key) {
                return mappings.ContainsByPredicate([input, key](auto const& mapping) {
                    return mapping.Action == input->vertical_move && mapping.Key == key;
                });
            };
            TestRunner->TestTrue(TEXT("Every profile maps gamepad ascend"),
                                 has_vertical_button(EKeys::Gamepad_FaceButton_Top));
            TestRunner->TestTrue(TEXT("Every profile maps gamepad descend"),
                                 has_vertical_button(EKeys::Gamepad_FaceButton_Bottom));
            auto const* const descend{mappings.FindByPredicate([input](auto const& mapping) {
                return mapping.Action == input->vertical_move &&
                       mapping.Key == EKeys::Gamepad_FaceButton_Bottom;
            })};
            if (descend != nullptr) {
                TestRunner->TestTrue(TEXT("Gamepad descend produces negative vertical input"),
                                     descend->Modifiers.ContainsByPredicate(
                                         [](TObjectPtr<UInputModifier> const& modifier) {
                                             return IsValid(modifier) &&
                                                    modifier->IsA<UInputModifierNegate>();
                                         }));
            }
        }
        TestRunner->TestTrue(TEXT("A has sampled lateral control"),
                             has_mapping(input->ship_1d_control_x, EKeys::A));
        TestRunner->TestTrue(TEXT("D has sampled lateral control"),
                             has_mapping(input->ship_1d_control_x, EKeys::D));
        TestRunner->TestTrue(TEXT("A has trim-based lateral movement"),
                             has_mapping(input->lateral_move, EKeys::A));
        TestRunner->TestTrue(TEXT("D has trim-based lateral movement"),
                             has_mapping(input->lateral_move, EKeys::D));
        TestRunner->TestTrue(TEXT("W has sampled forward control"),
                             has_mapping(input->ship_1d_control_y, EKeys::W));
        TestRunner->TestTrue(TEXT("S has sampled backward control"),
                             has_mapping(input->ship_1d_control_y, EKeys::S));
        TestRunner->TestTrue(TEXT("Mouse thumb 2 has sample-and-hold"),
                             has_mapping(input->sample_and_hold, EKeys::ThumbMouseButton2));
        TestRunner->TestTrue(
            TEXT("Mouse wheel up increases desired forward velocity"),
            has_mapping(input->increase_desired_forward_velocity, EKeys::MouseScrollUp));
        TestRunner->TestTrue(
            TEXT("Mouse wheel down decreases desired forward velocity"),
            has_mapping(input->decrease_desired_forward_velocity, EKeys::MouseScrollDown));
        auto const has_named_mapping = [mapping_context](FName const action_name, FKey const key) {
            return mapping_context->GetMappings().ContainsByPredicate(
                [action_name, key](FEnhancedActionKeyMapping const& mapping) {
                    return mapping.Key == key && IsValid(mapping.Action) &&
                           mapping.Action->GetFName() == action_name;
                });
        };
        auto const* const pointer_turn_mapping{
            mapping_context->GetMappings().FindByPredicate([](auto const& mapping) {
                return mapping.Key == EKeys::Mouse2D && IsValid(mapping.Action) &&
                       mapping.Action->GetFName() == TEXT("IA_Ship_TurnPointerDelta");
            })};
        if (TestRunner->TestNotNull(TEXT("Mouse2D has pointer-turn displacement"),
                                    pointer_turn_mapping)) {
            TestRunner->TestFalse(TEXT("Pointer displacement is not chord-gated in the mapping"),
                                  pointer_turn_mapping->Triggers.ContainsByPredicate(
                                      [](TObjectPtr<UInputTrigger> const& trigger) {
                                          return IsValid(trigger) &&
                                                 trigger->IsA<UInputTriggerChordAction>();
                                      }));
        }
        TestRunner->TestTrue(
            TEXT("Right mouse engages pointer turning"),
            has_named_mapping(TEXT("IA_Ship_EngagePointerTurn"), EKeys::RightMouseButton));
        TestRunner->TestTrue(TEXT("Left trigger provides analog throttle"),
                             has_mapping(input->throttle, EKeys::Gamepad_LeftTriggerAxis));
        TestRunner->TestTrue(TEXT("Left shoulder provides braking"),
                             has_mapping(input->brake, EKeys::Gamepad_LeftShoulder));
        TestRunner->TestTrue(TEXT("Right trigger fires lasers"),
                             has_mapping(input->fire_laser, EKeys::Gamepad_RightTriggerAxis));
        TestRunner->TestTrue(TEXT("D-pad Up selects the Up flight-model slot"),
                             has_mapping(input->select_flight_model_up, EKeys::Gamepad_DPad_Up));
        TestRunner->TestTrue(
            TEXT("D-pad Right selects the Right flight-model slot"),
            has_mapping(input->select_flight_model_right, EKeys::Gamepad_DPad_Right));
        TestRunner->TestTrue(
            TEXT("D-pad Down selects the Down flight-model slot"),
            has_mapping(input->select_flight_model_down, EKeys::Gamepad_DPad_Down));
        TestRunner->TestTrue(
            TEXT("D-pad Left selects the Left flight-model slot"),
            has_mapping(input->select_flight_model_left, EKeys::Gamepad_DPad_Left));

        TSet<ml::ioj::EControlBindingGroup> groups;
        for (auto const& mapping : mapping_context->GetMappings()) {
            if (!mapping.IsPlayerMappable()) {
                continue;
            }
            auto const* const mapping_settings{mapping.GetPlayerMappableKeySettings()};
            auto const* const metadata{
                IsValid(mapping_settings)
                    ? Cast<ml::ioj::UControlBindingMetadata>(mapping_settings->Metadata)
                    : nullptr};
            if (!TestRunner->TestNotNull(
                    *FString::Printf(TEXT("Mapping '%s' has presentation metadata"),
                                     *mapping.GetMappingName().ToString()),
                    metadata)) {
                continue;
            }
            groups.Add(metadata->group);
            TestRunner->TestTrue(TEXT("Binding display order is authored"),
                                 metadata->display_order > 0);
            TestRunner->TestTrue(TEXT("Binding category follows the authored group"),
                                 mapping.GetDisplayCategory().EqualTo(
                                     ml::ioj::control_binding_group_label(metadata->group)));
        }
        TestRunner->TestTrue(TEXT("Bindings cover the authored Flight group"),
                             groups.Contains(ml::ioj::EControlBindingGroup::Flight));
        TestRunner->TestTrue(TEXT("Bindings cover the authored Combat group"),
                             groups.Contains(ml::ioj::EControlBindingGroup::Combat));
        TestRunner->TestTrue(TEXT("Bindings cover the authored Utility group"),
                             groups.Contains(ml::ioj::EControlBindingGroup::Utility));

        auto const* const forward_mapping{
            mapping_context->GetMappings().FindByPredicate([input](auto const& mapping) {
                return mapping.Action == input->forward_move && mapping.Key == EKeys::W;
            })};
        auto const* const backward_mapping{
            mapping_context->GetMappings().FindByPredicate([input](auto const& mapping) {
                return mapping.Action == input->forward_move && mapping.Key == EKeys::S;
            })};
        if (TestRunner->TestNotNull(TEXT("Forward mapping has presentation"), forward_mapping) &&
            TestRunner->TestNotNull(TEXT("Backward mapping has presentation"), backward_mapping)) {
            TestRunner->TestEqual(TEXT("Forward mapping has a semantic label"),
                                  forward_mapping->GetDisplayName().ToString(),
                                  FString{TEXT("Forward")});
            TestRunner->TestEqual(TEXT("Backward mapping has a semantic label"),
                                  backward_mapping->GetDisplayName().ToString(),
                                  FString{TEXT("Backward")});
            TestRunner->TestTrue(TEXT("Multiple mappings retain distinct names"),
                                 forward_mapping->GetMappingName() !=
                                     backward_mapping->GetMappingName());
        }
    }

    TEST_METHOD(DoubleTapHoldGestureRecognitionIsConfigurable)
    {
        FShipInputGestureRecognizer gesture{0.25};
        TestRunner->TestFalse(TEXT("The first press is not a double tap"),
                              gesture.begin_press(1.0));
        gesture.end_press(1.1);
        TestRunner->TestTrue(TEXT("A quick second press starts a double-tap hold"),
                             gesture.begin_press(1.3));
        gesture.end_press(1.7);
        TestRunner->TestFalse(TEXT("A long hold cannot seed a later double tap"),
                              gesture.begin_press(1.8));

        gesture.reset();
        TestRunner->TestFalse(TEXT("Reset clears a pending tap"), gesture.begin_press(2.0));
    }

    TEST_METHOD(NeutralisationResetsGestureStateWithoutShip)
    {
        FShipControlContext context;
        FShipControlContextTestAccess::seed_throttle_tap(context);
        FShipControlContextTestAccess::neutralise_ship_input(context);

        TestRunner->TestFalse(TEXT("A gesture cannot survive neutralisation without a ship"),
                              FShipControlContextTestAccess::begin_throttle_press(context));
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
                auto const profile_has_action = [registered](UInputAction const* const action) {
                    for (auto const& row : registered->GetPlayerMappingRows()) {
                        for (auto const& mapping : row.Value.Mappings) {
                            if (mapping.GetAssociatedInputAction() == action) {
                                return true;
                            }
                        }
                    }
                    return false;
                };
                TestRunner->TestTrue(
                    *FString::Printf(TEXT("Profile '%s' contains increase velocity"), *profile.id),
                    profile_has_action(input->increase_desired_forward_velocity));
                TestRunner->TestTrue(
                    *FString::Printf(TEXT("Profile '%s' contains decrease velocity"), *profile.id),
                    profile_has_action(input->decrease_desired_forward_velocity));
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

            TArray<FEnhancedActionKeyMapping> expected;
            for (auto const& mapping : source->GetMappings()) {
                if (mapping.Key.IsGamepadKey()) {
                    expected.Add(mapping);
                }
            }
            for (auto const& mapping : generated->GetMappings()) {
                if (!mapping.Key.IsGamepadKey()) {
                    expected.Add(mapping);
                }
            }
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

        TestRunner->TestFalse(TEXT("Wheel cannot be held even if the caller permits it"),
                              capture.accept(EKeys::MouseScrollUp, true));
        TestRunner->TestFalse(TEXT("Axis cannot be a held activator"),
                              capture.accept(EKeys::Gamepad_LeftX, true));
        TestRunner->TestFalse(TEXT("Invalid activators leave capture empty"),
                              capture.held_key().IsValid());
        capture.accept(EKeys::ThumbMouseButton, true);
        TestRunner->TestFalse(TEXT("Repeated activator does not complete a chord"),
                              capture.accept(EKeys::ThumbMouseButton, true));
        TestRunner->TestFalse(TEXT("Mixed device chord is rejected"),
                              capture.accept(EKeys::Gamepad_FaceButton_Bottom, true));
        TestRunner->TestTrue(TEXT("Wheel can be the action input after retry"),
                             capture.accept(EKeys::MouseScrollDown, false));
    }

    TEST_METHOD(NewSamplingSessionStartsWithNeutralControl)
    {
        ::ioj::sim::SimClock clock;
        ::ioj::sim::EntityLedger ledger;
        ::ioj::sim::CombatEvents combat_events{ledger};
        ::ioj::sim::AgentIndices indexes{clock};
        ::ioj::sim::HealthTable health_table;
        ::ioj::sim::AgentAccessor agents{indexes, health_table};
        ::ioj::sim::SpatialQueryManager queries{agents};
        ::ioj::sim::lasers::Sim lasers{clock, combat_events, queries};
        ::ioj::sim::player::Sim simulation{
            clock, ledger, combat_events, health_table, queries, lasers};
        simulation.start_sampling();
        simulation.set_ship_1d_control_y(1.0f);
        simulation.stop_sampling();
        TestRunner->TestEqual(TEXT("Released sample retains the committed direction"),
                              ml::to_unreal(simulation.get_sampled_target_speed_scale()),
                              FVector2D{0.0f, 1.0f});

        simulation.start_sampling();
        TestRunner->TestEqual(TEXT("New sample starts from neutral"),
                              ml::to_unreal(simulation.get_sampled_target_speed_scale()),
                              FVector2D::ZeroVector);

        simulation.set_ship_1d_control_x(1.0f);
        simulation.start_sampling();
        TestRunner->TestEqual(TEXT("Starting a second axis does not reset the active sample"),
                              ml::to_unreal(simulation.get_sampled_target_speed_scale()),
                              FVector2D{1.0f, 0.0f});
    }

    TEST_METHOD(DesiredForwardVelocityTrimAdjustsThePersistentTarget)
    {
        ::ioj::sim::SimClock clock;
        ::ioj::sim::EntityLedger ledger;
        ::ioj::sim::CombatEvents combat_events{ledger};
        ::ioj::sim::AgentIndices indexes{clock};
        ::ioj::sim::HealthTable health_table;
        ::ioj::sim::AgentAccessor agents{indexes, health_table};
        ::ioj::sim::SpatialQueryManager queries{agents};
        ::ioj::sim::lasers::Sim lasers{clock, combat_events, queries};
        ::ioj::sim::player::Sim simulation{
            clock, ledger, combat_events, health_table, queries, lasers};
        auto profile{::ioj::sim::player::make_flight_model_profile(
            ::ioj::sim::player::FlightModelPreset::Gunship)};
        profile.config.translation.forward.manual.semantic =
            ::ioj::sim::player::TranslationSemantic::TargetSpeed;
        profile.config.translation.forward.normal.positive_speed_limit = 1000.f;
        profile.config.translation.forward.normal.negative_speed_limit = 1000.f;
        profile.config.translation.right.manual.semantic =
            ::ioj::sim::player::TranslationSemantic::TargetSpeed;
        profile.config.translation.right.normal.positive_speed_limit = 1000.f;
        profile.config.translation.right.normal.negative_speed_limit = 1000.f;
        TestRunner->TestTrue(TEXT("Target-speed experiment is accepted"),
                             simulation.set_flight_model_slot_profile(
                                 ::ioj::sim::player::FlightModelSlot::Left, profile));
        simulation.select_flight_model_slot(::ioj::sim::player::FlightModelSlot::Left);

        simulation.start_sampling();
        simulation.set_ship_2d_control({0.25, -0.5});
        simulation.stop_sampling();
        TestRunner->TestTrue(
            TEXT("Sample establishes a reverse persistent target"),
            FMath::IsNearlyEqual(simulation.get_controller_state().persistent_forward_target_speed,
                                 -500.f) &&
                FMath::IsNearlyEqual(
                    simulation.get_controller_state().persistent_right_target_speed, 250.f));

        simulation.adjust_desired_forward_velocity(1.f);
        TestRunner->TestTrue(
            TEXT("Increasing makes reverse velocity less negative"),
            FMath::IsNearlyEqual(simulation.get_controller_state().persistent_forward_target_speed,
                                 -450.f));

        ::ioj::sim::PlayerSimTestAccess::set_transform(
            simulation, ml::to_native(FTransform{FRotator{0.0, 90.0, 0.0}}));
        simulation.adjust_desired_forward_velocity(1.f);
        TestRunner->TestTrue(
            TEXT("Trim remains model-local after rotating"),
            FMath::IsNearlyEqual(simulation.get_controller_state().persistent_forward_target_speed,
                                 -400.f));

        for (int32 adjustment{}; adjustment < 20; ++adjustment) {
            simulation.adjust_desired_forward_velocity(-1.f);
        }
        TestRunner->TestTrue(
            TEXT("Reverse trim clamps to the configured velocity limit"),
            FMath::IsNearlyEqual(simulation.get_controller_state().persistent_forward_target_speed,
                                 -1000.f));

        auto const persistent_target{
            simulation.get_controller_state().persistent_forward_target_speed};
        simulation.start_boost();
        simulation.stop_boost();
        TestRunner->TestTrue(
            TEXT("Boost preserves the persistent target"),
            FMath::IsNearlyEqual(simulation.get_controller_state().persistent_forward_target_speed,
                                 persistent_target));

        simulation.start_sampling();
        simulation.adjust_desired_forward_velocity(1.f);
        TestRunner->TestTrue(
            TEXT("Trim does not alter an active sample"),
            FMath::IsNearlyEqual(simulation.get_controller_state().persistent_forward_target_speed,
                                 persistent_target));
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
        input.turn_pointer_delta = move_action;
        input.engage_pointer_turn = move_action;
        input.fire_laser = move_action;
        input.throttle = move_action;
        input.boost = move_action;
        input.brake = move_action;
        input.roll = move_action;
        input.cycle_next_fire_rate = move_action;
        input.cycle_prev_fire_rate = move_action;
        input.cycle_input_mapping_context = move_action;
        input.lateral_move = move_action;
        input.forward_move = move_action;
        input.vertical_move = move_action;
        input.sample_and_hold = move_action;
        input.increase_desired_forward_velocity = move_action;
        input.decrease_desired_forward_velocity = move_action;
        input.ship_2d_control = move_action;
        input.ship_1d_control_x = move_action;
        input.ship_1d_control_y = move_action;
        input.select_flight_model_up = move_action;
        input.select_flight_model_right = move_action;
        input.select_flight_model_down = move_action;
        input.select_flight_model_left = move_action;

        auto& sentinel_binding{input_component->BindActionValueLambda(
            sentinel_action, ETriggerEvent::Started, [](FInputActionValue const&) {})};
        auto const sentinel_handle{sentinel_binding.GetHandle()};

        FShipControlContext context;
        TestRunner->TestTrue(
            TEXT("Ship context initializes"),
            context.initialise(*controller, *input_component, *input_subsystem, input));
        context.set_ship(ship);
        TestRunner->TestFalse(TEXT("Ship context cannot bind before simulation initialization"),
                              context.can_bind());
        ::ioj::sim::SimClock clock;
        ::ioj::sim::EntityLedger ledger;
        ::ioj::sim::CombatEvents combat_events{ledger};
        ::ioj::sim::AgentIndices indexes{clock};
        ::ioj::sim::HealthTable health_table;
        ::ioj::sim::AgentAccessor agents{indexes, health_table};
        ::ioj::sim::SpatialQueryManager queries{agents};
        ::ioj::sim::lasers::Sim lasers{clock, combat_events, queries};
        ::ioj::sim::player::Sim simulation{
            clock, ledger, combat_events, health_table, queries, lasers};
        ::ioj::sim::player::CommandInterface commands{simulation};
        ship->bind_simulation(commands, simulation);
        TestRunner->TestTrue(TEXT("Ship context binds"), context.bind());
        TestRunner->TestTrue(TEXT("Ship context reports bound"), context.is_bound());
        TestRunner->TestTrue(TEXT("Ship mapping is active"),
                             input_subsystem->HasMappingContext(mapping_context));

        auto const bindings_after_bind{input_component->GetActionEventBindings().Num()};
        TestRunner->TestTrue(TEXT("Repeated bind is idempotent"), context.bind());
        TestRunner->TestEqual(TEXT("Repeated bind does not duplicate handlers"),
                              input_component->GetActionEventBindings().Num(),
                              bindings_after_bind);

        FShipControlContextTestAccess::start_throttle(context, 0.5f, 1.0);
        TestRunner->TestTrue(TEXT("Throttle always publishes analog intent"),
                             FMath::IsNearlyEqual(ship->get_throttle(), 0.5f));
        FShipControlContextTestAccess::stop_throttle(context, 1.1);
        TestRunner->TestTrue(TEXT("Throttle release clears intent"),
                             FMath::IsNearlyZero(ship->get_throttle()));

        FShipControlContextTestAccess::start_throttle(context, 1.f, 2.0);
        FShipControlContextTestAccess::stop_throttle(context, 2.1);
        FShipControlContextTestAccess::start_throttle(context, 1.f, 2.2);
        TestRunner->TestTrue(TEXT("Throttle double-tap publishes boost intent"),
                             simulation.get_controller_state().effective_action ==
                                 ::ioj::sim::player::BoostBrakeState::Boost);
        FShipControlContextTestAccess::select_flight_model_up(context);
        TestRunner->TestTrue(TEXT("Model selection clears gesture-derived boost intent"),
                             simulation.get_controller_state().effective_action ==
                                 ::ioj::sim::player::BoostBrakeState::None);
        FShipControlContextTestAccess::select_flight_model_down(context);
        TestRunner->TestTrue(TEXT("Repeated model selection leaves throttle boost cleared"),
                             !FShipControlContextTestAccess::throttle_boost_active(context) &&
                                 simulation.get_controller_state().effective_action ==
                                     ::ioj::sim::player::BoostBrakeState::None);
        FShipControlContextTestAccess::stop_throttle(context, 2.3);

        FShipControlContextTestAccess::start_boost(context);
        FShipControlContextTestAccess::select_flight_model_right(context);
        TestRunner->TestTrue(TEXT("Model selection preserves an explicitly held boost button"),
                             FShipControlContextTestAccess::boost_press_active(context) &&
                                 simulation.get_controller_state().effective_action ==
                                     ::ioj::sim::player::BoostBrakeState::Boost);
        FShipControlContextTestAccess::select_flight_model_left(context);
        TestRunner->TestTrue(
            TEXT("Repeated model selection preserves an explicitly held boost button"),
            FShipControlContextTestAccess::boost_press_active(context) &&
                !FShipControlContextTestAccess::throttle_boost_active(context) &&
                simulation.get_controller_state().effective_action ==
                    ::ioj::sim::player::BoostBrakeState::Boost);
        FShipControlContextTestAccess::stop_boost(context);
        TestRunner->TestTrue(TEXT("Boost release after repeated model selection clears intent"),
                             !FShipControlContextTestAccess::boost_press_active(context) &&
                                 simulation.get_controller_state().effective_action ==
                                     ::ioj::sim::player::BoostBrakeState::None);

        FShipControlContextTestAccess::start_brake(context, 3.0);
        TestRunner->TestTrue(TEXT("Brake publishes normal braking intent"),
                             simulation.get_controller_state().effective_action ==
                                 ::ioj::sim::player::BoostBrakeState::Brake);
        FShipControlContextTestAccess::select_flight_model_up(context);
        TestRunner->TestTrue(TEXT("Model selection preserves a held brake"),
                             FShipControlContextTestAccess::brake_press_active(context) &&
                                 simulation.get_controller_state().effective_action ==
                                     ::ioj::sim::player::BoostBrakeState::Brake);
        FShipControlContextTestAccess::select_flight_model_right(context);
        TestRunner->TestTrue(TEXT("Repeated model selection preserves a held brake"),
                             FShipControlContextTestAccess::brake_press_active(context) &&
                                 simulation.get_controller_state().effective_action ==
                                     ::ioj::sim::player::BoostBrakeState::Brake);
        FShipControlContextTestAccess::stop_brake(context, 3.1);
        TestRunner->TestTrue(TEXT("Brake release after repeated model selection clears intent"),
                             !FShipControlContextTestAccess::brake_press_active(context) &&
                                 simulation.get_controller_state().effective_action ==
                                     ::ioj::sim::player::BoostBrakeState::None);
        FShipControlContextTestAccess::select_flight_model_down(context);
        TestRunner->TestTrue(TEXT("Model selection without held brake does not start braking"),
                             !FShipControlContextTestAccess::brake_press_active(context) &&
                                 simulation.get_controller_state().effective_action ==
                                     ::ioj::sim::player::BoostBrakeState::None);

        FShipControlContextTestAccess::start_brake(context, 4.0);
        FShipControlContextTestAccess::stop_brake(context, 4.1);
        FShipControlContextTestAccess::start_brake(context, 4.2);
        TestRunner->TestTrue(TEXT("Brake double-tap publishes emergency braking intent"),
                             simulation.get_controller_state().effective_action ==
                                 ::ioj::sim::player::BoostBrakeState::EmergencyBrake);
        FShipControlContextTestAccess::select_flight_model_down(context);
        TestRunner->TestTrue(
            TEXT("Model selection preserves held brake without preserving the emergency gesture"),
            simulation.get_controller_state().effective_action ==
                ::ioj::sim::player::BoostBrakeState::Brake);
        FShipControlContextTestAccess::stop_brake(context, 4.3);

        FShipControlContextTestAccess::select_flight_model_up(context);
        TestRunner->TestTrue(TEXT("D-pad Up selects Starfox slot"),
                             simulation.get_active_flight_model_slot() ==
                                 ::ioj::sim::player::FlightModelSlot::Up);
        FShipControlContextTestAccess::select_flight_model_right(context);
        TestRunner->TestTrue(TEXT("D-pad Right selects Fighter slot"),
                             simulation.get_active_flight_model_slot() ==
                                 ::ioj::sim::player::FlightModelSlot::Right);
        FShipControlContextTestAccess::select_flight_model_down(context);
        TestRunner->TestTrue(TEXT("D-pad Down selects Skater slot"),
                             simulation.get_active_flight_model_slot() ==
                                 ::ioj::sim::player::FlightModelSlot::Down);
        FShipControlContextTestAccess::select_flight_model_left(context);
        TestRunner->TestTrue(TEXT("D-pad Left selects Gunship slot"),
                             simulation.get_active_flight_model_slot() ==
                                 ::ioj::sim::player::FlightModelSlot::Left);

        FShipControlContextTestAccess::start_throttle(context, 1.f, 6.0);
        TestRunner->TestTrue(TEXT("Throttle press is tracked"),
                             FShipControlContextTestAccess::throttle_press_active(context));
        FShipControlContextTestAccess::select_flight_model_up(context);
        TestRunner->TestFalse(TEXT("Model selection clears active throttle gesture state"),
                              FShipControlContextTestAccess::throttle_press_active(context));

        FShipControlContextTestAccess::start_brake(context, 7.0);
        TestRunner->TestTrue(TEXT("Brake press is tracked"),
                             FShipControlContextTestAccess::brake_press_active(context));
        FShipControlContextTestAccess::stop_brake(context, 7.1);
        FShipControlContextTestAccess::select_flight_model_right(context);
        FShipControlContextTestAccess::start_brake(context, 7.2);
        TestRunner->TestTrue(TEXT("Model selection clears pending emergency-brake gesture state"),
                             FShipControlContextTestAccess::brake_press_active(context) &&
                                 simulation.get_controller_state().effective_action ==
                                     ::ioj::sim::player::BoostBrakeState::Brake);
        FShipControlContextTestAccess::stop_brake(context, 7.3);

        ship->set_move_input(FVector2D{0.5f, -0.25f});
        ship->turn(FVector2D{0.25f, 0.75f});
        ship->set_throttle(0.75f);
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
        TestRunner->TestTrue(TEXT("Throttle is neutralized"),
                             FMath::IsNearlyZero(ship->get_throttle()));
        TestRunner->TestTrue(TEXT("Laser firing is stopped"),
                             ship->get_laser_firing_mode() == ::ioj::sim::LaserFiringState::idle);

        context.shutdown();
        ship->Destroy();
        controller->Destroy();
    }
};
