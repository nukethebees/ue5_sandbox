#include <SandboxTests/support/PlayerControllerTestAccess.h>
#include <SandboxTests/support/test_setup.h>

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
#include <EnhancedInputSubsystems.h>
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

    BEFORE_EACH()
    {
        checks.test_runner = TestRunner;
        checks.all_passed = true;
        level_setup.begin_test(TestCommandBuilder, *TestRunner, checks);
    }

    AFTER_EACH()
    {
        if (auto* const controller{controller_.Get()}) {
            controller->ConsoleCommand(TEXT("Input.-key Four"), true);
        }
        level_setup.end_test();
        controller_.Reset();
        subsystem_.Reset();
        settings_.Reset();
        active_profile_id_.Reset();
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
                settings->cancel();
            });
    }
};
