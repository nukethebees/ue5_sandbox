#include <SpaceGame/input/CanonicalShipControls.h>
#include <SpaceGame/input/ControlBindingMetadata.h>
#include <SpaceGame/input/SpaceGameInputModifier.h>
#include <SpaceGame/ships/common/SpaceShipControllerInputs.h>
#include <SpaceGame/ships/player/SpaceGamePlayerController.h>
#include <SpaceGame/simulation/SpaceGameLevelConfig.h>

#include <CQTest.h>
#include <EdGraph/EdGraph.h>
#include <EdGraph/EdGraphNode.h>
#include <Engine/Blueprint.h>
#include <Engine/BlueprintGeneratedClass.h>
#include <Engine/InputDelegateBinding.h>
#include <Engine/SimpleConstructionScript.h>
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
    TEST_METHOD(ProductionControllerBlueprintHasNoInputGraph)
    {
        auto* const blueprint{LoadObject<UBlueprint>(
            nullptr,
            TEXT("/SpaceGame/Players/BP_SpaceGamePlayerController.BP_SpaceGamePlayerController"))};
        if (!TestRunner->TestNotNull(TEXT("Production controller Blueprint"), blueprint)) {
            return;
        }
        TestRunner->TestTrue(TEXT("Direct C++ controller parent"),
                             blueprint->ParentClass ==
                                 ioj::ASpaceGamePlayerController::StaticClass());
        TArray<UEdGraph*> graphs;
        blueprint->GetAllGraphs(graphs);
        for (auto const* const graph : graphs) {
            for (auto const& node_ptr : graph->Nodes) {
                auto const* const node{node_ptr.Get()};
                if (graph->GetFName() == TEXT("UserConstructionScript") &&
                    node->GetClass()->GetFName() == TEXT("K2Node_FunctionEntry")) {
                    continue;
                }
                TestRunner->AddError(
                    FString::Printf(TEXT("Unexpected controller graph node: %s/%s"),
                                    *graph->GetName(),
                                    *node->GetClass()->GetName()));
            }
        }
        TestRunner->TestTrue(TEXT("No Blueprint-added components"),
                             blueprint->SimpleConstructionScript == nullptr ||
                                 blueprint->SimpleConstructionScript->GetAllNodes().IsEmpty());
        auto const* const generated{Cast<UBlueprintGeneratedClass>(blueprint->GeneratedClass)};
        if (!TestRunner->TestNotNull(TEXT("Generated controller class"), generated)) {
            return;
        }
        for (auto const& binding_ptr : generated->DynamicBindingObjects) {
            auto const* const binding{binding_ptr.Get()};
            TestRunner->TestFalse(TEXT("No Blueprint input binding"),
                                  IsValid(binding) && binding->IsA<UInputDelegateBinding>());
        }
    }

    TEST_METHOD(GeneratedContextsAndActions)
    {
        for (auto const& definition : ioj::canonical_ship_control_contexts()) {
            auto* const context{ioj::load_ship_control_context(definition.scope)};
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
                        ? Cast<ioj::UControlBindingMetadata>(key_settings->Metadata)
                        : nullptr};
                TestRunner->TestTrue(TEXT("Mapping metadata has owning scope"), IsValid(metadata));
            }
        }

        auto check_action = [this](TCHAR const* const name, EInputActionValueType const expected) {
            auto const path{FString::Printf(TEXT("/SpaceGame/Input/SpaceShip/%s.%s"), name, name)};
            auto* const action{LoadObject<UInputAction>(nullptr, *path)};
            if (TestRunner->TestNotNull(name, action)) {
                TestRunner->TestTrue(name, action->ValueType == expected);
            }
        };
        for (auto const* const name :
             {TEXT("IA_Ship_Pitch"), TEXT("IA_Ship_Yaw"), TEXT("IA_Ship_Accelerate")}) {
            check_action(name, EInputActionValueType::Axis1D);
        }
        for (auto const* const name : {TEXT("IA_Ship_Brake"),
                                       TEXT("IA_Ship_Boost"),
                                       TEXT("IA_Ship_EmergencyBrake"),
                                       TEXT("IA_Ship_FirePrimary")}) {
            check_action(name, EInputActionValueType::Boolean);
        }
    }

    TEST_METHOD(ExactPhysicalBindings)
    {
        using ioj::EShipControlScope;
        using Binding = std::pair<FKey, TCHAR const*>;
        auto check_scope = [this](EShipControlScope const scope,
                                  std::initializer_list<Binding> const expected) {
            auto const* const context{ioj::load_ship_control_context(scope)};
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
        check_scope(
            EShipControlScope::General,
            {{EKeys::Escape, TEXT("IA_pause")}, {EKeys::Gamepad_Special_Right, TEXT("IA_pause")}});
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
    }

    TEST_METHOD(ControllerAxesCarrySemanticIdentity)
    {
        using ioj::EShipControlScope;
        using ioj::ESpaceGameInputAxis;
        {
            auto const* const context{ioj::load_ship_control_context(EShipControlScope::Starfox)};
            if (!TestRunner->TestNotNull(TEXT("Flight context loads"), context)) {
                return;
            }
            for (auto const& mapping : context->GetMappings()) {
                if (!mapping.Key.IsGamepadKey() && mapping.Key != EKeys::MouseX &&
                    mapping.Key != EKeys::MouseY) {
                    continue;
                }
                ESpaceGameInputAxis expected{ESpaceGameInputAxis::None};
                auto const name{mapping.Action->GetName()};
                if (name == TEXT("IA_Ship_Pitch")) {
                    expected = ESpaceGameInputAxis::Pitch;
                } else if (name == TEXT("IA_Ship_Yaw")) {
                    expected = ESpaceGameInputAxis::Yaw;
                } else {
                    continue;
                }
                auto const* const response{mapping.Modifiers.FindByPredicate(
                    [](TObjectPtr<UInputModifier> const& modifier) {
                        return modifier.Get()->IsA<ioj::USpaceGameInputModifier>();
                    })};
                TestRunner->TestTrue(TEXT("Semantic axis response exists"), response != nullptr);
                if (response != nullptr) {
                    TestRunner->TestTrue(
                        TEXT("Semantic axis matches action"),
                        CastChecked<ioj::USpaceGameInputModifier>(response->Get())->axis ==
                            expected);
                }
            }
        }
    }

    TEST_METHOD(ProductionControllerUsesCanonicalContexts)
    {
        auto* const config{LoadObject<ioj::USpaceGameLevelConfig>(
            nullptr,
            TEXT("/SpaceGame/Levels/DA_GameRuntimeLevelConfig."
                 "DA_GameRuntimeLevelConfig"))};
        if (!TestRunner->TestNotNull(TEXT("Runtime config"), config)) {
            return;
        }
        auto const* const defaults{Cast<ioj::ASpaceGamePlayerController>(
            config->classes.player_controller_class.GetDefaultObject())};
        if (!TestRunner->TestNotNull(TEXT("Canonical player controller"), defaults)) {
            return;
        }
        auto const* const property{
            FindFProperty<FStructProperty>(defaults->GetClass(), TEXT("input"))};
        if (!TestRunner->TestNotNull(TEXT("Ship input property"), property)) {
            return;
        }
        auto const& input{
            *property->ContainerPtrToValuePtr<ioj::FSpaceShipControllerInputs>(defaults)};
        TestRunner->TestTrue(TEXT("Starfox asset"),
                             input.starfox ==
                                 ioj::load_ship_control_context(ioj::EShipControlScope::Starfox));
    }
};
