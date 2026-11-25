#include <ioj/levels/level_definition.h>
#include <ioj/sim/player/sim.h>
#include <SandboxTests/support/PlayerControllerTestAccess.h>
#include <SandboxTests/support/test_setup.h>
#include <SpaceGame/input/CanonicalShipControls.h>
#include <SpaceGame/input/SpaceGameInputModifier.h>
#include <SpaceGame/input/SpaceGameInputUserSettings.h>
#include <SpaceGame/settings/OptionsDraft.h>
#include <SpaceGame/ships/player/SpaceGamePlayerController.h>
#include <SpaceGame/ships/player/TestSpaceShip.h>
#include <SpaceGame/simulation/TestBatchOrchestrator.h>
#include <SpaceGame/system/GameSubsystem.h>
#include <SpaceGame/ui/common/InputActionRouter.h>
#include <SpaceGamePresentation/presentation/widgets/ShipHudWidget.h>

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
#include <Input/HittestGrid.h>
#include <InputAction.h>
#include <InputKeyEventArgs.h>
#include <InputMappingContext.h>
#include <Kismet/GameplayStatics.h>
#include <Misc/Guid.h>
#include <PlayerMappableKeySettings.h>
#include <Rendering/DrawElements.h>
#include <Slate/SceneViewport.h>
#include <Slate/SObjectWidget.h>
#include <Types/PaintArgs.h>
#include <UObject/UnrealType.h>
#include <UserSettings/EnhancedInputUserSettings.h>
#include <Widgets/SViewport.h>
#include <Widgets/SWindow.h>

TEST_CLASS(PlayerInputSmoke, "Sandbox.LevelTests")
{
    inline static ioj::FTestBatchOrchestratorLevelSetup level_setup{};
    inline static FTimespan const timeout{FTimespan::FromSeconds(2)};

    ioj::FSoftTestAssertions checks{};
    TWeakObjectPtr<ioj::ASpaceGamePlayerController> controller_{};
    TWeakObjectPtr<UEnhancedInputLocalPlayerSubsystem> subsystem_{};
    TWeakObjectPtr<UEnhancedInputUserSettings> settings_{};
    FString active_profile_id_{};
    FString original_save_slot_{};
    FString test_save_slot_{};
    ioj::FControlBindingAddress remap_address_{};
    FKey original_remap_key_{};
    FKey original_default_key_{};
    int32 test_user_index_{};
    bool restore_remap_{};
    bool original_pitch_inversion_{};
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
        used_slate_input_ = false;
    }

    AFTER_ALL()
    { level_setup.teardown(); }

    auto setup() -> bool {
        auto* const orchestrator{level_setup.get_orchestrator()};
        if (!checks.is_valid(orchestrator, TEXT("Orchestrator exists"))) {
            return false;
        }
        auto definition{ioj::levels::LevelDefinition::make({
            .metadata = {.id = ioj::levels::LevelId{"input-smoke"}, .title = "Input Smoke"},
            .player_entity_id = ioj::levels::EntityId{"player"},
            .entities = {{.id = ioj::levels::EntityId{"player"},
                          .archetype = ioj::levels::EntityArchetype::PlayerFighter,
                          .team = ioj::Team::Blue}},
        })};
        if (!checks.is_true(static_cast<bool>(orchestrator->load_level(definition.value())),
                            TEXT("Gameplay level loads directly"))) {
            return false;
        }
        auto* const controller{Cast<ioj::ASpaceGamePlayerController>(
            level_setup.get_world().GetFirstPlayerController())};
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
        for (auto const& control : ioj::canonical_ship_control_contexts()) {
            auto* const context{ioj::load_ship_control_context(control.scope)};
            checks.is_true(IsValid(context) && settings->IsMappingContextRegistered(context),
                           TEXT("Canonical context registered without opening Options"));
        }
        orchestrator->start_simulation();
        return checks.all_passed;
    }

    auto ready() const -> bool {
        auto const* const controller{controller_.Get()};
        auto const* const subsystem{subsystem_.Get()};
        auto const* const ship{
            IsValid(controller) ? Cast<ioj::ATestSpaceShip>(controller->GetPawn()) : nullptr};
        return IsValid(subsystem) && IsValid(ship) && ship->has_simulation() &&
               controller->get_active_control_context() == ::ioj::levels::ControlContext::Player &&
               subsystem->HasMappingContext(
                   ioj::load_ship_control_context(ioj::EShipControlScope::General)) &&
               subsystem->HasMappingContext(
                   ioj::load_ship_control_context(ioj::EShipControlScope::Starfox));
    }

    void check_active_contexts(ioj::EShipControlScope const mode) {
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
            checks.is_true(
                context == ioj::load_ship_control_context(ioj::EShipControlScope::General) ||
                    context == ioj::load_ship_control_context(mode),
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
        for (auto const& [name, expected] :
             {std::pair{TEXT("IA_Ship_Pitch"), 3}, std::pair{TEXT("IA_Ship_FirePrimary"), 3}}) {
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

    auto native_player() const -> ioj::sim::player::Sim const* {
        auto const* const orchestrator{level_setup.get_orchestrator()};
        auto const* const level{IsValid(orchestrator) ? orchestrator->get_level_simulation()
                                                      : nullptr};
        return level ? level->player_ship_simulation() : nullptr;
    }

    auto native_pitch() const -> float {
        auto const* const sim{native_player()};
        return sim != nullptr ? sim->flight_intent().pitch : 0.0f;
    }

    auto prepare_slate_input() -> bool {
        auto* const local_player{controller_->GetLocalPlayer()};
        auto* const router{local_player->GetSubsystem<UCommonUIActionRouterBase>()};
        auto* const accept_simulation{IConsoleManager::Get().FindConsoleVariable(
            TEXT("CommonUI.ShouldVirtualAcceptSimulateMouseButton"))};
        if (!checks.is_true(IsValid(router) && router->IsA<ioj::UInputActionRouter>(),
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
        for (auto const& definition : ioj::canonical_ship_control_contexts()) {
            loaded->RegisterInputMappingContext(ioj::load_ship_control_context(definition.scope));
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

    auto pitch_mapping_uses(ioj::UOptionsDraft const& settings, FKey const key) const -> bool {
        return settings.control_bindings(EHardwareDevicePrimaryType::Gamepad)
            .ContainsByPredicate([key](auto const& binding) {
                return binding.address.mapping_name == FName{TEXT("Starfox.Pitch.Gamepad")} &&
                       binding.current_key == key;
            });
    }

    TEST_METHOD(DirectGameplayStartup)
    {
        TestCommandBuilder.Do([this] { setup(); })
            .Until([this] { return !checks.all_passed || ready(); }, timeout)
            .Then([this] {
                if (!checks.is_true(ready(), TEXT("Starfox context activates after possession"))) {
                    return;
                }
                check_active_contexts(ioj::EShipControlScope::Starfox);
                check_single_ship_action_bindings();
            });
    }

    void check_mouse_overlay(FVector2f const displacement) {
        auto const* hud{Cast<ioj::UShipHudWidget>(controller_->get_active_hud())};
        if (!TestRunner->TestNotNull(TEXT("Ship HUD receives the gesture"), hud)) {
            return;
        }
        auto const widget{StaticCastSharedPtr<SObjectWidget>(hud->GetCachedWidget())};
        auto const window{FSlateApplication::Get().FindWidgetWindow(widget.ToSharedRef())};
        FSlateWindowElementList elements{window};
        FHittestGrid grid;
        FPaintArgs const args{nullptr, grid, FVector2f::ZeroVector, 0., 0.f};
        auto const geometry{FGeometry::MakeRoot(FVector2f{800.f, 600.f}, FSlateLayoutTransform{})};
        auto const layer{widget->OnPaint(
            args, geometry, FSlateRect{0.f, 0.f, 800.f, 600.f}, elements, 0, FWidgetStyle{}, true)};
        auto const& circles{elements.GetUncachedDrawElements()
                                .Get<static_cast<uint8>(EElementType::ET_RoundedBox)>()};
        if (!TestRunner->TestTrue(TEXT("HUD paints both steering indicators"),
                                  circles.Num() >= 2)) {
            return;
        }
        auto const& ring{circles.Last(1)};
        auto const& pointer{circles.Last()};
        FVector2f const size{ring.GetLocalSize()};
        FVector2f const centre{FVector2f{ring.GetPosition()} + size * 0.5f};
        FVector2f const tip{FVector2f{pointer.GetPosition()} +
                            FVector2f{pointer.GetLocalSize()} * 0.5f};
        TestRunner->TestTrue(TEXT("Ring is centred and pointer follows the physical drag"),
                             pointer.GetLayer() == layer &&
                                 centre.Equals(FVector2f{400.f, 300.f}) && size.X == size.Y &&
                                 tip.Equals(centre + displacement * (size.X * 0.5f), 0.01f));
    }

    TEST_METHOD(MouseSteeringUsesFixedPressOrigin)
    {
        TestCommandBuilder.Do([this] { setup(); })
            .Until([this] { return !checks.all_passed || ready(); }, timeout)
            .Then([this] {
                if (!checks.all_passed) {
                    return;
                }
                auto* const controller{controller_.Get()};
                auto* const settings{NewObject<ioj::UOptionsDraft>(controller)};
                settings->begin_edit(*ioj::UGameSubsystem::get(controller->GetGameInstance()),
                                     controller->GetLocalPlayer(),
                                     ioj::GameSettingCategory::Controls);
                settings->set_mouse_turn_sensitivity(2.f);
                settings->set_invert_mouse_yaw(true);
                settings->set_invert_mouse_pitch(false);
                settings->set_invert_gamepad_pitch(false);
                settings->set_gamepad_turn_sensitivity(1.f);
                auto* game{ioj::UGameSubsystem::get(controller->GetGameInstance())};
                if (!checks.is_true(game->apply_settings(ioj::GameSettingCategory::Controls,
                                                         settings->settings_state())
                                        .has_value(),
                                    TEXT("Controls applied"))) {
                    return;
                }

                auto const key{[controller](FKey k, EInputEvent event, float amount) {
                    controller->InputKey(FInputKeyEventArgs::CreateSimulated(k, event, amount, 1));
                }};
                auto const frame{[controller] {
                    TArray<UInputComponent*> const stack{controller->InputComponent.Get()};
                    controller->PlayerInput->ProcessInputStack(stack, 1.f / 60.f, false);
                }};
                auto const turn{[this](float yaw, float pitch, TCHAR const* description) {
                    auto const& intent{native_player()->flight_intent()};
                    TestRunner->TestEqual(
                        FString{description} + TEXT(" yaw"), intent.yaw, yaw, 0.001f);
                    TestRunner->TestEqual(
                        FString{description} + TEXT(" pitch"), intent.pitch, pitch, 0.001f);
                }};
                key(EKeys::MouseX, IE_Axis, 100.f);
                frame();
                turn(0.f, 0.f, TEXT("Mouse is idle before press"));
                key(EKeys::RightMouseButton, IE_Pressed, 1.f);
                frame();
                key(EKeys::MouseX, IE_Axis, 400.f);
                frame();
                turn(-1.f, 0.f, TEXT("Two radii gives full inverted yaw"));
                check_mouse_overlay(FVector2f{2.f, 0.f});
                key(EKeys::MouseX, IE_Axis, -100.f);
                frame();
                turn(-1.f, 0.f, TEXT("Still outside after reversal"));
                key(EKeys::MouseY, IE_Axis, -300.f);
                frame();
                turn(-UE_INV_SQRT_2,
                     UE_INV_SQRT_2,
                     TEXT("Direction follows the pointer beyond the rim"));
                key(EKeys::MouseX, IE_Axis, -200.f);
                key(EKeys::MouseY, IE_Axis, 300.f);
                frame();
                turn(-0.5f, 0.f, TEXT("Inside the rim at half radius"));
                key(EKeys::Gamepad_LeftY, IE_Axis, 1.f);
                frame();
                turn(-0.5f, 0.f, TEXT("Stationary mouse retains control"));
                key(EKeys::RightMouseButton, IE_Released, 0.f);
                frame();
                turn(0.f, 1.f, TEXT("Release restores the held gamepad axis"));
                key(EKeys::RightMouseButton, IE_Pressed, 1.f);
                frame();
                turn(0.f, 0.f, TEXT("Next press establishes a fresh origin"));
                check_mouse_overlay(FVector2f::ZeroVector);
                key(EKeys::MouseX, IE_Axis, 100.f);
                frame();
                controller->FlushPressedKeys();
                frame();
                key(EKeys::MouseX, IE_Axis, 100.f);
                frame();
                TestRunner->TestEqual(TEXT("Viewport focus flush releases mouse steering"),
                                      native_player()->flight_intent().yaw,
                                      0.f);
                key(EKeys::RightMouseButton, IE_Released, 0.f);
                frame();
                auto const bindings{
                    settings->control_bindings(EHardwareDevicePrimaryType::KeyboardAndMouse)};
                auto const* binding{bindings.FindByPredicate([](auto const& row) {
                    return row.address.mapping_name == TEXT("Starfox.Steer.KeyboardMouse");
                })};
                if (TestRunner->TestNotNull(TEXT("Steering is remappable"), binding) &&
                    TestRunner->TestTrue(
                        TEXT("Rebind the hold button"),
                        settings->set_control_binding(binding->address, EKeys::J, false))) {
                    auto* input{settings_.Get()};
                    FMapPlayerKeyArgs args{};
                    args.MappingName = binding->address.mapping_name;
                    args.ProfileIdString = binding->address.profile_id;
                    args.Slot = binding->address.slot;
                    args.HardwareDeviceId = binding->address.hardware_device_id;
                    args.NewKey = EKeys::J;
                    FGameplayTagContainer failure;
                    input->MapPlayerKey(args, failure);
                    FModifyContextOptions options;
                    options.bForceImmediately = true;
                    subsystem_->RequestRebuildControlMappings(options);
                    key(EKeys::J, IE_Pressed, 1.f);
                    key(EKeys::MouseX, IE_Axis, 100.f);
                    frame();
                    turn(-0.5f, 0.f, TEXT("Remapped button engages steering"));
                    FPlayerControllerTestAccess::toggle_pause(*controller);
                    turn(0.f, 0.f, TEXT("Opening pause clears the held turn"));
                    key(EKeys::J, IE_Released, 0.f);
                    args.NewKey = binding->current_key;
                    input->MapPlayerKey(args, failure);
                }
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
                auto* const settings{IsValid(game_instance)
                                         ? NewObject<ioj::UOptionsDraft>(game_instance)
                                         : nullptr};
                if (!checks.is_true(IsValid(settings), TEXT("Game settings exist"))) {
                    return;
                }
                settings->begin_edit(*ioj::UGameSubsystem::get(controller->GetGameInstance()),
                                     controller->GetLocalPlayer(),
                                     ioj::GameSettingCategory::Controls);
                auto const bindings{
                    settings->control_bindings(EHardwareDevicePrimaryType::Gamepad)};
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
                original_pitch_inversion_ = settings->settings_state().invert_gamepad_pitch;

                checks.is_true(
                    settings->set_control_binding(remap_address_, EKeys::Gamepad_RightY, false),
                    TEXT("Canonical remap previews in the edit transaction"));
                settings->apply();
                checks.is_true(pitch_mapping_uses(*settings, EKeys::Gamepad_RightY),
                               TEXT("Apply retains the remap in the active key profile"));
            })
            .Until(
                [this] {
                    return !checks.all_passed || canonical_remap_reloaded(EKeys::Gamepad_RightY);
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
                           native_pitch() * (original_pitch_inversion_ ? -1.0f : 1.0f) > 0.5f;
                },
                timeout)
            .Then([this] {
                checks.is_true(native_pitch() * (original_pitch_inversion_ ? -1.0f : 1.0f) > 0.5f,
                               TEXT("Remapped right-stick Pitch retains its semantic inversion"));
                send_slate_axis(EKeys::Gamepad_RightY, 0.0f);
            })
            .Then([this] {
                if (!checks.is_true(canonical_remap_reloaded(EKeys::Gamepad_RightY),
                                    TEXT("Saved semantic gamepad row survives a fresh reload"))) {
                    return;
                }
                auto* const controller{controller_.Get()};
                auto* const settings{NewObject<ioj::UOptionsDraft>(controller)};
                settings->begin_edit(*ioj::UGameSubsystem::get(controller->GetGameInstance()),
                                     controller->GetLocalPlayer(),
                                     ioj::GameSettingCategory::Controls);
                checks.is_true(settings->set_control_binding(
                                   remap_address_, EKeys::Gamepad_FaceButton_Right, false),
                               TEXT("A second remap can be previewed"));
                settings->cancel();
                checks.is_true(canonical_remap_reloaded(EKeys::Gamepad_RightY),
                               TEXT("Cancel does not persist the previewed remap"));
                checks.is_true(pitch_mapping_uses(*settings, EKeys::Gamepad_RightY),
                               TEXT("Cancel restores the previously applied live mapping"));
                settings->begin_edit(*ioj::UGameSubsystem::get(controller->GetGameInstance()),
                                     controller->GetLocalPlayer(),
                                     ioj::GameSettingCategory::Controls);
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
                           native_pitch() * (original_pitch_inversion_ ? -1.0f : 1.0f) > 0.5f;
                },
                timeout)
            .Then([this] {
                checks.is_true(native_pitch() * (original_pitch_inversion_ ? -1.0f : 1.0f) > 0.5f,
                               TEXT("Default left-stick Pitch matches the remapped right stick"));
            });
    }
};
