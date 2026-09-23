#include <SpaceGame/input/CanonicalShipControls.h>
#include <SpaceGame/input/ControlBindingMetadata.h>
#include <SpaceGame/input/SpaceGameInputModifier.h>
#include <SpaceGame/ships/common/SpaceShipControllerInputs.h>
#include <SpaceGame/ships/player/SpaceGamePlayerController.h>
#include <SpaceGame/simulation/SpaceGameLevelConfig.h>

#include <CQTest.h>
#include <InputAction.h>
#include <InputMappingContext.h>
#include <InputModifiers.h>
#include <PlayerMappableKeySettings.h>
#include <UObject/UnrealType.h>

#include <initializer_list>
#include <tuple>
#include <utility>

namespace {
auto action_names_for_key(UInputMappingContext const& context, FKey const key) -> FString {
    TArray<FString> names;
    for (auto const& mapping : context.GetMappings()) {
        if (mapping.Key == key && IsValid(mapping.Action)) {
            names.Add(mapping.Action->GetName().ToLower());
        }
    }
    names.Sort();
    return FString::Join(names, TEXT(","));
}
}

TEST_CLASS(CanonicalShipInput, "Sandbox.UnitTests")
{
    TEST_METHOD(GeneratedContextsAndActions)
    {
        for (auto const& definition : ml::ioj::canonical_ship_control_contexts()) {
            auto* const context{ml::ioj::load_ship_control_context(definition.scope)};
            if (!TestRunner->TestNotNull(definition.asset_name, context)) {
                continue;
            }
            TestRunner->TestTrue(TEXT("No legacy profile overrides"),
                                 context->GetProfilesWithOverridenMappings().IsEmpty());
            TSet<FName> identities;
            for (auto const& mapping : context->GetMappings()) {
                auto const name{mapping.GetMappingName()};
                TestRunner->TestFalse(TEXT("Mapping identity is valid"), name.IsNone());
                TestRunner->TestFalse(TEXT("Mapping identity is unique"),
                                      identities.Contains(name));
                identities.Add(name);
                auto const identity{name.ToString()};
                TestRunner->TestTrue(
                    TEXT("Mapping identity has owning scope"),
                    identity.StartsWith(FString{definition.asset_name}.RightChop(9) + TEXT(".")));
                TestRunner->TestTrue(TEXT("Mapping identity has hardware device"),
                                     identity.EndsWith(mapping.Key.IsGamepadKey()
                                                           ? TEXT(".Gamepad")
                                                           : TEXT(".KeyboardMouse")));
                auto const* const key_settings{mapping.GetPlayerMappableKeySettings()};
                auto const* const metadata{
                    IsValid(key_settings)
                        ? Cast<ml::ioj::UControlBindingMetadata>(key_settings->Metadata)
                        : nullptr};
                TestRunner->TestTrue(TEXT("Mapping metadata has owning scope"),
                                     IsValid(metadata) && metadata->scope == definition.scope);
            }
        }

        auto check_action = [this](TCHAR const* const name, EInputActionValueType const expected) {
            auto const path{FString::Printf(TEXT("/SpaceGame/Input/SpaceShip/%s.%s"), name, name)};
            auto* const action{LoadObject<UInputAction>(nullptr, *path)};
            if (TestRunner->TestNotNull(name, action)) {
                TestRunner->TestTrue(name, action->ValueType == expected);
            }
        };
        for (auto const* const name : {TEXT("IA_Ship_TranslateForward"),
                                       TEXT("IA_Ship_TranslateRight"),
                                       TEXT("IA_Ship_TranslateUp"),
                                       TEXT("IA_Ship_Pitch"),
                                       TEXT("IA_Ship_Yaw"),
                                       TEXT("IA_Ship_Roll"),
                                       TEXT("IA_Ship_Accelerate")}) {
            check_action(name, EInputActionValueType::Axis1D);
        }
        for (auto const* const name : {TEXT("IA_Ship_Brake"),
                                       TEXT("IA_Ship_Boost"),
                                       TEXT("IA_Ship_EmergencyBrake"),
                                       TEXT("IA_Ship_FirePrimary"),
                                       TEXT("IA_Ship_SelectStarfox"),
                                       TEXT("IA_Ship_SelectFighter"),
                                       TEXT("IA_Ship_SelectSkater"),
                                       TEXT("IA_Ship_SelectGunship")}) {
            check_action(name, EInputActionValueType::Boolean);
        }
    }

    TEST_METHOD(ExactPhysicalBindings)
    {
        using ml::ioj::EShipControlScope;
        using Binding = std::pair<FKey, TCHAR const*>;
        auto check_scope = [this](EShipControlScope const scope,
                                  std::initializer_list<Binding> const expected) {
            auto const* const context{ml::ioj::load_ship_control_context(scope)};
            if (!TestRunner->TestNotNull(TEXT("Canonical context loads"), context)) {
                return;
            }
            TestRunner->TestEqual(TEXT("Complete context has no extra historical mappings"),
                                  context->GetMappings().Num(),
                                  static_cast<int32>(expected.size()));
            for (auto const& [key, action] : expected) {
                TestRunner->TestEqual(TEXT("Physical key has exactly its expected action"),
                                      action_names_for_key(*context, key),
                                      FString{action}.ToLower());
            }
        };
        check_scope(EShipControlScope::General,
                    {{EKeys::Escape, TEXT("IA_pause")},
                     {EKeys::Gamepad_Special_Right, TEXT("IA_pause")},
                     {EKeys::One, TEXT("IA_Ship_SelectStarfox")},
                     {EKeys::Two, TEXT("IA_Ship_SelectFighter")},
                     {EKeys::Three, TEXT("IA_Ship_SelectSkater")},
                     {EKeys::Four, TEXT("IA_Ship_SelectGunship")},
                     {EKeys::Gamepad_DPad_Up, TEXT("IA_Ship_SelectStarfox")},
                     {EKeys::Gamepad_DPad_Right, TEXT("IA_Ship_SelectFighter")},
                     {EKeys::Gamepad_DPad_Down, TEXT("IA_Ship_SelectSkater")},
                     {EKeys::Gamepad_DPad_Left, TEXT("IA_Ship_SelectGunship")}});
        check_scope(EShipControlScope::Starfox,
                    {{EKeys::MouseY, TEXT("IA_Ship_Pitch")},
                     {EKeys::MouseX, TEXT("IA_Ship_Yaw")},
                     {EKeys::LeftMouseButton, TEXT("IA_Ship_FirePrimary")},
                     {EKeys::W, TEXT("IA_Ship_Accelerate")},
                     {EKeys::S, TEXT("IA_Ship_Brake")},
                     {EKeys::LeftShift, TEXT("IA_Ship_Boost")},
                     {EKeys::X, TEXT("IA_Ship_EmergencyBrake")},
                     {EKeys::Gamepad_LeftY, TEXT("IA_Ship_Pitch")},
                     {EKeys::Gamepad_LeftX, TEXT("IA_Ship_Yaw")},
                     {EKeys::Gamepad_LeftTriggerAxis, TEXT("IA_Ship_Accelerate")},
                     {EKeys::Gamepad_LeftShoulder, TEXT("IA_Ship_Brake")},
                     {EKeys::Gamepad_RightShoulder, TEXT("IA_Ship_Boost")},
                     {EKeys::Gamepad_FaceButton_Left, TEXT("IA_Ship_EmergencyBrake")},
                     {EKeys::Gamepad_RightTriggerAxis, TEXT("IA_Ship_FirePrimary")}});
        check_scope(EShipControlScope::Fighter,
                    {{EKeys::MouseY, TEXT("IA_Ship_Pitch")},
                     {EKeys::MouseX, TEXT("IA_Ship_Yaw")},
                     {EKeys::LeftMouseButton, TEXT("IA_Ship_FirePrimary")},
                     {EKeys::W, TEXT("IA_Ship_Accelerate")},
                     {EKeys::S, TEXT("IA_Ship_Brake")},
                     {EKeys::LeftShift, TEXT("IA_Ship_Boost")},
                     {EKeys::X, TEXT("IA_Ship_EmergencyBrake")},
                     {EKeys::Q, TEXT("IA_Ship_Roll")},
                     {EKeys::E, TEXT("IA_Ship_Roll")},
                     {EKeys::Gamepad_RightY, TEXT("IA_Ship_Pitch")},
                     {EKeys::Gamepad_RightX, TEXT("IA_Ship_Yaw")},
                     {EKeys::Gamepad_LeftX, TEXT("IA_Ship_Roll")},
                     {EKeys::Gamepad_LeftTriggerAxis, TEXT("IA_Ship_Accelerate")},
                     {EKeys::Gamepad_LeftShoulder, TEXT("IA_Ship_Brake")},
                     {EKeys::Gamepad_RightShoulder, TEXT("IA_Ship_Boost")},
                     {EKeys::Gamepad_FaceButton_Right, TEXT("IA_Ship_EmergencyBrake")},
                     {EKeys::Gamepad_RightTriggerAxis, TEXT("IA_Ship_FirePrimary")}});
        check_scope(EShipControlScope::Skater,
                    {{EKeys::MouseY, TEXT("IA_Ship_Pitch")},
                     {EKeys::MouseX, TEXT("IA_Ship_Yaw")},
                     {EKeys::LeftMouseButton, TEXT("IA_Ship_FirePrimary")},
                     {EKeys::W, TEXT("IA_Ship_Accelerate")},
                     {EKeys::S, TEXT("IA_Ship_Brake")},
                     {EKeys::LeftShift, TEXT("IA_Ship_Boost")},
                     {EKeys::X, TEXT("IA_Ship_EmergencyBrake")},
                     {EKeys::A, TEXT("IA_Ship_Roll")},
                     {EKeys::D, TEXT("IA_Ship_Roll")},
                     {EKeys::Gamepad_LeftY, TEXT("IA_Ship_Pitch")},
                     {EKeys::Gamepad_LeftX, TEXT("IA_Ship_Yaw")},
                     {EKeys::Gamepad_RightX, TEXT("IA_Ship_Roll")},
                     {EKeys::Gamepad_LeftTriggerAxis, TEXT("IA_Ship_Accelerate")},
                     {EKeys::Gamepad_LeftShoulder, TEXT("IA_Ship_Brake")},
                     {EKeys::Gamepad_RightShoulder, TEXT("IA_Ship_Boost")},
                     {EKeys::Gamepad_FaceButton_Right, TEXT("IA_Ship_EmergencyBrake")},
                     {EKeys::Gamepad_RightTriggerAxis, TEXT("IA_Ship_FirePrimary")}});
        check_scope(EShipControlScope::Gunship,
                    {{EKeys::MouseY, TEXT("IA_Ship_Pitch")},
                     {EKeys::MouseX, TEXT("IA_Ship_Yaw")},
                     {EKeys::LeftMouseButton, TEXT("IA_Ship_FirePrimary")},
                     {EKeys::W, TEXT("IA_Ship_TranslateForward")},
                     {EKeys::S, TEXT("IA_Ship_TranslateForward")},
                     {EKeys::D, TEXT("IA_Ship_TranslateRight")},
                     {EKeys::A, TEXT("IA_Ship_TranslateRight")},
                     {EKeys::SpaceBar, TEXT("IA_Ship_TranslateUp")},
                     {EKeys::C, TEXT("IA_Ship_TranslateUp")},
                     {EKeys::LeftShift, TEXT("IA_Ship_Boost")},
                     {EKeys::LeftControl, TEXT("IA_Ship_Brake")},
                     {EKeys::X, TEXT("IA_Ship_EmergencyBrake")},
                     {EKeys::Gamepad_LeftY, TEXT("IA_Ship_TranslateForward")},
                     {EKeys::Gamepad_LeftX, TEXT("IA_Ship_TranslateRight")},
                     {EKeys::Gamepad_RightY, TEXT("IA_Ship_Pitch")},
                     {EKeys::Gamepad_RightX, TEXT("IA_Ship_Yaw")},
                     {EKeys::Gamepad_FaceButton_Top, TEXT("IA_Ship_TranslateUp")},
                     {EKeys::Gamepad_FaceButton_Bottom, TEXT("IA_Ship_TranslateUp")},
                     {EKeys::Gamepad_RightShoulder, TEXT("IA_Ship_Boost")},
                     {EKeys::Gamepad_LeftShoulder, TEXT("IA_Ship_Brake")},
                     {EKeys::Gamepad_LeftTriggerAxis, TEXT("IA_Ship_EmergencyBrake")},
                     {EKeys::Gamepad_RightTriggerAxis, TEXT("IA_Ship_FirePrimary")}});
    }

    TEST_METHOD(DirectionalBindingsPreserveSign)
    {
        using ml::ioj::EShipControlScope;
        auto check_sign = [this](EShipControlScope const scope,
                                 FKey const key,
                                 TCHAR const* const action,
                                 bool const negative) {
            auto const* const context{ml::ioj::load_ship_control_context(scope)};
            if (!TestRunner->TestNotNull(TEXT("Directional context loads"), context)) {
                return;
            }
            int32 matching{};
            for (auto const& mapping : context->GetMappings()) {
                if (mapping.Key != key) {
                    continue;
                }
                ++matching;
                TestRunner->TestEqual(
                    TEXT("Directional action"), mapping.Action->GetName(), FString{action});
                int32 negate_count{};
                int32 response_count{};
                for (auto const modifier_ptr : mapping.Modifiers) {
                    auto const* const modifier{modifier_ptr.Get()};
                    negate_count += modifier->IsA<UInputModifierNegate>() ? 1 : 0;
                    response_count += modifier->IsA<ml::ioj::USpaceGameInputModifier>() ? 1 : 0;
                }
                TestRunner->TestEqual(TEXT("Negate presence matches authored direction"),
                                      negate_count,
                                      negative ? 1 : 0);
                TestRunner->TestEqual(TEXT("Gamepad directional response remains present"),
                                      response_count,
                                      key.IsGamepadKey() ? 1 : 0);
                TestRunner->TestEqual(TEXT("No unexpected directional modifier"),
                                      mapping.Modifiers.Num(),
                                      negate_count + response_count);
            }
            TestRunner->TestEqual(TEXT("Directional key has one mapping"), matching, 1);
        };
        check_sign(EShipControlScope::Fighter, EKeys::Q, TEXT("IA_Ship_Roll"), true);
        check_sign(EShipControlScope::Fighter, EKeys::E, TEXT("IA_Ship_Roll"), false);
        check_sign(EShipControlScope::Skater, EKeys::A, TEXT("IA_Ship_Roll"), true);
        check_sign(EShipControlScope::Skater, EKeys::D, TEXT("IA_Ship_Roll"), false);
        for (auto const& [positive, negative, action] :
             {std::tuple{EKeys::W, EKeys::S, TEXT("IA_Ship_TranslateForward")},
              std::tuple{EKeys::D, EKeys::A, TEXT("IA_Ship_TranslateRight")},
              std::tuple{EKeys::SpaceBar, EKeys::C, TEXT("IA_Ship_TranslateUp")},
              std::tuple{EKeys::Gamepad_FaceButton_Top,
                         EKeys::Gamepad_FaceButton_Bottom,
                         TEXT("IA_Ship_TranslateUp")}}) {
            check_sign(EShipControlScope::Gunship, positive, action, false);
            check_sign(EShipControlScope::Gunship, negative, action, true);
        }
    }

    TEST_METHOD(ProductionControllerUsesCanonicalContexts)
    {
        auto* const config{
            LoadObject<USpaceGameLevelConfig>(nullptr,
                                              TEXT("/SpaceGame/Levels/DA_GameRuntimeLevelConfig."
                                                   "DA_GameRuntimeLevelConfig"))};
        if (!TestRunner->TestNotNull(TEXT("Runtime config"), config)) {
            return;
        }
        auto const* const defaults{Cast<ASpaceGamePlayerController>(
            config->classes.player_controller_class.GetDefaultObject())};
        if (!TestRunner->TestNotNull(TEXT("Canonical player controller"), defaults)) {
            return;
        }
        auto const* const property{
            FindFProperty<FStructProperty>(defaults->GetClass(), TEXT("input"))};
        if (!TestRunner->TestNotNull(TEXT("Ship input property"), property)) {
            return;
        }
        auto const& input{*property->ContainerPtrToValuePtr<FSpaceShipControllerInputs>(defaults)};
        TestRunner->TestTrue(TEXT("Starfox asset"),
                             input.starfox == ml::ioj::load_ship_control_context(
                                                  ml::ioj::EShipControlScope::Starfox));
        TestRunner->TestTrue(TEXT("Fighter asset"),
                             input.fighter == ml::ioj::load_ship_control_context(
                                                  ml::ioj::EShipControlScope::Fighter));
        TestRunner->TestTrue(
            TEXT("Skater asset"),
            input.skater == ml::ioj::load_ship_control_context(ml::ioj::EShipControlScope::Skater));
        TestRunner->TestTrue(TEXT("Gunship asset"),
                             input.gunship == ml::ioj::load_ship_control_context(
                                                  ml::ioj::EShipControlScope::Gunship));
    }
};
