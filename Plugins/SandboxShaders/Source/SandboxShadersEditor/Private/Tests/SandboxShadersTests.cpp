#include "SbxShadersExperiments/EnergyShield/EnergyShieldExperimentActor.h"
#include "SbxShadersExperiments/SpaceEnergyField/SpaceEnergyFieldExperimentActor.h"

#include "Camera/CameraActor.h"
#include "Components/StaticMeshComponent.h"
#include "CQTest.h"
#include "Editor.h"
#include "Engine/Level.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Interfaces/IPluginManager.h"
#include "LevelEditorSubsystem.h"
#include "MaterialEditingLibrary.h"
#include "Materials/Material.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "RenderingThread.h"
#include "ShaderCore.h"

namespace {
constexpr TCHAR shield_material_path[]{
    TEXT("/SandboxShaders/Experiments/EnergyShield/M_EnergyShield.M_EnergyShield")};
constexpr TCHAR display_material_path[]{
    TEXT("/SandboxShaders/Experiments/SpaceEnergyField/"
         "M_SpaceEnergyFieldDisplay.M_SpaceEnergyFieldDisplay")};
constexpr TCHAR showcase_object_path[]{
    TEXT("/SandboxShaders/Showcase/SandboxShaders_Showcase.SandboxShaders_Showcase")};
constexpr TCHAR showcase_package_path[]{TEXT("/SandboxShaders/Showcase/SandboxShaders_Showcase")};

template <typename ActorType>
int32 count_actor_type(UWorld const& world) {
    auto const level{world.PersistentLevel};
    if (!IsValid(level)) {
        return 0;
    }

    int32 count{0};
    for (AActor const* const actor : level->Actors) {
        if (IsValid(actor) && actor->IsA<ActorType>()) {
            ++count;
        }
    }
    return count;
}
}

TEST_CLASS(ShaderInfrastructure, "SandboxShaders.UnitTests")
{
    TEST_METHOD(InitialisesModulesAndShaderMapping)
    {
        TestRunner->TestTrue(TEXT("SandboxShaders module is loaded"),
                             FModuleManager::Get().IsModuleLoaded(TEXT("SandboxShaders")));
        TestRunner->TestTrue(TEXT("SbxShadersExperiments module is loaded"),
                             FModuleManager::Get().IsModuleLoaded(TEXT("SbxShadersExperiments")));

        auto const plugin{IPluginManager::Get().FindPlugin(TEXT("SandboxShaders"))};
        if (!TestRunner->TestTrue(TEXT("SandboxShaders plugin is available"), plugin.IsValid())) {
            return;
        }

        auto const* const mapped_directory{
            AllShaderSourceDirectoryMappings().Find(TEXT("/Plugin/SandboxShaders"))};
        if (!TestRunner->TestNotNull(TEXT("Plugin shader directory is mapped"), mapped_directory)) {
            return;
        }

        auto const expected_directory{FPaths::ConvertRelativePathToFull(
            FPaths::Combine(plugin->GetBaseDir(), TEXT("Shaders")))};
        auto const actual_directory{FPaths::ConvertRelativePathToFull(*mapped_directory)};
        TestRunner->TestEqual(TEXT("Mapping points at the plugin shader directory"),
                              actual_directory,
                              expected_directory);
        TestRunner->TestTrue(TEXT("Energy shield HLSL resolves"),
                             FPaths::FileExists(GetShaderSourceFilePath(TEXT(
                                 "/Plugin/SandboxShaders/Private/EnergyShield/EnergyShield.ush"))));
        TestRunner->TestTrue(
            TEXT("Space field HLSL resolves"),
            FPaths::FileExists(GetShaderSourceFilePath(
                TEXT("/Plugin/SandboxShaders/Private/SpaceEnergyField/SpaceEnergyField.usf"))));
    }

    TEST_METHOD(LoadsMaterialsAndComposedShowcase)
    {
        TestRunner->TestNotNull(TEXT("Energy shield material loads"),
                                LoadObject<UMaterial>(nullptr, shield_material_path));
        TestRunner->TestNotNull(TEXT("Space field display material loads"),
                                LoadObject<UMaterial>(nullptr, display_material_path));

        auto* const world{LoadObject<UWorld>(nullptr, showcase_object_path)};
        if (!TestRunner->TestNotNull(TEXT("Showcase map loads"), world)) {
            return;
        }
        TestRunner->TestEqual(TEXT("Showcase contains one shield experiment"),
                              count_actor_type<AEnergyShieldExperimentActor>(*world),
                              1);
        TestRunner->TestEqual(TEXT("Showcase contains one space field experiment"),
                              count_actor_type<ASpaceEnergyFieldExperimentActor>(*world),
                              1);
        TestRunner->TestEqual(
            TEXT("Showcase contains one camera"), count_actor_type<ACameraActor>(*world), 1);
    }
};

TEST_CLASS(ShaderSmoke, "SandboxShaders.ShaderSmoke")
{
    TEST_METHOD(CompilesMaterialsAndDispatchesSpaceField)
    {
        if (!TestRunner->TestFalse(TEXT("Smoke test uses a real RHI"), GUsingNullRHI)) {
            return;
        }

        auto* const shield_material{LoadObject<UMaterial>(nullptr, shield_material_path)};
        auto* const display_material{LoadObject<UMaterial>(nullptr, display_material_path)};
        if (!TestRunner->TestNotNull(TEXT("Energy shield material loads"), shield_material) ||
            !TestRunner->TestNotNull(TEXT("Space field material loads"), display_material)) {
            return;
        }
        TestRunner->TestTrue(TEXT("Energy shield shader compiles without errors"),
                             UMaterialEditingLibrary::RecompileMaterial(shield_material).IsEmpty());
        TestRunner->TestTrue(
            TEXT("Space field display shader compiles without errors"),
            UMaterialEditingLibrary::RecompileMaterial(display_material).IsEmpty());

        auto* const level_editor{
            GEditor != nullptr ? GEditor->GetEditorSubsystem<ULevelEditorSubsystem>() : nullptr};
        if (!TestRunner->TestNotNull(TEXT("Level editor subsystem is available"), level_editor) ||
            !TestRunner->TestTrue(TEXT("Showcase opens"),
                                  level_editor->LoadLevel(showcase_package_path))) {
            return;
        }

        auto* const world{GEditor->GetEditorWorldContext().World()};
        ASpaceEnergyFieldExperimentActor* space_field{nullptr};
        AEnergyShieldExperimentActor* shield{nullptr};
        for (TActorIterator<AActor> actor_iterator{world}; actor_iterator; ++actor_iterator) {
            if (space_field == nullptr) {
                space_field = Cast<ASpaceEnergyFieldExperimentActor>(*actor_iterator);
            }
            if (shield == nullptr) {
                shield = Cast<AEnergyShieldExperimentActor>(*actor_iterator);
            }
        }
        if (!TestRunner->TestNotNull(TEXT("Space field actor is active"), space_field) ||
            !TestRunner->TestNotNull(TEXT("Shield actor is active"), shield)) {
            return;
        }

        shield->RerunConstructionScripts();
        space_field->RerunConstructionScripts();
        space_field->Tick(1.0f / 60.0f);
        FlushRenderingCommands();

        auto* const shield_mesh{Cast<UStaticMeshComponent>(shield->GetRootComponent())};
        auto* const field_mesh{Cast<UStaticMeshComponent>(space_field->GetRootComponent())};
        TestRunner->TestNotNull(TEXT("Shield has a runtime material"),
                                IsValid(shield_mesh) ? shield_mesh->GetMaterial(0) : nullptr);
        TestRunner->TestNotNull(TEXT("Space field has a runtime display material"),
                                IsValid(field_mesh) ? field_mesh->GetMaterial(0) : nullptr);
    }
};
