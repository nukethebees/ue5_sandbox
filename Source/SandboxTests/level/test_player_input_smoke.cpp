#include <SandboxTests/support/PlayerControllerTestAccess.h>
#include <SandboxTests/support/test_setup.h>

#include <SpaceGame/input/ControlProfiles.h>
#include <SpaceGame/levels/ExampleLevels.h>
#include <SpaceGame/levels/LevelLoader.h>
#include <SpaceGame/ships/player/SpaceGamePlayerController.h>
#include <SpaceGame/ships/player/TestSpaceShip.h>
#include <SpaceGame/simulation/TestBatchOrchestrator.h>

#include <CQTest.h>
#include <Engine/LocalPlayer.h>
#include <EnhancedInputComponent.h>
#include <EnhancedInputSubsystems.h>
#include <InputAction.h>
#include <InputMappingContext.h>
#include <UserSettings/EnhancedInputUserSettings.h>

TEST_CLASS(PlayerInputSmoke, "Sandbox.LevelTests")
{
    inline static ml::FTestBatchOrchestratorLevelSetup level_setup{};
    inline static FTimespan const timeout{FTimespan::FromSeconds(2)};

    ml::FSoftTestAssertions checks{};
    TWeakObjectPtr<ASpaceGamePlayerController> controller_{nullptr};
    TWeakObjectPtr<UEnhancedInputLocalPlayerSubsystem> input_subsystem_{nullptr};
    TWeakObjectPtr<UEnhancedInputUserSettings> input_settings_{nullptr};
    FString original_profile_id_{};
    TArray<FKey> pressed_keys_{};
    int32 release_wait_ticks_{0};

    BEFORE_EACH()
    {
        checks.test_runner = TestRunner;
        checks.all_passed = true;
        level_setup.begin_test(TestCommandBuilder, *TestRunner, checks);
    }

    AFTER_EACH()
    {
        release_pressed_keys();
        restore_profile();
        level_setup.end_test();
        controller_.Reset();
        input_subsystem_.Reset();
        input_settings_.Reset();
    }

    AFTER_ALL()
    { level_setup.teardown(); }
  private:
    auto setup(FString const& profile_id, bool const start_simulation = true) -> bool {
        auto* const orchestrator{level_setup.get_orchestrator()};
        if (!checks.is_valid(orchestrator, TEXT("Input smoke orchestrator is available"))) {
            return false;
        }

        ml::FLevelLoader loader{*orchestrator};
        auto const load_result{loader.load(ml::example_levels::make_native_example())};
        if (!checks.is_true(static_cast<bool>(load_result), TEXT("Input smoke level loads"))) {
            return false;
        }
        auto* const controller{
            Cast<ASpaceGamePlayerController>(level_setup.get_world().GetFirstPlayerController())};
        if (!checks.is_valid(controller, TEXT("Production player controller is active"))) {
            return false;
        }
        controller_ = controller;
        if (!checks.is_true(controller->GetClass()->GetOutermost()->GetName() ==
                                TEXT("/SpaceGame/Players/BP_SpaceGamePlayerController"),
                            TEXT("Smoke test uses the canonical runtime controller Blueprint")) ||
            !checks.is_true(IsValid(Cast<UEnhancedInputComponent>(controller->InputComponent)),
                            TEXT("Production Enhanced Input component is active")) ||
            !checks.is_true(controller->GetPawn().Get() == orchestrator->get_player_ship(),
                            TEXT("Player controller possesses the loaded player ship"))) {
            return false;
        }

        auto* const local_player{controller->GetLocalPlayer()};
        auto* const subsystem{
            IsValid(local_player)
                ? ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(local_player)
                : nullptr};
        auto* const settings{IsValid(subsystem) ? subsystem->GetUserSettings() : nullptr};
        if (!checks.is_true(IsValid(subsystem),
                            TEXT("Enhanced Input local-player subsystem is active")) ||
            !checks.is_true(IsValid(settings), TEXT("Enhanced Input user settings are active"))) {
            return false;
        }
        input_subsystem_ = subsystem;
        input_settings_ = settings;
        original_profile_id_ = settings->GetActiveKeyProfileId();

        FGameplayTagContainer failure_reason;
        settings->ResetKeyProfileIdToDefault(profile_id, failure_reason);
        auto const profile_selected{settings->GetActiveKeyProfileId() == profile_id ||
                                    settings->SetActiveKeyProfile(profile_id)};
        if (!checks.is_true(failure_reason.IsEmpty(),
                            TEXT("Authored control profile resets for the smoke test")) ||
            !checks.is_true(profile_selected, TEXT("Requested control profile becomes active"))) {
            return false;
        }
        FModifyContextOptions rebuild_options;
        rebuild_options.bForceImmediately = true;
        subsystem->RequestRebuildControlMappings(rebuild_options,
                                                 EInputMappingRebuildType::RebuildWithFlush);
        if (start_simulation) {
            orchestrator->start_simulation();
        }
        return true;
    }

    auto input_is_ready() const -> bool {
        auto const* const controller{controller_.Get()};
        auto const* const subsystem{input_subsystem_.Get()};
        auto const* const ship{IsValid(controller) ? Cast<ATestSpaceShip>(controller->GetPawn())
                                                   : nullptr};
        if (!IsValid(controller) || !IsValid(subsystem) || !IsValid(ship) ||
            !ship->has_simulation()) {
            return false;
        }
        auto const& ship_input{FPlayerControllerTestAccess::ship_input(*controller)};
        auto const& global_input{FPlayerControllerTestAccess::global_input(*controller)};
        return controller->get_active_control_context() == EPlayerControlContext::Player &&
               subsystem->HasMappingContext(ship_input.get_mapping_context()) &&
               subsystem->HasMappingContext(global_input.mapping_context);
    }

    void check_input_is_ready() {
        checks.is_true(input_is_ready(),
                       TEXT("Simulation-bound player input contexts become active"));
    }

    void restore_profile() {
        auto* const settings{input_settings_.Get()};
        auto* const subsystem{input_subsystem_.Get()};
        if (!IsValid(settings) || !IsValid(subsystem) || original_profile_id_.IsEmpty()) {
            return;
        }
        settings->SetActiveKeyProfile(original_profile_id_);
        FModifyContextOptions rebuild_options;
        rebuild_options.bForceImmediately = true;
        subsystem->RequestRebuildControlMappings(rebuild_options,
                                                 EInputMappingRebuildType::RebuildWithFlush);
        original_profile_id_.Reset();
    }

    auto has_ship_mapping(
        FString const& profile_id, UInputAction const* const action, FKey const key) const -> bool {
        auto const* const controller{controller_.Get()};
        if (!IsValid(controller) || !IsValid(action)) {
            return false;
        }
        UInputMappingContext const* const context{
            FPlayerControllerTestAccess::ship_input(*controller).get_mapping_context()};
        if (!IsValid(context)) {
            return false;
        }
        auto const has_mapping = [action, key](auto const& mappings) {
            return mappings.ContainsByPredicate([action, key](auto const& mapping) {
                return mapping.Action == action && mapping.Key == key;
            });
        };
        auto const default_profile{ml::ioj::control_profile_definitions()[0].id};
        auto const& mappings{profile_id == default_profile
                                 ? context->GetMappings()
                                 : context->GetMappingsForProfile(profile_id)};
        return has_mapping(mappings);
    }

    auto has_global_mapping(UInputAction const* const action, FKey const key) const -> bool {
        auto const* const controller{controller_.Get()};
        if (!IsValid(controller) || !IsValid(action)) {
            return false;
        }
        auto const* const context{
            FPlayerControllerTestAccess::global_input(*controller).mapping_context};
        return IsValid(context) &&
               context->GetMappings().ContainsByPredicate([action, key](auto const& mapping) {
                   return mapping.Action == action && mapping.Key == key;
               });
    }

    void press_key(FKey const key, FVector const value = FVector{1.0, 0.0, 0.0}) {
        auto* const controller{controller_.Get()};
        if (!checks.is_true(IsValid(controller), TEXT("Player controller accepts input"))) {
            return;
        }
        if (!pressed_keys_.Contains(key)) {
            pressed_keys_.Add(key);
        }
        FString command;
        if (EKeys::GetPairedKeyDetails(key) != nullptr) {
            command =
                FString::Printf(TEXT("Input.+key %s X=%f Y=%f"), *key.ToString(), value.X, value.Y);
        } else if (key.IsAnalog()) {
            command = FString::Printf(TEXT("Input.+key %s %f"), *key.ToString(), value.X);
        } else {
            command = FString::Printf(TEXT("Input.+key %s"), *key.ToString());
        }
        controller->ConsoleCommand(command, true);
    }

    void release_key(FKey const key) {
        auto* const controller{controller_.Get()};
        if (!pressed_keys_.Contains(key)) {
            return;
        }
        if (IsValid(controller)) {
            controller->ConsoleCommand(FString::Printf(TEXT("Input.-key %s"), *key.ToString()),
                                       true);
        }
        pressed_keys_.Remove(key);
    }

    void release_pressed_keys() {
        auto const keys{pressed_keys_};
        for (auto const key : keys) {
            release_key(key);
        }
    }

    auto snapshot() const -> FPlayerInputSnapshot {
        auto const* const controller{controller_.Get()};
        return IsValid(controller) ? controller->get_input_snapshot() : FPlayerInputSnapshot{};
    }

    auto keyboard_forward_is_active() const -> bool {
        auto const state{snapshot()};
        return FMath::IsNearlyZero(state.movement.X) && state.movement.Y > 0.0 &&
               state.turn.IsNearlyZero() && state.sampled_movement.IsNearlyZero();
    }

    auto keyboard_backward_is_active() const -> bool {
        auto const state{snapshot()};
        return FMath::IsNearlyZero(state.movement.X) && state.movement.Y < 0.0 &&
               state.turn.IsNearlyZero() && state.sampled_movement.IsNearlyZero();
    }

    auto keyboard_left_is_active() const -> bool {
        auto const state{snapshot()};
        return state.movement.X < 0.0 && FMath::IsNearlyZero(state.movement.Y) &&
               state.turn.IsNearlyZero() && state.sampled_movement.IsNearlyZero();
    }

    auto keyboard_right_is_active() const -> bool {
        auto const state{snapshot()};
        return state.movement.X > 0.0 && FMath::IsNearlyZero(state.movement.Y) &&
               state.turn.IsNearlyZero() && state.sampled_movement.IsNearlyZero();
    }

    auto ship_axes_are_clear() const -> bool {
        auto const state{snapshot()};
        return state.movement.IsNearlyZero() && state.turn.IsNearlyZero();
    }

    auto fire_is_active(bool const expected) const -> bool {
        return controller_.IsValid() && snapshot().fire_active == expected;
    }

    void run_fire_test(FString profile_id, FKey const key, FVector const value) {
        TestCommandBuilder
            .Do([this, profile_id, key, value] {
                if (!setup(profile_id)) {
                    return;
                }
            })
            .Until([this] { return !checks.all_passed || input_is_ready(); }, timeout)
            .Then([this, profile_id = MoveTemp(profile_id), key, value] {
                check_input_is_ready();
                auto const* const fire_action{
                    FPlayerControllerTestAccess::ship_input(*controller_).fire_laser};
                if (!checks.is_true(has_ship_mapping(profile_id, fire_action, key),
                                    TEXT("Authored profile maps the tested key to fire"))) {
                    return;
                }
                checks.is_true(fire_is_active(false), TEXT("Fire starts inactive"));
                press_key(key, value);
            })
            .Until([this] { return !checks.all_passed || fire_is_active(true); }, timeout)
            .Then([this, key] {
                checks.is_true(fire_is_active(true),
                               TEXT("Physical fire key reaches controller state"));
                release_key(key);
            })
            .Until([this] { return !checks.all_passed || fire_is_active(false); }, timeout)
            .Then([this] {
                checks.is_true(fire_is_active(false),
                               TEXT("Releasing fire clears controller state"));
            });
    }
  public:
    TEST_METHOD(KeyboardMovementContract)
    {
        auto const default_profile{ml::ioj::control_profile_definitions()[0].id};
        TestCommandBuilder
            .Do([this, default_profile] {
                if (!setup(default_profile, false)) {
                    return;
                }
                auto const& ship_input{FPlayerControllerTestAccess::ship_input(*controller_)};
                checks.is_true(
                    has_ship_mapping(default_profile, ship_input.vertical_move, EKeys::W),
                    TEXT("Default profile maps W to vertical movement"));
                checks.is_true(ship_axes_are_clear(), TEXT("Ship input starts neutral"));
                press_key(EKeys::W);
                release_wait_ticks_ = 0;
            })
            .Until(
                [this] {
                    ++release_wait_ticks_;
                    return !checks.all_passed || release_wait_ticks_ > 1;
                },
                timeout)
            .Then([this] {
                checks.is_true(ship_axes_are_clear(),
                               TEXT("Held W cannot reach an unbound simulation"));
                if (auto* const orchestrator{level_setup.get_orchestrator()};
                    checks.is_valid(orchestrator, TEXT("Input smoke orchestrator remains valid"))) {
                    orchestrator->start_simulation();
                }
            })
            .Until(
                [this] {
                    return !checks.all_passed || (input_is_ready() && keyboard_forward_is_active());
                },
                timeout)
            .Then([this] {
                checks.is_true(keyboard_forward_is_active(),
                               TEXT("W produces forward movement without turning"));
                release_key(EKeys::W);
            })
            .Until([this] { return !checks.all_passed || ship_axes_are_clear(); }, timeout)
            .Then([this] {
                checks.is_true(ship_axes_are_clear(), TEXT("Releasing W clears movement"));
                press_key(EKeys::S);
            })
            .Until([this] { return !checks.all_passed || keyboard_backward_is_active(); }, timeout)
            .Then([this] {
                checks.is_true(keyboard_backward_is_active(),
                               TEXT("S produces backward movement without turning"));
                release_key(EKeys::S);
                press_key(EKeys::A);
            })
            .Until([this] { return !checks.all_passed || keyboard_left_is_active(); }, timeout)
            .Then([this] {
                checks.is_true(keyboard_left_is_active(),
                               TEXT("A produces left movement without turning"));
                release_key(EKeys::A);
            })
            .Until([this] { return !checks.all_passed || ship_axes_are_clear(); }, timeout)
            .Then([this] {
                checks.is_true(ship_axes_are_clear(), TEXT("Releasing A clears movement"));
                press_key(EKeys::D);
            })
            .Until([this] { return !checks.all_passed || keyboard_right_is_active(); }, timeout)
            .Then([this] {
                checks.is_true(keyboard_right_is_active(),
                               TEXT("D produces right movement without turning"));
                release_key(EKeys::D);
            })
            .Until([this] { return !checks.all_passed || ship_axes_are_clear(); }, timeout)
            .Then([this] {
                checks.is_true(ship_axes_are_clear(), TEXT("Releasing D clears movement"));
            });
    }

    TEST_METHOD(KeyboardSampleAndHold)
    {
        auto const default_profile{ml::ioj::control_profile_definitions()[0].id};
        TestCommandBuilder
            .Do([this, default_profile] {
                if (!setup(default_profile)) {
                    return;
                }
            })
            .Until([this] { return !checks.all_passed || input_is_ready(); }, timeout)
            .Then([this, default_profile] {
                check_input_is_ready();
                auto const& ship_input{FPlayerControllerTestAccess::ship_input(*controller_)};
                if (!checks.is_true(
                        has_ship_mapping(
                            default_profile, ship_input.sample_and_hold, EKeys::ThumbMouseButton2),
                        TEXT("Default profile maps mouse thumb 2 to sample-and-hold"))) {
                    return;
                }
                press_key(EKeys::ThumbMouseButton2);
            })
            .Until([this] { return !checks.all_passed || snapshot().sampling_active; }, timeout)
            .Then([this] {
                auto const state{snapshot()};
                checks.is_true(state.sampling_active,
                               TEXT("Mouse thumb 2 starts a sampling session"));
                checks.is_true(state.sampled_movement.IsNearlyZero(),
                               TEXT("Sampling starts from neutral"));
                press_key(EKeys::W);
                press_key(EKeys::D);
            })
            .Until(
                [this] {
                    auto const state{snapshot()};
                    return !checks.all_passed ||
                           (state.sampling_active && state.sampled_movement.X > 0.0 &&
                            state.sampled_movement.Y > 0.0);
                },
                timeout)
            .Then([this] {
                auto const state{snapshot()};
                checks.is_true(state.sampling_active,
                               TEXT("Sampling remains active while chorded keys are held"));
                checks.is_true(state.sampled_movement.X > 0.0 && state.sampled_movement.Y > 0.0,
                               TEXT("D and W sample right and forward"));
                checks.is_true(state.movement.IsNearlyZero() && state.turn.IsNearlyZero(),
                               TEXT("Chorded sampling does not leak into movement or turn"));
                release_key(EKeys::ThumbMouseButton2);
                release_key(EKeys::W);
                release_key(EKeys::D);
            })
            .Until([this] { return !checks.all_passed || !snapshot().sampling_active; }, timeout)
            .Then([this] {
                auto const state{snapshot()};
                checks.is_true(!state.sampling_active,
                               TEXT("Releasing sample-and-hold commits the sample"));
                checks.is_true(state.sampled_movement.X > 0.0 && state.sampled_movement.Y > 0.0,
                               TEXT("Committed sample retains its direction"));
                press_key(EKeys::ThumbMouseButton2);
            })
            .Until(
                [this] {
                    auto const state{snapshot()};
                    return !checks.all_passed ||
                           (state.sampling_active && state.sampled_movement.IsNearlyZero());
                },
                timeout)
            .Then([this] {
                auto const state{snapshot()};
                checks.is_true(state.sampling_active && state.sampled_movement.IsNearlyZero(),
                               TEXT("A new sampling session resets to neutral"));
                release_key(EKeys::ThumbMouseButton2);
            });
    }

    TEST_METHOD(PointerVirtualStickTurn)
    {
        auto const default_profile{ml::ioj::control_profile_definitions()[0].id};
        TestCommandBuilder
            .Do([this, default_profile] {
                if (!setup(default_profile)) {
                    return;
                }
            })
            .Until([this] { return !checks.all_passed || input_is_ready(); }, timeout)
            .Then([this, default_profile] {
                check_input_is_ready();
                auto const& ship_input{FPlayerControllerTestAccess::ship_input(*controller_)};
                auto const mappings_valid{
                    checks.is_true(has_ship_mapping(default_profile,
                                                    ship_input.turn_pointer_delta,
                                                    EKeys::Mouse2D),
                                   TEXT("Default profile maps Mouse2D to pointer displacement")) &&
                    checks.is_true(has_ship_mapping(default_profile,
                                                    ship_input.engage_pointer_turn,
                                                    EKeys::RightMouseButton),
                                   TEXT("Default profile maps right mouse to pointer engagement"))};
                if (!mappings_valid) {
                    return;
                }
                press_key(EKeys::Mouse2D, FVector{20.0, 0.0, 0.0});
                release_wait_ticks_ = 0;
            })
            .Until(
                [this] {
                    ++release_wait_ticks_;
                    return !checks.all_passed || release_wait_ticks_ > 2;
                },
                timeout)
            .Then([this] {
                checks.is_true(snapshot().turn.IsNearlyZero(),
                               TEXT("Pointer displacement is ignored until RMB is held"));
                release_key(EKeys::Mouse2D);
                press_key(EKeys::RightMouseButton);
                release_wait_ticks_ = 0;
            })
            .Until(
                [this] {
                    ++release_wait_ticks_;
                    return !checks.all_passed || release_wait_ticks_ > 1;
                },
                timeout)
            .Then([this] { press_key(EKeys::Mouse2D, FVector{20.0, 0.0, 0.0}); })
            .Until(
                [this] {
                    auto const turn{snapshot().turn};
                    return !checks.all_passed || !FMath::IsNearlyZero(turn.X);
                },
                timeout)
            .Then([this] {
                auto const turn{snapshot().turn};
                checks.is_true(turn.X > 0.0 && FMath::IsNearlyZero(turn.Y),
                               TEXT("Horizontal pointer displacement produces horizontal turn"));
                release_key(EKeys::Mouse2D);
                release_wait_ticks_ = 0;
            })
            .Until(
                [this] {
                    ++release_wait_ticks_;
                    return !checks.all_passed || release_wait_ticks_ > 1;
                },
                timeout)
            .Then([this] {
                checks.is_true(!FMath::IsNearlyZero(snapshot().turn.X),
                               TEXT("Pointer displacement remains a held turn rate"));
                release_key(EKeys::RightMouseButton);
            })
            .Until([this] { return !checks.all_passed || snapshot().turn.IsNearlyZero(); }, timeout)
            .Then([this] {
                checks.is_true(snapshot().turn.IsNearlyZero(),
                               TEXT("Releasing pointer engagement clears turn"));
                press_key(EKeys::RightMouseButton);
                release_wait_ticks_ = 0;
            })
            .Until(
                [this] {
                    ++release_wait_ticks_;
                    return !checks.all_passed || release_wait_ticks_ > 1;
                },
                timeout)
            .Then([this] { press_key(EKeys::Mouse2D, FVector{0.0, 20.0, 0.0}); })
            .Until([this] { return !checks.all_passed || !FMath::IsNearlyZero(snapshot().turn.Y); },
                   timeout)
            .Then([this] {
                auto const turn{snapshot().turn};
                checks.is_true(FMath::IsNearlyZero(turn.X) && turn.Y > 0.0,
                               TEXT("Vertical pointer displacement produces vertical turn"));
                release_key(EKeys::Mouse2D);
                release_key(EKeys::RightMouseButton);
            });
    }

    TEST_METHOD(MouseFirePressAndRelease)
    {
        run_fire_test(ml::ioj::control_profile_definitions()[0].id,
                      EKeys::LeftMouseButton,
                      FVector{1.0, 0.0, 0.0});
    }

    TEST_METHOD(GamepadMovementAndFire)
    {
        auto const profile{ml::ioj::control_profile_definitions()[1].id};
        TestCommandBuilder
            .Do([this, profile] {
                if (!setup(profile)) {
                    return;
                }
            })
            .Until([this] { return !checks.all_passed || input_is_ready(); }, timeout)
            .Then([this, profile] {
                check_input_is_ready();
                auto const& ship_input{FPlayerControllerTestAccess::ship_input(*controller_)};
                checks.is_true(has_ship_mapping(profile, ship_input.move, EKeys::Gamepad_Right2D),
                               TEXT("Gamepad profile maps the right stick to movement"));
                checks.is_true(has_ship_mapping(
                                   profile, ship_input.fire_laser, EKeys::Gamepad_RightTriggerAxis),
                               TEXT("Gamepad profile maps the right trigger to fire"));
                press_key(EKeys::Gamepad_Right2D, FVector{0.65, 0.8, 0.0});
            })
            .Until(
                [this] {
                    auto const movement{snapshot().movement};
                    return !checks.all_passed || (movement.X > 0.0 && movement.Y > 0.0);
                },
                timeout)
            .Then([this] {
                auto const movement{snapshot().movement};
                checks.is_true(movement.X > 0.0 && movement.Y > 0.0,
                               TEXT("Gamepad stick reaches movement state"));
                release_key(EKeys::Gamepad_Right2D);
            })
            .Until([this] { return !checks.all_passed || snapshot().movement.IsNearlyZero(); },
                   timeout)
            .Then([this] {
                checks.is_true(snapshot().movement.IsNearlyZero(),
                               TEXT("Gamepad stick release clears movement"));
                press_key(EKeys::Gamepad_RightTriggerAxis);
            })
            .Until([this] { return !checks.all_passed || fire_is_active(true); }, timeout)
            .Then([this] {
                checks.is_true(fire_is_active(true), TEXT("Gamepad trigger reaches fire state"));
                release_key(EKeys::Gamepad_RightTriggerAxis);
            })
            .Until([this] { return !checks.all_passed || fire_is_active(false); }, timeout)
            .Then([this] {
                checks.is_true(fire_is_active(false),
                               TEXT("Gamepad trigger release clears fire state"));
            });
    }

    TEST_METHOD(GamepadPauseAndResume)
    {
        auto const default_profile{ml::ioj::control_profile_definitions()[0].id};
        TestCommandBuilder
            .Do([this, default_profile] {
                if (!setup(default_profile)) {
                    return;
                }
            })
            .Until([this] { return !checks.all_passed || input_is_ready(); }, timeout)
            .Then([this] {
                check_input_is_ready();
                auto const& global_input{FPlayerControllerTestAccess::global_input(*controller_)};
                if (!checks.is_true(
                        has_global_mapping(global_input.toggle_menu, EKeys::Gamepad_Special_Right),
                        TEXT("Global context maps gamepad Start to pause"))) {
                    return;
                }
                auto const snapshot{controller_->get_input_snapshot()};
                checks.is_true(!snapshot.pause_menu_active, TEXT("Pause menu starts inactive"));
                checks.is_true(snapshot.control_context == EPlayerControlContext::Player,
                               TEXT("Player context starts active"));
                press_key(EKeys::Gamepad_Special_Right);
            })
            .Until(
                [this] {
                    if (!checks.all_passed || !controller_.IsValid()) {
                        return true;
                    }
                    auto const snapshot{controller_->get_input_snapshot()};
                    return snapshot.pause_menu_active &&
                           snapshot.control_context == EPlayerControlContext::None;
                },
                timeout)
            .Then([this] {
                auto const snapshot{controller_->get_input_snapshot()};
                checks.is_true(snapshot.pause_menu_active,
                               TEXT("Gamepad Start opens the pause path"));
                checks.is_true(snapshot.control_context == EPlayerControlContext::None,
                               TEXT("Pause suspends player input context"));
                release_key(EKeys::Gamepad_Special_Right);
                release_wait_ticks_ = 0;
            })
            .Until(
                [this] {
                    ++release_wait_ticks_;
                    return release_wait_ticks_ > 1;
                },
                timeout)
            .Then([this] { press_key(EKeys::Gamepad_Special_Right); })
            .Until(
                [this] {
                    if (!checks.all_passed || !controller_.IsValid()) {
                        return true;
                    }
                    auto const snapshot{controller_->get_input_snapshot()};
                    return !snapshot.pause_menu_active &&
                           snapshot.control_context == EPlayerControlContext::Player;
                },
                timeout)
            .Then([this] {
                auto const snapshot{controller_->get_input_snapshot()};
                checks.is_true(!snapshot.pause_menu_active,
                               TEXT("Second gamepad Start closes pause"));
                checks.is_true(snapshot.control_context == EPlayerControlContext::Player,
                               TEXT("Closing pause restores player input context"));
                release_key(EKeys::Gamepad_Special_Right);
            });
    }
};
