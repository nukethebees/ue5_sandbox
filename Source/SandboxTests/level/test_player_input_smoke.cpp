#include <ioj/sim/player/sim.h>
#include <SandboxTests/support/PlayerControllerTestAccess.h>
#include <SandboxTests/support/test_setup.h>
#include <SpaceGame/input/CanonicalShipControls.h>
#include <SpaceGame/input/SpaceGameInputModifier.h>
#include <SpaceGame/input/SpaceGameInputUserSettings.h>
#include <SpaceGame/levels/ExampleLevels.h>
#include <SpaceGame/levels/LevelLoader.h>
#include <SpaceGame/settings/GameSettingsSubsystem.h>
#include <SpaceGame/ships/player/SpaceGamePlayerController.h>
#include <SpaceGame/ships/player/TestSpaceShip.h>
#include <SpaceGame/simulation/TestBatchOrchestrator.h>
#include <SpaceGame/ui/common/InputActionRouter.h>

#include <CommonInputSubsystem.h>
#include <CQTest.h>
#include <Engine/GameViewportClient.h>
#include <Engine/LocalPlayer.h>
#include <EnhancedInputComponent.h>
#include <EnhancedInputDeveloperSettings.h>
#include <EnhancedInputSubsystems.h>
#include <EnhancedPlayerInput.h>
#include <Framework/Application/SlateApplication.h>
#include <Framework/Application/SlateUser.h>
#include <HAL/IConsoleManager.h>
#include <Input/CommonAnalogCursor.h>
#include <InputAction.h>
#include <InputKeyEventArgs.h>
#include <InputMappingContext.h>
#include <Kismet/GameplayStatics.h>
#include <Misc/Guid.h>
#include <PlayerMappableKeySettings.h>
#include <Slate/SceneViewport.h>
#include <UObject/UnrealType.h>
#include <UserSettings/EnhancedInputUserSettings.h>
#include <Widgets/SViewport.h>

TEST_CLASS(PlayerInputSmoke, "Sandbox.LevelTests")
{
    inline static ml::FTestBatchOrchestratorLevelSetup level_setup{};
    inline static FTimespan const timeout{FTimespan::FromSeconds(2)};

    ml::FSoftTestAssertions checks{};
    TWeakObjectPtr<ASpaceGamePlayerController> controller_{};
    TWeakObjectPtr<UEnhancedInputLocalPlayerSubsystem> subsystem_{};
    TWeakObjectPtr<UEnhancedInputUserSettings> settings_{};
    FString active_profile_id_{};
    FString original_save_slot_{};
    FString test_save_slot_{};
    ml::ioj::FControlBindingAddress remap_address_{};
    FKey original_remap_key_{};
    FKey original_default_key_{};
    int32 test_user_index_{};
    bool restore_remap_{};
    bool restore_pitch_inversion_{};
    bool original_pitch_inversion_{};
    bool restore_other_inversions_{};
    bool original_yaw_inversion_{};
    bool original_roll_inversion_{};
    bool original_vertical_inversion_{};
    bool used_slate_input_{};
    int32 original_accept_simulation_{};
    FIntPoint original_viewport_size_{};

    BEFORE_EACH()
    {
        checks.test_runner = TestRunner;
        checks.all_passed = true;
        level_setup.begin_test(TestCommandBuilder, *TestRunner, checks);
    }

    AFTER_EACH()
    {
        if (used_slate_input_ && controller_.IsValid()) {
            send_slate_key(EKeys::Gamepad_FaceButton_Bottom, false);
            for (auto const key : {EKeys::Gamepad_LeftX,
                                   EKeys::Gamepad_LeftY,
                                   EKeys::Gamepad_RightX,
                                   EKeys::Gamepad_RightY}) {
                send_slate_axis(key, 0.0f);
            }
            IConsoleManager::Get()
                .FindConsoleVariable(TEXT("CommonUI.ShouldVirtualAcceptSimulateMouseButton"))
                ->Set(original_accept_simulation_, ECVF_SetByCode);
            controller_->GetLocalPlayer()->ViewportClient->GetGameViewport()->SetViewportSize(
                original_viewport_size_.X, original_viewport_size_.Y);
        }
        if (restore_remap_) {
            if (auto* const settings{settings_.Get()}) {
                FMapPlayerKeyArgs args{};
                args.MappingName = remap_address_.mapping_name;
                args.Slot = remap_address_.slot;
                args.HardwareDeviceId = remap_address_.hardware_device_id;
                args.ProfileIdString = remap_address_.profile_id;
                FGameplayTagContainer failure_reason;
                if (original_remap_key_ == original_default_key_) {
                    settings->UnMapPlayerKey(args, failure_reason);
                } else {
                    args.NewKey = original_remap_key_;
                    settings->MapPlayerKey(args, failure_reason);
                }
                settings->ApplySettings();
            }
        }
        if (restore_pitch_inversion_) {
            if (auto* const input_settings{
                    Cast<ml::ioj::USpaceGameInputUserSettings>(settings_.Get())}) {
                input_settings->set_invert_gamepad_pitch(original_pitch_inversion_);
                input_settings->ApplySettings();
            }
        }
        if (restore_other_inversions_) {
            if (auto* const input_settings{
                    Cast<ml::ioj::USpaceGameInputUserSettings>(settings_.Get())}) {
                input_settings->set_invert_gamepad_yaw(original_yaw_inversion_);
                input_settings->set_invert_gamepad_roll(original_roll_inversion_);
                input_settings->set_invert_gamepad_vertical_translation(
                    original_vertical_inversion_);
                input_settings->ApplySettings();
            }
        }
        if (!test_save_slot_.IsEmpty()) {
            GetMutableDefault<UEnhancedInputDeveloperSettings>()->InputSettingsSaveSlotName =
                original_save_slot_;
            UGameplayStatics::DeleteGameInSlot(test_save_slot_, test_user_index_);
        }
        if (auto* const controller{controller_.Get()}) {
            controller->ConsoleCommand(TEXT("Input.-key Four"), true);
            controller->ConsoleCommand(TEXT("Input.-key Gamepad_FaceButton_Bottom"), true);
            controller->InputKey(FInputKeyEventArgs::CreateSimulated(
                EKeys::Gamepad_FaceButton_Bottom, IE_Released, 0.0f));
            controller->ConsoleCommand(TEXT("Input.-key Gamepad_LeftY"), true);
            controller->ConsoleCommand(TEXT("Input.-key Gamepad_RightY"), true);
            controller->ConsoleCommand(TEXT("Input.-key Gamepad_RightX"), true);
            controller->ConsoleCommand(TEXT("Input.-key Gamepad_LeftX"), true);
            controller->ConsoleCommand(TEXT("Input.-key C"), true);
            controller->ConsoleCommand(TEXT("Input.-key One"), true);
            controller->ConsoleCommand(TEXT("Input.-key Two"), true);
        }
        level_setup.end_test();
        controller_.Reset();
        subsystem_.Reset();
        settings_.Reset();
        active_profile_id_.Reset();
        original_save_slot_.Reset();
        test_save_slot_.Reset();
        restore_remap_ = false;
        restore_pitch_inversion_ = false;
        restore_other_inversions_ = false;
        used_slate_input_ = false;
    }

    AFTER_ALL()
    { level_setup.teardown(); }

    auto setup() -> bool {
        auto* const orchestrator{level_setup.get_orchestrator()};
        if (!checks.is_valid(orchestrator, TEXT("Orchestrator exists"))) {
            return false;
        }
        ml::FLevelLoader loader{*orchestrator};
        if (!checks.is_true(
                static_cast<bool>(loader.load(ml::example_levels::make_native_example())),
                TEXT("Gameplay level loads directly"))) {
            return false;
        }
        auto* const controller{
            Cast<ASpaceGamePlayerController>(level_setup.get_world().GetFirstPlayerController())};
        if (!checks.is_valid(controller, TEXT("Production player controller is active"))) {
            return false;
        }
        controller_ = controller;
        auto* const local_player{controller->GetLocalPlayer()};
        auto* const subsystem{
            IsValid(local_player)
                ? ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(local_player)
                : nullptr};
        auto* const settings{IsValid(subsystem) ? subsystem->GetUserSettings() : nullptr};
        if (!checks.is_true(IsValid(subsystem), TEXT("Enhanced Input subsystem exists")) ||
            !checks.is_true(IsValid(settings), TEXT("Persistent input settings exist"))) {
            return false;
        }
        subsystem_ = subsystem;
        settings_ = settings;
        checks.is_true(IsValid(Cast<UEnhancedInputComponent>(controller->InputComponent)),
                       TEXT("Production Enhanced Input component is active"));
        checks.is_true(controller->GetClass()->GetOutermost()->GetName() ==
                           TEXT("/SpaceGame/Players/BP_SpaceGamePlayerController"),
                       TEXT("Canonical runtime controller Blueprint is used"));
        for (auto const& definition : ml::ioj::canonical_ship_control_contexts()) {
            auto* const context{ml::ioj::load_ship_control_context(definition.scope)};
            checks.is_true(IsValid(context) && settings->IsMappingContextRegistered(context),
                           TEXT("Canonical context registered without opening Options"));
        }
        orchestrator->start_simulation();
        return checks.all_passed;
    }

    auto ready() const -> bool {
        auto const* const controller{controller_.Get()};
        auto const* const subsystem{subsystem_.Get()};
        auto const* const ship{IsValid(controller) ? Cast<ATestSpaceShip>(controller->GetPawn())
                                                   : nullptr};
        return IsValid(subsystem) && IsValid(ship) && ship->has_simulation() &&
               controller->get_active_control_context() == EPlayerControlContext::Player &&
               subsystem->HasMappingContext(
                   ml::ioj::load_ship_control_context(ml::ioj::EShipControlScope::General)) &&
               subsystem->HasMappingContext(
                   ml::ioj::load_ship_control_context(ml::ioj::EShipControlScope::Starfox));
    }

    void check_active_contexts(ml::ioj::EShipControlScope const mode) {
        auto const* const controller{controller_.Get()};
        auto const* const input{
            IsValid(controller) ? Cast<UEnhancedPlayerInput>(controller->PlayerInput) : nullptr};
        if (!checks.is_true(IsValid(input), TEXT("Enhanced PlayerInput exists"))) {
            return;
        }
        auto const* const property{FindFProperty<FMapProperty>(UEnhancedPlayerInput::StaticClass(),
                                                               TEXT("AppliedInputContextData"))};
        if (!checks.is_true(property != nullptr, TEXT("Applied IMC state is inspectable"))) {
            return;
        }
        FScriptMapHelper const active{property, property->ContainerPtrToValuePtr<void>(input)};
        checks.is_true(active.Num() == 2, TEXT("Exactly General and one flight IMC are active"));
        auto const* const key_property{CastFieldChecked<FObjectProperty>(property->KeyProp)};
        for (auto index{active.CreateIterator()}; index; ++index) {
            auto const* const context{Cast<UInputMappingContext>(
                key_property->GetObjectPropertyValue(active.GetKeyPtr(index)))};
            checks.is_true(context == ml::ioj::load_ship_control_context(
                                          ml::ioj::EShipControlScope::General) ||
                               context == ml::ioj::load_ship_control_context(mode),
                           TEXT("No Blueprint or retired ship IMC is active"));
        }
    }

    void check_single_ship_action_bindings() {
        auto const* const controller{controller_.Get()};
        auto const* const component{IsValid(controller)
                                        ? Cast<UEnhancedInputComponent>(controller->InputComponent)
                                        : nullptr};
        if (!checks.is_true(IsValid(component), TEXT("Enhanced Input component exists"))) {
            return;
        }
        for (auto const& [name, expected] : {std::pair{TEXT("IA_Ship_Pitch"), 3},
                                             std::pair{TEXT("IA_Ship_TranslateUp"), 3},
                                             std::pair{TEXT("IA_Ship_FirePrimary"), 3},
                                             std::pair{TEXT("IA_Ship_SelectGunship"), 1}}) {
            int32 count{};
            for (auto const& binding : component->GetActionEventBindings()) {
                if (IsValid(binding->GetAction()) && binding->GetAction()->GetName() == name) {
                    ++count;
                }
            }
            checks.is_true(count == expected,
                           FString::Printf(TEXT("%s has only its canonical C++ bindings"), name));
        }
    }

    auto native_pitch() const -> float {
        auto const* const orchestrator{level_setup.get_orchestrator()};
        auto const* const sim{IsValid(orchestrator) ? orchestrator->get_player_ship_simulation()
                                                    : nullptr};
        return sim != nullptr ? sim->get_flight_intent().rotation.x : 0.0f;
    }

    auto native_yaw() const -> float {
        auto const* const sim{level_setup.get_orchestrator()->get_player_ship_simulation()};
        return sim != nullptr ? sim->get_flight_intent().rotation.y : 0.0f;
    }

    auto native_roll() const -> float {
        auto const* const sim{level_setup.get_orchestrator()->get_player_ship_simulation()};
        return sim != nullptr ? sim->get_flight_intent().rotation.z : 0.0f;
    }

    auto native_vertical() const -> float {
        auto const* const sim{level_setup.get_orchestrator()->get_player_ship_simulation()};
        return sim != nullptr ? sim->get_flight_intent().translation.z : 0.0f;
    }

    auto native_rotation(ml::ioj::ESpaceGameInputAxis const axis) const -> float {
        switch (axis) {
            case ml::ioj::ESpaceGameInputAxis::Pitch:
                return native_pitch();
            case ml::ioj::ESpaceGameInputAxis::Yaw:
                return native_yaw();
            case ml::ioj::ESpaceGameInputAxis::Roll:
                return native_roll();
            default:
                return 0.0f;
        }
    }

    auto prepare_slate_input() -> bool {
        auto* const local_player{controller_->GetLocalPlayer()};
        auto* const router{local_player->GetSubsystem<UCommonUIActionRouterBase>()};
        auto* const accept_simulation{IConsoleManager::Get().FindConsoleVariable(
            TEXT("CommonUI.ShouldVirtualAcceptSimulateMouseButton"))};
        if (!checks.is_true(IsValid(router) && router->IsA<ml::ioj::UInputActionRouter>(),
                            TEXT("Production CommonUI router is installed")) ||
            !checks.is_true(accept_simulation != nullptr, TEXT("CommonUI Accept CVar exists"))) {
            return false;
        }
        original_accept_simulation_ = accept_simulation->GetInt();
        accept_simulation->Set(1, ECVF_SetByCode);
        used_slate_input_ = true;
        local_player->GetSubsystem<UCommonInputSubsystem>()->SetCurrentInputType(
            ECommonInputType::Gamepad);
        auto* const viewport{local_player->ViewportClient->GetGameViewport()};
        original_viewport_size_ = viewport->GetSizeXY();
        // NullRHI leaves the viewport at zero size, which discards physical key events.
        if (original_viewport_size_ == FIntPoint::ZeroValue) {
            viewport->SetViewportSize(1280, 720);
        }
        checks.is_true(viewport->GetSizeXY() != FIntPoint::ZeroValue,
                       TEXT("Scene viewport accepts physical key events"));
        FSlateApplication::Get().SetUserFocus(
            local_player->GetSlateUser()->GetUserIndex(),
            local_player->ViewportClient->GetGameViewportWidget());
        checks.is_true(local_player->GetSlateUser()->GetFocusedWidget() ==
                           local_player->ViewportClient->GetGameViewportWidget(),
                       TEXT("PIE viewport is attached to Slate and receives focus"));
        checks.is_true(router->GetActiveInputMode() == ECommonInputMode::Game,
                       TEXT("Gameplay owns the CommonUI input mode"));
        return checks.all_passed;
    }

    void send_slate_key(FKey const key, bool const pressed) {
        auto const user{
            static_cast<uint32>(controller_->GetLocalPlayer()->GetSlateUser()->GetUserIndex())};
        FKeyEvent const event{key, FModifierKeysState{}, user, false, 0, 0};
        auto& slate{FSlateApplication::Get()};
        if (pressed) {
            slate.ProcessKeyDownEvent(event);
        } else {
            slate.ProcessKeyUpEvent(event);
        }
    }

    void send_slate_axis(FKey const key, float const value) {
        auto const user{
            static_cast<uint32>(controller_->GetLocalPlayer()->GetSlateUser()->GetUserIndex())};
        FAnalogInputEvent const event{key, FModifierKeysState{}, user, false, 0, 0, value};
        FSlateApplication::Get().ProcessAnalogInputEvent(event);
    }

    auto canonical_remap_reloaded(FKey const expected_key) const -> bool {
        auto* const controller{controller_.Get()};
        if (!IsValid(controller) || test_save_slot_.IsEmpty() ||
            !UGameplayStatics::DoesSaveGameExist(test_save_slot_, test_user_index_)) {
            return false;
        }
        auto* const loaded{
            UEnhancedInputUserSettings::LoadOrCreateSettings(controller->GetLocalPlayer())};
        if (!IsValid(loaded)) {
            return false;
        }
        for (auto const& definition : ml::ioj::canonical_ship_control_contexts()) {
            loaded->RegisterInputMappingContext(
                ml::ioj::load_ship_control_context(definition.scope));
        }
        auto const* const profile{loaded->GetDefaultKeyProfile()};
        auto const* const row{
            IsValid(profile) ? profile->GetPlayerMappingRows().Find(remap_address_.mapping_name)
                             : nullptr};
        if (row == nullptr || profile->GetProfileIdString() != remap_address_.profile_id) {
            return false;
        }
        for (auto const& mapping : row->Mappings) {
            if (mapping.GetCurrentKey() == expected_key &&
                mapping.GetPrimaryDeviceType() == EHardwareDevicePrimaryType::Gamepad &&
                mapping.GetHardwareDeviceId().HardwareDeviceIdentifier ==
                    remap_address_.hardware_device_id) {
                return true;
            }
        }
        return false;
    }

    auto inversion_reloaded() const -> bool {
        auto* const controller{controller_.Get()};
        auto const* const loaded{IsValid(controller)
                                     ? Cast<ml::ioj::USpaceGameInputUserSettings>(
                                           UEnhancedInputUserSettings::LoadOrCreateSettings(
                                               controller->GetLocalPlayer()))
                                     : nullptr};
        return IsValid(loaded) && loaded->invert_gamepad_pitch() == !original_pitch_inversion_ &&
               loaded->invert_gamepad_yaw() == !original_yaw_inversion_ &&
               loaded->invert_gamepad_roll() == !original_roll_inversion_;
    }

    TEST_METHOD(DirectGameplayStartupAndModeSwitch)
    {
        TestCommandBuilder.Do([this] { setup(); })
            .Until([this] { return !checks.all_passed || ready(); }, timeout)
            .Then([this] {
                if (!checks.is_true(ready(), TEXT("Starfox context activates after possession"))) {
                    return;
                }
                check_active_contexts(ml::ioj::EShipControlScope::Starfox);
                check_single_ship_action_bindings();
                auto* const subsystem{subsystem_.Get()};
                for (auto const scope : {ml::ioj::EShipControlScope::Fighter,
                                         ml::ioj::EShipControlScope::Skater,
                                         ml::ioj::EShipControlScope::Gunship}) {
                    checks.is_true(
                        !subsystem->HasMappingContext(ml::ioj::load_ship_control_context(scope)),
                        TEXT("Other flight modes remain inactive"));
                }
                active_profile_id_ = settings_->GetActiveKeyProfileId();
                controller_->ConsoleCommand(TEXT("Input.+key Four"), true);
            })
            .Until(
                [this] {
                    auto const* const subsystem{subsystem_.Get()};
                    return !checks.all_passed ||
                           (IsValid(subsystem) &&
                            subsystem->HasMappingContext(ml::ioj::load_ship_control_context(
                                ml::ioj::EShipControlScope::Gunship)));
                },
                timeout)
            .Then([this] {
                auto* const controller{controller_.Get()};
                auto* const ship{IsValid(controller) ? Cast<ATestSpaceShip>(controller->GetPawn())
                                                     : nullptr};
                auto* const subsystem{subsystem_.Get()};
                checks.is_true(IsValid(ship) && ship->get_active_flight_model_slot() ==
                                                    ::ioj::sim::player::FlightModelSlot::Left,
                               TEXT("Gunship key selects native Left slot"));
                checks.is_true(subsystem->HasMappingContext(ml::ioj::load_ship_control_context(
                                   ml::ioj::EShipControlScope::General)),
                               TEXT("General remains active"));
                for (auto const scope : {ml::ioj::EShipControlScope::Starfox,
                                         ml::ioj::EShipControlScope::Fighter,
                                         ml::ioj::EShipControlScope::Skater}) {
                    checks.is_true(
                        !subsystem->HasMappingContext(ml::ioj::load_ship_control_context(scope)),
                        TEXT("Other modes are inactive"));
                }
                checks.is_true(settings_->GetActiveKeyProfileId() == active_profile_id_,
                               TEXT("Flight-mode selection does not cycle key profiles"));
                check_active_contexts(ml::ioj::EShipControlScope::Gunship);
            });
    }

    TEST_METHOD(GunshipGamepadADescendsWithoutFiring)
    {
        TestCommandBuilder.Do([this] { setup(); })
            .Until([this] { return !checks.all_passed || ready(); }, timeout)
            .Then([this] {
                if (!checks.is_true(ready(), TEXT("Direct gameplay startup is ready"))) {
                    return;
                }
                controller_->ConsoleCommand(TEXT("Input.+key Four"), true);
            })
            .Until(
                [this] {
                    auto const* const ship{IsValid(controller_.Get())
                                               ? Cast<ATestSpaceShip>(controller_->GetPawn())
                                               : nullptr};
                    return !checks.all_passed ||
                           (IsValid(ship) && ship->get_active_flight_model_slot() ==
                                                 ::ioj::sim::player::FlightModelSlot::Left);
                },
                timeout)
            .Then([this] {
                controller_->ConsoleCommand(TEXT("Input.+key Gamepad_FaceButton_Bottom"), true);
            })
            .Until(
                [this] {
                    auto const* const sim{
                        level_setup.get_orchestrator()->get_player_ship_simulation()};
                    return !checks.all_passed ||
                           (sim && sim->get_flight_intent().translation.z < -0.5f);
                },
                timeout)
            .Then([this] {
                auto const* const sim{level_setup.get_orchestrator()->get_player_ship_simulation()};
                checks.is_true(sim && sim->get_flight_intent().translation.z < -0.5f,
                               TEXT("Gunship gamepad A sends negative vertical intent"));
                checks.is_true(sim && sim->laser_firing_mode == ::ioj::sim::LaserFiringState::idle,
                               TEXT("Gunship gamepad A does not fire"));
                check_active_contexts(ml::ioj::EShipControlScope::Gunship);
                check_single_ship_action_bindings();
            });
    }

    TEST_METHOD(ControllerRotationHasConsistentPhysicalSignsAcrossFlightModes)
    {
        using Axis = ml::ioj::ESpaceGameInputAxis;
        using Scope = ml::ioj::EShipControlScope;
        TestCommandBuilder.Do([this] { setup(); })
            .Until([this] { return !checks.all_passed || ready(); }, timeout)
            .Then([this] {
                auto* const settings{Cast<ml::ioj::USpaceGameInputUserSettings>(settings_.Get())};
                original_pitch_inversion_ = settings->invert_gamepad_pitch();
                original_yaw_inversion_ = settings->invert_gamepad_yaw();
                original_roll_inversion_ = settings->invert_gamepad_roll();
                original_vertical_inversion_ = settings->invert_gamepad_vertical_translation();
                restore_pitch_inversion_ = true;
                restore_other_inversions_ = true;
                prepare_slate_input();
            });

        struct Mode {
            Scope scope;
            FKey select;
            FKey pitch;
            FKey yaw;
            FKey roll;
        };
        Mode const modes[]{
            {Scope::Starfox, EKeys::One, EKeys::Gamepad_LeftY, EKeys::Gamepad_LeftX, {}},
            {Scope::Fighter,
             EKeys::Two,
             EKeys::Gamepad_RightY,
             EKeys::Gamepad_RightX,
             EKeys::Gamepad_LeftX},
            {Scope::Skater,
             EKeys::Three,
             EKeys::Gamepad_LeftY,
             EKeys::Gamepad_LeftX,
             EKeys::Gamepad_RightX},
            {Scope::Gunship, EKeys::Four, EKeys::Gamepad_RightY, EKeys::Gamepad_RightX, {}},
        };
        for (auto const mode : modes) {
            TestCommandBuilder.Then([this, mode] { send_slate_key(mode.select, true); })
                .Until(
                    [this, mode] {
                        auto const* const ship{Cast<ATestSpaceShip>(controller_->GetPawn())};
                        return !checks.all_passed || ship->get_active_flight_model_slot() ==
                                                         ml::ioj::flight_model_slot(mode.scope);
                    },
                    timeout)
                .Then([this, mode] {
                    send_slate_key(mode.select, false);
                    check_active_contexts(mode.scope);
                });
            struct AxisBinding {
                Axis axis;
                FKey key;
            };
            for (auto const binding : {AxisBinding{Axis::Pitch, mode.pitch},
                                       AxisBinding{Axis::Yaw, mode.yaw},
                                       AxisBinding{Axis::Roll, mode.roll}}) {
                if (!binding.key.IsValid()) {
                    continue;
                }
                for (bool const inverted : {false, true}) {
                    TestCommandBuilder.Then([this, binding, inverted] {
                        auto* const settings{
                            Cast<ml::ioj::USpaceGameInputUserSettings>(settings_.Get())};
                        settings->set_invert_gamepad_pitch(inverted && binding.axis == Axis::Pitch);
                        settings->set_invert_gamepad_yaw(inverted && binding.axis == Axis::Yaw);
                        settings->set_invert_gamepad_roll(inverted && binding.axis == Axis::Roll);
                        settings->ApplySettings();
                    });
                    for (float const physical_value : {1.0f, -1.0f}) {
                        auto const expected_sign{physical_value * (inverted ? -1.0f : 1.0f)};
                        TestCommandBuilder
                            .Then([this, binding, physical_value] {
                                // Slate stick-up/right is positive, before the viewport's RightY
                                // flip.
                                send_slate_axis(binding.key, physical_value);
                            })
                            .Until(
                                [this, binding, expected_sign] {
                                    return !checks.all_passed ||
                                           native_rotation(binding.axis) * expected_sign > 0.5f;
                                },
                                timeout)
                            .Then([this, mode, binding, inverted, expected_sign] {
                                checks.is_true(native_rotation(binding.axis) * expected_sign > 0.5f,
                                               FString::Printf(TEXT("Scope %d axis %d inverted=%d "
                                                                    "follows physical direction"),
                                                               static_cast<int32>(mode.scope),
                                                               static_cast<int32>(binding.axis),
                                                               inverted));
                                send_slate_axis(binding.key, 0.0f);
                            })
                            .Until(
                                [this, binding] {
                                    return !checks.all_passed ||
                                           FMath::IsNearlyZero(native_rotation(binding.axis));
                                },
                                timeout);
                    }
                }
            }
        }
    }

    TEST_METHOD(GunshipSlateADescendsWithoutCommonUIClick)
    {
        TestCommandBuilder.Do([this] { setup(); })
            .Until([this] { return !checks.all_passed || ready(); }, timeout)
            .Then([this] { controller_->ConsoleCommand(TEXT("Input.+key Four"), true); })
            .Until(
                [this] {
                    auto const* const ship{IsValid(controller_.Get())
                                               ? Cast<ATestSpaceShip>(controller_->GetPawn())
                                               : nullptr};
                    return !checks.all_passed ||
                           (IsValid(ship) && ship->get_active_flight_model_slot() ==
                                                 ::ioj::sim::player::FlightModelSlot::Left);
                },
                timeout)
            .Then([this] {
                controller_->ConsoleCommand(TEXT("Input.-key Four"), true);
                check_active_contexts(ml::ioj::EShipControlScope::Gunship);
                check_single_ship_action_bindings();
                if (prepare_slate_input()) {
                    send_slate_key(EKeys::Gamepad_FaceButton_Bottom, true);
                    checks.is_true(!FSlateApplication::Get().GetPressedMouseButtons().Contains(
                                       EKeys::LeftMouseButton),
                                   TEXT("Gameplay Accept does not synthesize mouse down"));
                }
            })
            .Until(
                [this] {
                    auto const* const sim{
                        level_setup.get_orchestrator()->get_player_ship_simulation()};
                    return !checks.all_passed ||
                           (sim != nullptr && sim->get_flight_intent().translation.z < -0.5f);
                },
                timeout)
            .Then([this] {
                auto const* const sim{level_setup.get_orchestrator()->get_player_ship_simulation()};
                checks.is_true(sim != nullptr && sim->get_flight_intent().translation.z < -0.5f,
                               TEXT("Slate and CommonUI route A to descent"));
                checks.is_true(sim != nullptr &&
                                   sim->laser_firing_mode == ::ioj::sim::LaserFiringState::idle,
                               TEXT("CommonUI does not turn gameplay A into mouse fire"));
                send_slate_key(EKeys::Gamepad_FaceButton_Bottom, false);
            })
            .Until([this] { return !checks.all_passed || FMath::IsNearlyZero(native_vertical()); },
                   timeout)
            .Then([this] {
                checks.is_true(FMath::IsNearlyZero(native_vertical()),
                               TEXT("Slate A release clears descent"));
                FPlayerControllerTestAccess::toggle_pause(*controller_);
            })
            .Until(
                [this] {
                    auto* const router{
                        controller_->GetLocalPlayer()->GetSubsystem<UCommonUIActionRouterBase>()};
                    return !checks.all_passed ||
                           (FPlayerControllerTestAccess::has_modal(*controller_) &&
                            router->GetActiveInputMode() == ECommonInputMode::Menu);
                },
                timeout)
            .Then([this] {
                auto* const router{
                    controller_->GetLocalPlayer()->GetSubsystem<UCommonUIActionRouterBase>()};
                auto const user{static_cast<uint32>(
                    controller_->GetLocalPlayer()->GetSlateUser()->GetUserIndex())};
                FKeyEvent const accept{
                    EKeys::Gamepad_FaceButton_Bottom, FModifierKeysState{}, user, false, 0, 0};
                checks.is_true(
                    router->GetCommonAnalogCursor()->ShouldVirtualAcceptSimulateMouseButton(
                        accept, IE_Pressed),
                    TEXT("Menu Accept retains CommonUI mouse-click behavior"));
                send_slate_key(EKeys::Gamepad_FaceButton_Bottom, true);
                checks.is_true(FSlateApplication::Get().GetPressedMouseButtons().Contains(
                                   EKeys::LeftMouseButton),
                               TEXT("Menu Accept still synthesizes mouse down"));
                send_slate_key(EKeys::Gamepad_FaceButton_Bottom, false);
                checks.is_true(!FSlateApplication::Get().GetPressedMouseButtons().Contains(
                                   EKeys::LeftMouseButton),
                               TEXT("Menu Accept releases the synthetic mouse button"));
                // NullRHI has no rendered hit-test grid for clicking Resume.
                FPlayerControllerTestAccess::toggle_pause(*controller_);
            })
            .Until(
                [this] {
                    return !checks.all_passed ||
                           (!FPlayerControllerTestAccess::has_modal(*controller_) &&
                            controller_->get_active_control_context() ==
                                EPlayerControlContext::Player);
                },
                timeout)
            .Then([this] {
                checks.is_true(!FPlayerControllerTestAccess::has_modal(*controller_),
                               TEXT("Pause returns to gameplay"));
                check_active_contexts(ml::ioj::EShipControlScope::Gunship);
                send_slate_key(EKeys::Gamepad_FaceButton_Bottom, true);
            })
            .Until([this] { return !checks.all_passed || native_vertical() < -0.5f; }, timeout)
            .Then([this] {
                checks.is_true(native_vertical() < -0.5f,
                               TEXT("Gameplay A descends again after Resume"));
                checks.is_true(level_setup.get_orchestrator()
                                       ->get_player_ship_simulation()
                                       ->laser_firing_mode == ::ioj::sim::LaserFiringState::idle,
                               TEXT("Resume does not leave synthetic mouse fire held"));
            });
    }

    TEST_METHOD(ControllerYawRollAndVerticalInversionAreSemantic)
    {
        TestCommandBuilder.Do([this] { setup(); })
            .Until([this] { return !checks.all_passed || ready(); }, timeout)
            .Then([this] {
                auto* const settings{Cast<ml::ioj::USpaceGameInputUserSettings>(settings_.Get())};
                if (!checks.is_true(IsValid(settings), TEXT("Canonical input settings exist"))) {
                    return;
                }
                original_yaw_inversion_ = settings->invert_gamepad_yaw();
                original_roll_inversion_ = settings->invert_gamepad_roll();
                original_vertical_inversion_ = settings->invert_gamepad_vertical_translation();
                restore_other_inversions_ = true;
                settings->set_invert_gamepad_yaw(true);
                settings->set_invert_gamepad_roll(true);
                settings->set_invert_gamepad_vertical_translation(true);
                settings->ApplySettings();
                controller_->ConsoleCommand(TEXT("Input.+key Two"), true);
            })
            .Until(
                [this] {
                    auto const* const ship{IsValid(controller_.Get())
                                               ? Cast<ATestSpaceShip>(controller_->GetPawn())
                                               : nullptr};
                    return !checks.all_passed ||
                           (IsValid(ship) && ship->get_active_flight_model_slot() ==
                                                 ::ioj::sim::player::FlightModelSlot::Right);
                },
                timeout)
            .Then([this] {
                controller_->ConsoleCommand(TEXT("Input.-key Two"), true);
                controller_->ConsoleCommand(TEXT("Input.+key Gamepad_RightX 1.0"), true);
                controller_->ConsoleCommand(TEXT("Input.+key Gamepad_LeftX 1.0"), true);
            })
            .Until(
                [this] {
                    return !checks.all_passed || (native_yaw() < -0.5f && native_roll() < -0.5f);
                },
                timeout)
            .Then([this] {
                checks.is_true(native_yaw() < -0.5f,
                               TEXT("Controller Yaw inversion reverses Fighter yaw"));
                checks.is_true(native_roll() < -0.5f,
                               TEXT("Controller Roll inversion reverses Fighter roll"));
                controller_->ConsoleCommand(TEXT("Input.-key Gamepad_RightX"), true);
                controller_->ConsoleCommand(TEXT("Input.-key Gamepad_LeftX"), true);
                controller_->ConsoleCommand(TEXT("Input.+key Four"), true);
            })
            .Until(
                [this] {
                    auto const* const ship{IsValid(controller_.Get())
                                               ? Cast<ATestSpaceShip>(controller_->GetPawn())
                                               : nullptr};
                    return !checks.all_passed ||
                           (IsValid(ship) && ship->get_active_flight_model_slot() ==
                                                 ::ioj::sim::player::FlightModelSlot::Left);
                },
                timeout)
            .Then([this] {
                controller_->ConsoleCommand(TEXT("Input.-key Four"), true);
                controller_->ConsoleCommand(TEXT("Input.+key Gamepad_FaceButton_Bottom"), true);
            })
            .Until([this] { return !checks.all_passed || native_vertical() > 0.5f; }, timeout)
            .Then([this] {
                checks.is_true(native_vertical() > 0.5f,
                               TEXT("Controller vertical inversion follows authored A Negate"));
                controller_->ConsoleCommand(TEXT("Input.-key Gamepad_FaceButton_Bottom"), true);
                controller_->ConsoleCommand(TEXT("Input.+key C"), true);
            })
            .Until([this] { return !checks.all_passed || native_vertical() < -0.5f; }, timeout)
            .Then([this] {
                checks.is_true(native_vertical() < -0.5f,
                               TEXT("Controller vertical inversion does not affect keyboard C"));
            });
    }

    TEST_METHOD(ControlsQueriesStayWithinDeviceAndFlightScope)
    {
        TestCommandBuilder.Do([this] { setup(); })
            .Until([this] { return !checks.all_passed || ready(); }, timeout)
            .Then([this] {
                if (!checks.is_true(ready(), TEXT("Gameplay input is ready"))) {
                    return;
                }
                auto* const controller{controller_.Get()};
                auto* const game_instance{controller->GetGameInstance()};
                auto* const settings{
                    IsValid(game_instance)
                        ? game_instance->GetSubsystem<ml::ioj::UGameSettingsSubsystem>()
                        : nullptr};
                if (!checks.is_true(IsValid(settings), TEXT("Game settings exist"))) {
                    return;
                }
                settings->begin_edit(controller->GetLocalPlayer());
                using ml::ioj::EShipControlScope;
                auto const starfox{settings->control_bindings(EHardwareDevicePrimaryType::Gamepad,
                                                              EShipControlScope::Starfox)};
                auto const gunship{settings->control_bindings(EHardwareDevicePrimaryType::Gamepad,
                                                              EShipControlScope::Gunship)};
                auto const general{settings->control_bindings(
                    EHardwareDevicePrimaryType::KeyboardAndMouse, EShipControlScope::General)};
                checks.is_true(!starfox.IsEmpty() && !gunship.IsEmpty() && !general.IsEmpty(),
                               TEXT("Each canonical scope has its own remappable bindings"));
                for (auto const& binding : starfox) {
                    checks.is_true(binding.scope == EShipControlScope::Starfox &&
                                       binding.device_type == EHardwareDevicePrimaryType::Gamepad,
                                   TEXT("Starfox query has only controller Starfox rows"));
                }
                for (auto const& binding : gunship) {
                    checks.is_true(binding.scope == EShipControlScope::Gunship &&
                                       binding.device_type == EHardwareDevicePrimaryType::Gamepad,
                                   TEXT("Gunship query has only controller Gunship rows"));
                }
                checks.is_true(starfox.ContainsByPredicate([](auto const& binding) {
                    return binding.address.mapping_name == FName{TEXT("Starfox.Pitch.Gamepad")};
                }) && gunship.ContainsByPredicate([](auto const& binding) {
                    return binding.address.mapping_name == FName{TEXT("Gunship.Pitch.Gamepad")};
                }),
                               TEXT("Same semantic action has separate stable mode identities"));
                checks.is_true(gunship.ContainsByPredicate([](auto const& binding) {
                    return binding.address.mapping_name ==
                           FName{TEXT("Gunship.FirePrimary.Gamepad")};
                }),
                               TEXT("Gunship fire mapping is exposed to controls settings"));

                auto* const unrelated{NewObject<UInputMappingContext>(GetTransientPackage())};
                auto* const unrelated_action{NewObject<UInputAction>(unrelated)};
                auto& mapping{unrelated->MapKey(unrelated_action, EKeys::Gamepad_FaceButton_Left)};
                auto* const behavior_property{FindFProperty<FEnumProperty>(
                    FEnhancedActionKeyMapping::StaticStruct(), TEXT("SettingBehavior"))};
                auto* const settings_property{FindFProperty<FObjectProperty>(
                    FEnhancedActionKeyMapping::StaticStruct(), TEXT("PlayerMappableKeySettings"))};
                if (!checks.is_true(behavior_property != nullptr && settings_property != nullptr,
                                    TEXT("Player-mappable mapping properties exist"))) {
                    settings->cancel();
                    return;
                }
                behavior_property->GetUnderlyingProperty()->SetIntPropertyValue(
                    behavior_property->ContainerPtrToValuePtr<void>(&mapping),
                    static_cast<int64>(EPlayerMappableKeySettingBehaviors::OverrideSettings));
                auto* const key_settings{NewObject<UPlayerMappableKeySettings>(unrelated)};
                key_settings->Name = FName{TEXT("Unrelated.Test.Gamepad")};
                settings_property->SetObjectPropertyValue_InContainer(&mapping, key_settings);
                checks.is_true(settings_->RegisterInputMappingContext(unrelated),
                               TEXT("Unrelated player-mappable context registers"));
                auto const* const profile{settings_->GetDefaultKeyProfile()};
                checks.is_true(IsValid(profile) && profile->GetPlayerMappingRows().Contains(
                                                       FName{TEXT("Unrelated.Test.Gamepad")}),
                               TEXT("Unrelated mapping really entered the key profile"));
                checks.is_true(!settings
                                    ->control_bindings(EHardwareDevicePrimaryType::Gamepad,
                                                       EShipControlScope::Gunship)
                                    .ContainsByPredicate([](auto const& binding) {
                                        return binding.address.mapping_name ==
                                               FName{TEXT("Unrelated.Test.Gamepad")};
                                    }),
                               TEXT("Canonical scope query excludes unrelated registered IMC"));
                settings_->UnregisterInputMappingContext(unrelated);
                settings->cancel();
            });
    }

    TEST_METHOD(CanonicalRemapPersistsAcrossSettingsReload)
    {
        TestCommandBuilder.Do([this] { setup(); })
            .Until([this] { return !checks.all_passed || ready(); }, timeout)
            .Then([this] {
                if (!checks.is_true(ready(), TEXT("Gameplay input is ready"))) {
                    return;
                }
                auto* const controller{controller_.Get()};
                auto* const game_instance{controller->GetGameInstance()};
                auto* const settings{
                    IsValid(game_instance)
                        ? game_instance->GetSubsystem<ml::ioj::UGameSettingsSubsystem>()
                        : nullptr};
                if (!checks.is_true(IsValid(settings), TEXT("Game settings exist"))) {
                    return;
                }
                settings->begin_edit(controller->GetLocalPlayer());
                auto const bindings{settings->control_bindings(
                    EHardwareDevicePrimaryType::Gamepad, ml::ioj::EShipControlScope::Starfox)};
                auto const* const binding{bindings.FindByPredicate([](auto const& candidate) {
                    return candidate.address.mapping_name == FName{TEXT("Starfox.Pitch.Gamepad")};
                })};
                if (!checks.is_true(binding != nullptr, TEXT("Canonical pitch mapping exists"))) {
                    return;
                }

                remap_address_ = binding->address;
                original_remap_key_ = binding->current_key;
                original_default_key_ = binding->default_key;
                test_user_index_ = controller->GetLocalPlayer()->GetLocalPlayerIndex();
                auto* const developer_settings{
                    GetMutableDefault<UEnhancedInputDeveloperSettings>()};
                original_save_slot_ = developer_settings->InputSettingsSaveSlotName;
                test_save_slot_ = FString::Printf(TEXT("CanonicalInputTest_%s"),
                                                  *FGuid::NewGuid().ToString(EGuidFormats::Digits));
                developer_settings->InputSettingsSaveSlotName = test_save_slot_;
                restore_remap_ = true;
                auto* const input_settings{
                    Cast<ml::ioj::USpaceGameInputUserSettings>(settings_.Get())};
                if (!checks.is_true(IsValid(input_settings),
                                    TEXT("Canonical input settings exist"))) {
                    return;
                }
                original_pitch_inversion_ = input_settings->invert_gamepad_pitch();
                restore_pitch_inversion_ = true;

                original_yaw_inversion_ = input_settings->invert_gamepad_yaw();
                original_roll_inversion_ = input_settings->invert_gamepad_roll();
                original_vertical_inversion_ =
                    input_settings->invert_gamepad_vertical_translation();
                restore_other_inversions_ = true;

                checks.is_true(
                    settings->set_control_binding(remap_address_, EKeys::Gamepad_RightY, false),
                    TEXT("Canonical remap previews in the edit transaction"));
                settings->set_setting(ml::ioj::EGameSetting::InvertGamepadPitch,
                                      ml::ioj::FGameSettingValue{!original_pitch_inversion_});
                settings->set_setting(ml::ioj::EGameSetting::InvertGamepadYaw,
                                      ml::ioj::FGameSettingValue{!original_yaw_inversion_});
                settings->set_setting(ml::ioj::EGameSetting::InvertGamepadRoll,
                                      ml::ioj::FGameSettingValue{!original_roll_inversion_});
                settings->apply();
                checks.is_true(settings
                                   ->control_bindings(EHardwareDevicePrimaryType::Gamepad,
                                                      ml::ioj::EShipControlScope::Starfox)
                                   .ContainsByPredicate([](auto const& current) {
                                       return current.address.mapping_name ==
                                                  FName{TEXT("Starfox.Pitch.Gamepad")} &&
                                              current.current_key == EKeys::Gamepad_RightY;
                                   }),
                               TEXT("Apply retains the remap in the active key profile"));
            })
            .Until(
                [this] {
                    return !checks.all_passed || (canonical_remap_reloaded(EKeys::Gamepad_RightY) &&
                                                  inversion_reloaded());
                },
                FTimespan::FromSeconds(5))
            .Then([this] {
                if (prepare_slate_input()) {
                    send_slate_axis(EKeys::Gamepad_RightY, 1.0f);
                }
            })
            .Until(
                [this] {
                    return !checks.all_passed ||
                           native_pitch() * (original_pitch_inversion_ ? 1.0f : -1.0f) > 0.5f;
                },
                timeout)
            .Then([this] {
                checks.is_true(native_pitch() * (original_pitch_inversion_ ? 1.0f : -1.0f) > 0.5f,
                               TEXT("Remapped right-stick Pitch retains its semantic inversion"));
                send_slate_axis(EKeys::Gamepad_RightY, 0.0f);
            })
            .Then([this] {
                if (!checks.is_true(canonical_remap_reloaded(EKeys::Gamepad_RightY),
                                    TEXT("Saved semantic gamepad row survives a fresh reload"))) {
                    return;
                }
                checks.is_true(inversion_reloaded(),
                               TEXT("Apply persists controller Pitch, Yaw, and Roll inversion"));
                auto* const controller{controller_.Get()};
                auto* const settings{
                    controller->GetGameInstance()->GetSubsystem<ml::ioj::UGameSettingsSubsystem>()};
                settings->begin_edit(controller->GetLocalPlayer());
                settings->set_setting(ml::ioj::EGameSetting::InvertGamepadPitch,
                                      ml::ioj::FGameSettingValue{original_pitch_inversion_});
                settings->set_setting(ml::ioj::EGameSetting::InvertGamepadYaw,
                                      ml::ioj::FGameSettingValue{original_yaw_inversion_});
                settings->set_setting(ml::ioj::EGameSetting::InvertGamepadRoll,
                                      ml::ioj::FGameSettingValue{original_roll_inversion_});
                checks.is_true(settings->set_control_binding(
                                   remap_address_, EKeys::Gamepad_FaceButton_Right, false),
                               TEXT("A second remap can be previewed"));
                settings->cancel();
                auto const* const input_settings{
                    Cast<ml::ioj::USpaceGameInputUserSettings>(settings_.Get())};
                checks.is_true(
                    input_settings->invert_gamepad_pitch() == !original_pitch_inversion_ &&
                        input_settings->invert_gamepad_yaw() == !original_yaw_inversion_ &&
                        input_settings->invert_gamepad_roll() == !original_roll_inversion_ &&
                        inversion_reloaded(),
                    TEXT("Cancel restores live rotation inversion without changing its save"));
                checks.is_true(canonical_remap_reloaded(EKeys::Gamepad_RightY),
                               TEXT("Cancel does not persist the previewed remap"));
                checks.is_true(settings
                                   ->control_bindings(EHardwareDevicePrimaryType::Gamepad,
                                                      ml::ioj::EShipControlScope::Starfox)
                                   .ContainsByPredicate([](auto const& current) {
                                       return current.address.mapping_name ==
                                                  FName{TEXT("Starfox.Pitch.Gamepad")} &&
                                              current.current_key == EKeys::Gamepad_RightY;
                                   }),
                               TEXT("Cancel restores the previously applied live mapping"));
                settings->begin_edit(controller->GetLocalPlayer());
                checks.is_true(settings->reset_control_binding(remap_address_),
                               TEXT("Reset previews the canonical default"));
                settings->apply();
            })
            .Until(
                [this] {
                    return !checks.all_passed || canonical_remap_reloaded(original_default_key_);
                },
                FTimespan::FromSeconds(5))
            .Then([this] {
                checks.is_true(canonical_remap_reloaded(original_default_key_),
                               TEXT("Reset and Apply persist the canonical default"));
                send_slate_axis(EKeys::Gamepad_LeftY, 1.0f);
            })
            .Until(
                [this] {
                    return !checks.all_passed ||
                           native_pitch() * (original_pitch_inversion_ ? 1.0f : -1.0f) > 0.5f;
                },
                timeout)
            .Then([this] {
                checks.is_true(native_pitch() * (original_pitch_inversion_ ? 1.0f : -1.0f) > 0.5f,
                               TEXT("Default left-stick Pitch matches the remapped right stick"));
            });
    }

    TEST_METHOD(FlightTuningCannotChangeModeTopologyOrActiveSlot)
    {
        TestCommandBuilder.Do([this] { setup(); })
            .Until([this] { return !checks.all_passed || ready(); }, timeout)
            .Then([this] {
                if (!checks.is_true(ready(), TEXT("Gameplay input is ready"))) {
                    return;
                }
                auto* const controller{controller_.Get()};
                auto* const ship{Cast<ATestSpaceShip>(controller->GetPawn())};
                auto* const game_instance{controller->GetGameInstance()};
                auto* const settings{
                    IsValid(game_instance)
                        ? game_instance->GetSubsystem<ml::ioj::UGameSettingsSubsystem>()
                        : nullptr};
                if (!checks.is_true(IsValid(settings), TEXT("Game settings exist")) ||
                    !checks.is_valid(ship, TEXT("Player ship exists"))) {
                    return;
                }
                settings->begin_edit(controller->GetLocalPlayer());
                auto const before{settings->flight_model_loadout()};
                auto const active_slot{ship->get_active_flight_model_slot()};
                using ml::ioj::EShipControlScope;
                using namespace ::ioj::sim::player;
                auto tuned{settings->flight_model_profile(EShipControlScope::Gunship)};
                tuned.config.translation.up.normal.positive_target_speed += 100.f;
                tuned.config.translation.up.manual.response.mode = ResponseMode::SecondOrder;
                checks.is_true(
                    settings->set_flight_model_profile(EShipControlScope::Gunship, tuned),
                    TEXT("Numeric and response tuning are accepted"));
                tuned.config.boost.available = !tuned.config.boost.available;
                tuned.config.boost.accelerator_activates_boost =
                    !tuned.config.boost.accelerator_activates_boost;
                tuned.config.brake.available = !tuned.config.brake.available;
                tuned.config.emergency_brake.available = !tuned.config.emergency_brake.available;
                tuned.config.rotation.roll.stabilization.enabled =
                    !tuned.config.rotation.roll.stabilization.enabled;
                checks.is_true(
                    settings->set_flight_model_profile(EShipControlScope::Gunship, tuned),
                    TEXT("Behavioral boost, brake, and stabilization tuning is accepted"));
                checks.is_true(settings->flight_model_loadout().up == before.up &&
                                   settings->flight_model_loadout().right == before.right &&
                                   settings->flight_model_loadout().down == before.down,
                               TEXT("Editing Gunship leaves other slots unchanged"));

                auto changed{tuned};
                changed.config.translation.up.manual.semantic = TranslationSemantic::Disabled;
                checks.is_true(
                    !settings->set_flight_model_profile(EShipControlScope::Gunship, changed),
                    TEXT("Disabling an authored channel is rejected"));
                changed = tuned;
                changed.config.translation.up.manual.input_source =
                    TranslationInputSource::Accelerator;
                checks.is_true(
                    !settings->set_flight_model_profile(EShipControlScope::Gunship, changed),
                    TEXT("Changing input source is rejected"));
                changed = tuned;
                changed.config.translation.up.manual.reference_frame = ReferenceFrame::World;
                checks.is_true(
                    !settings->set_flight_model_profile(EShipControlScope::Gunship, changed),
                    TEXT("Changing authored reference frame is rejected"));
                changed = settings->flight_model_profile(EShipControlScope::Starfox);
                changed.config.translation.up.manual.semantic = TranslationSemantic::TargetVelocity;
                checks.is_true(
                    !settings->set_flight_model_profile(EShipControlScope::Starfox, changed),
                    TEXT("Enabling an absent channel is rejected"));
                checks.is_true(ship->get_active_flight_model_slot() == active_slot,
                               TEXT("Editing a slot never selects it in gameplay"));
                settings->cancel();
                checks.is_true(settings->flight_model_loadout() == before &&
                                   ship->get_active_flight_model_slot() == active_slot,
                               TEXT("Cancel restores tuning without changing the runtime slot"));
            });
    }
};
