#include <SandboxTests/support/PlayerControllerTestAccess.h>
#include <SandboxTests/support/test_setup.h>

#include <ioj/sim/player/sim.h>
#include <SpaceGame/input/CanonicalShipControls.h>
#include <SpaceGame/levels/ExampleLevels.h>
#include <SpaceGame/levels/LevelLoader.h>
#include <SpaceGame/settings/GameSettingsSubsystem.h>
#include <SpaceGame/ships/player/SpaceGamePlayerController.h>
#include <SpaceGame/ships/player/TestSpaceShip.h>
#include <SpaceGame/simulation/TestBatchOrchestrator.h>

#include <CQTest.h>
#include <Engine/LocalPlayer.h>
#include <EnhancedInputComponent.h>
#include <EnhancedInputDeveloperSettings.h>
#include <EnhancedInputSubsystems.h>
#include <InputAction.h>
#include <InputMappingContext.h>
#include <Kismet/GameplayStatics.h>
#include <Misc/Guid.h>
#include <PlayerMappableKeySettings.h>
#include <UObject/UnrealType.h>
#include <UserSettings/EnhancedInputUserSettings.h>

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

    BEFORE_EACH()
    {
        checks.test_runner = TestRunner;
        checks.all_passed = true;
        level_setup.begin_test(TestCommandBuilder, *TestRunner, checks);
    }

    AFTER_EACH()
    {
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
        }
        level_setup.end_test();
        controller_.Reset();
        subsystem_.Reset();
        settings_.Reset();
        active_profile_id_.Reset();
        original_save_slot_.Reset();
        test_save_slot_.Reset();
        restore_remap_ = false;
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

    TEST_METHOD(DirectGameplayStartupAndModeSwitch)
    {
        TestCommandBuilder.Do([this] { setup(); })
            .Until([this] { return !checks.all_passed || ready(); }, timeout)
            .Then([this] {
                if (!checks.is_true(ready(), TEXT("Starfox context activates after possession"))) {
                    return;
                }
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
                    EHardwareDevicePrimaryType::Gamepad, ml::ioj::EShipControlScope::Gunship)};
                auto const* const binding{bindings.FindByPredicate([](auto const& candidate) {
                    return candidate.address.mapping_name == FName{TEXT("Gunship.Pitch.Gamepad")};
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

                checks.is_true(settings->set_control_binding(
                                   remap_address_, EKeys::Gamepad_FaceButton_Left, false),
                               TEXT("Canonical remap previews in the edit transaction"));
                settings->apply();
                checks.is_true(settings
                                   ->control_bindings(EHardwareDevicePrimaryType::Gamepad,
                                                      ml::ioj::EShipControlScope::Gunship)
                                   .ContainsByPredicate([](auto const& current) {
                                       return current.address.mapping_name ==
                                                  FName{TEXT("Gunship.Pitch.Gamepad")} &&
                                              current.current_key == EKeys::Gamepad_FaceButton_Left;
                                   }),
                               TEXT("Apply retains the remap in the active key profile"));
            })
            .Until(
                [this] {
                    return !checks.all_passed ||
                           canonical_remap_reloaded(EKeys::Gamepad_FaceButton_Left);
                },
                FTimespan::FromSeconds(5))
            .Then([this] {
                if (!checks.is_true(canonical_remap_reloaded(EKeys::Gamepad_FaceButton_Left),
                                    TEXT("Saved semantic gamepad row survives a fresh reload"))) {
                    return;
                }
                auto* const controller{controller_.Get()};
                auto* const settings{
                    controller->GetGameInstance()->GetSubsystem<ml::ioj::UGameSettingsSubsystem>()};
                settings->begin_edit(controller->GetLocalPlayer());
                checks.is_true(settings->set_control_binding(
                                   remap_address_, EKeys::Gamepad_FaceButton_Right, false),
                               TEXT("A second remap can be previewed"));
                settings->cancel();
                checks.is_true(canonical_remap_reloaded(EKeys::Gamepad_FaceButton_Left),
                               TEXT("Cancel does not persist the previewed remap"));
                checks.is_true(settings
                                   ->control_bindings(EHardwareDevicePrimaryType::Gamepad,
                                                      ml::ioj::EShipControlScope::Gunship)
                                   .ContainsByPredicate([](auto const& current) {
                                       return current.address.mapping_name ==
                                                  FName{TEXT("Gunship.Pitch.Gamepad")} &&
                                              current.current_key == EKeys::Gamepad_FaceButton_Left;
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
