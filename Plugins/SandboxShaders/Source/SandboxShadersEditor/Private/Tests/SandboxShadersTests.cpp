#include "SbxShadersExperiments/EnergyShield/EnergyShieldExperimentActor.h"
#include "SbxShadersExperiments/RadarDisplay/RadarDisplayExperimentActor.h"
#include "SbxShadersExperiments/ShieldImpact/ShieldImpactExperimentActor.h"
#include "SbxShadersExperiments/SpaceEnergyField/SpaceEnergyFieldExperimentActor.h"
#include "SbxShadersExperiments/VertexRipple/VertexRippleExperimentActor.h"

#include "Camera/CameraActor.h"
#include "Components/StaticMeshComponent.h"
#include "CQTest.h"
#include "Editor.h"
#include "Engine/Level.h"
#include "Engine/StaticMesh.h"
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
constexpr TCHAR impact_material_path[]{
    TEXT("/SandboxShaders/Experiments/ShieldImpact/M_ShieldImpact.M_ShieldImpact")};
constexpr TCHAR ripple_material_path[]{
    TEXT("/SandboxShaders/Experiments/VertexRipple/M_VertexRipple.M_VertexRipple")};
constexpr TCHAR ripple_mesh_path[]{
    TEXT("/SandboxShaders/Experiments/VertexRipple/SM_VertexRippleGrid.SM_VertexRippleGrid")};
constexpr TCHAR radar_material_path[]{
    TEXT("/SandboxShaders/Experiments/RadarDisplay/M_RadarDisplay.M_RadarDisplay")};
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
        TestRunner->TestTrue(TEXT("Shield impact HLSL resolves"),
                             FPaths::FileExists(GetShaderSourceFilePath(TEXT(
                                 "/Plugin/SandboxShaders/Private/ShieldImpact/ShieldImpact.ush"))));
        TestRunner->TestTrue(TEXT("Vertex ripple HLSL resolves"),
                             FPaths::FileExists(GetShaderSourceFilePath(TEXT(
                                 "/Plugin/SandboxShaders/Private/VertexRipple/VertexRipple.ush"))));
        TestRunner->TestTrue(TEXT("Radar display HLSL resolves"),
                             FPaths::FileExists(GetShaderSourceFilePath(TEXT(
                                 "/Plugin/SandboxShaders/Private/RadarDisplay/RadarDisplay.ush"))));
    }

    TEST_METHOD(LoadsMaterialsAndComposedShowcase)
    {
        TestRunner->TestNotNull(TEXT("Energy shield material loads"),
                                LoadObject<UMaterial>(nullptr, shield_material_path));
        TestRunner->TestNotNull(TEXT("Space field display material loads"),
                                LoadObject<UMaterial>(nullptr, display_material_path));
        TestRunner->TestNotNull(TEXT("Shield impact material loads"),
                                LoadObject<UMaterial>(nullptr, impact_material_path));
        TestRunner->TestNotNull(TEXT("Vertex ripple material loads"),
                                LoadObject<UMaterial>(nullptr, ripple_material_path));
        TestRunner->TestNotNull(TEXT("Vertex ripple grid loads"),
                                LoadObject<UStaticMesh>(nullptr, ripple_mesh_path));
        TestRunner->TestNotNull(TEXT("Radar display material loads"),
                                LoadObject<UMaterial>(nullptr, radar_material_path));

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
        TestRunner->TestEqual(TEXT("Showcase contains one shield impact experiment"),
                              count_actor_type<AShieldImpactExperimentActor>(*world),
                              1);
        TestRunner->TestEqual(TEXT("Showcase contains one vertex ripple experiment"),
                              count_actor_type<AVertexRippleExperimentActor>(*world),
                              1);
        TestRunner->TestEqual(TEXT("Showcase contains one radar display experiment"),
                              count_actor_type<ARadarDisplayExperimentActor>(*world),
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
        auto* const impact_material{LoadObject<UMaterial>(nullptr, impact_material_path)};
        auto* const ripple_material{LoadObject<UMaterial>(nullptr, ripple_material_path)};
        auto* const radar_material{LoadObject<UMaterial>(nullptr, radar_material_path)};
        if (!TestRunner->TestNotNull(TEXT("Energy shield material loads"), shield_material) ||
            !TestRunner->TestNotNull(TEXT("Space field material loads"), display_material) ||
            !TestRunner->TestNotNull(TEXT("Shield impact material loads"), impact_material) ||
            !TestRunner->TestNotNull(TEXT("Vertex ripple material loads"), ripple_material) ||
            !TestRunner->TestNotNull(TEXT("Radar display material loads"), radar_material)) {
            return;
        }
        TestRunner->TestTrue(TEXT("Energy shield shader compiles without errors"),
                             UMaterialEditingLibrary::RecompileMaterial(shield_material).IsEmpty());
        TestRunner->TestTrue(
            TEXT("Space field display shader compiles without errors"),
            UMaterialEditingLibrary::RecompileMaterial(display_material).IsEmpty());
        TestRunner->TestTrue(TEXT("Shield impact shader compiles without errors"),
                             UMaterialEditingLibrary::RecompileMaterial(impact_material).IsEmpty());
        TestRunner->TestTrue(TEXT("Vertex ripple shader compiles without errors"),
                             UMaterialEditingLibrary::RecompileMaterial(ripple_material).IsEmpty());
        TestRunner->TestTrue(TEXT("Radar display shader compiles without errors"),
                             UMaterialEditingLibrary::RecompileMaterial(radar_material).IsEmpty());

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
        AShieldImpactExperimentActor* impact{nullptr};
        AVertexRippleExperimentActor* ripple{nullptr};
        ARadarDisplayExperimentActor* radar{nullptr};
        for (TActorIterator<AActor> actor_iterator{world}; actor_iterator; ++actor_iterator) {
            if (space_field == nullptr) {
                space_field = Cast<ASpaceEnergyFieldExperimentActor>(*actor_iterator);
            }
            if (shield == nullptr) {
                shield = Cast<AEnergyShieldExperimentActor>(*actor_iterator);
            }
            if (impact == nullptr) {
                impact = Cast<AShieldImpactExperimentActor>(*actor_iterator);
            }
            if (ripple == nullptr) {
                ripple = Cast<AVertexRippleExperimentActor>(*actor_iterator);
            }
            if (radar == nullptr) {
                radar = Cast<ARadarDisplayExperimentActor>(*actor_iterator);
            }
        }
        if (!TestRunner->TestNotNull(TEXT("Space field actor is active"), space_field) ||
            !TestRunner->TestNotNull(TEXT("Shield actor is active"), shield) ||
            !TestRunner->TestNotNull(TEXT("Shield impact actor is active"), impact) ||
            !TestRunner->TestNotNull(TEXT("Vertex ripple actor is active"), ripple) ||
            !TestRunner->TestNotNull(TEXT("Radar actor is active"), radar)) {
            return;
        }

        shield->RerunConstructionScripts();
        space_field->RerunConstructionScripts();
        impact->RerunConstructionScripts();
        ripple->RerunConstructionScripts();
        radar->RerunConstructionScripts();
        impact->trigger_impact_at_local_position(FVector{-1.0, 0.25, 0.1});
        impact->Tick(1.0f / 60.0f);
        space_field->Tick(1.0f / 60.0f);
        FlushRenderingCommands();

        auto* const shield_mesh{Cast<UStaticMeshComponent>(shield->GetRootComponent())};
        auto* const field_mesh{Cast<UStaticMeshComponent>(space_field->GetRootComponent())};
        auto* const impact_mesh{Cast<UStaticMeshComponent>(impact->GetRootComponent())};
        auto* const ripple_mesh{Cast<UStaticMeshComponent>(ripple->GetRootComponent())};
        auto* const radar_mesh{Cast<UStaticMeshComponent>(radar->GetRootComponent())};
        TestRunner->TestNotNull(TEXT("Shield has a runtime material"),
                                IsValid(shield_mesh) ? shield_mesh->GetMaterial(0) : nullptr);
        TestRunner->TestNotNull(TEXT("Space field has a runtime display material"),
                                IsValid(field_mesh) ? field_mesh->GetMaterial(0) : nullptr);
        TestRunner->TestNotNull(TEXT("Shield impact has a runtime material"),
                                IsValid(impact_mesh) ? impact_mesh->GetMaterial(0) : nullptr);
        TestRunner->TestNotNull(TEXT("Vertex ripple has a runtime material"),
                                IsValid(ripple_mesh) ? ripple_mesh->GetMaterial(0) : nullptr);
        TestRunner->TestNotNull(TEXT("Radar has a runtime material"),
                                IsValid(radar_mesh) ? radar_mesh->GetMaterial(0) : nullptr);
    }
};
