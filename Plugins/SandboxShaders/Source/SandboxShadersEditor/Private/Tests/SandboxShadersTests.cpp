#include "SbxShadersExperiments/ConstructionSpawn/ConstructionSpawnExperimentActor.h"
#include "SbxShadersExperiments/EnergyBeam/EnergyBeamExperimentActor.h"
#include "SbxShadersExperiments/EnergyShield/EnergyShieldExperimentActor.h"
#include "SbxShadersExperiments/EngineExhaust/EngineExhaustExperimentActor.h"
#include "SbxShadersExperiments/PlanetAtmosphere/PlanetAtmosphereExperimentActor.h"
#include "SbxShadersExperiments/RadarDisplay/RadarDisplayExperimentActor.h"
#include "SbxShadersExperiments/RaymarchedAnomaly/RaymarchedAnomalyExperimentActor.h"
#include "SbxShadersExperiments/ShieldImpact/ShieldImpactExperimentActor.h"
#include "SbxShadersExperiments/SpaceEnergyField/SpaceEnergyFieldExperimentActor.h"
#include "SbxShadersExperiments/TacticalScan/TacticalScanExperimentActor.h"
#include "SbxShadersExperiments/VertexRipple/VertexRippleExperimentActor.h"
#include "SbxShadersExperiments/WarpField/WarpFieldExperimentActor.h"

#include "Camera/CameraActor.h"
#include "Components/PostProcessComponent.h"
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
#include "Materials/MaterialInstanceDynamic.h"
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
constexpr TCHAR tactical_scan_material_path[]{
    TEXT("/SandboxShaders/Experiments/TacticalScan/M_TacticalScan.M_TacticalScan")};
constexpr TCHAR warp_field_material_path[]{
    TEXT("/SandboxShaders/Experiments/WarpField/M_WarpField.M_WarpField")};
constexpr TCHAR warp_backdrop_material_path[]{
    TEXT("/SandboxShaders/Experiments/WarpField/M_WarpBackdrop.M_WarpBackdrop")};
constexpr TCHAR anomaly_material_path[]{
    TEXT("/SandboxShaders/Experiments/RaymarchedAnomaly/M_RaymarchedAnomaly.M_RaymarchedAnomaly")};
constexpr TCHAR exhaust_material_path[]{
    TEXT("/SandboxShaders/Experiments/EngineExhaust/M_EngineExhaust.M_EngineExhaust")};
constexpr TCHAR planet_surface_material_path[]{
    TEXT("/SandboxShaders/Experiments/PlanetAtmosphere/M_PlanetSurface.M_PlanetSurface")};
constexpr TCHAR planet_atmosphere_material_path[]{
    TEXT("/SandboxShaders/Experiments/PlanetAtmosphere/M_PlanetAtmosphere.M_PlanetAtmosphere")};
constexpr TCHAR construction_material_path[]{
    TEXT("/SandboxShaders/Experiments/ConstructionSpawn/M_ConstructionSpawn.M_ConstructionSpawn")};
constexpr TCHAR energy_beam_material_path[]{
    TEXT("/SandboxShaders/Experiments/EnergyBeam/M_EnergyBeam.M_EnergyBeam")};
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
        TestRunner->TestTrue(TEXT("Tactical scan HLSL resolves"),
                             FPaths::FileExists(GetShaderSourceFilePath(TEXT(
                                 "/Plugin/SandboxShaders/Private/TacticalScan/TacticalScan.ush"))));
        TestRunner->TestTrue(TEXT("Warp field HLSL resolves"),
                             FPaths::FileExists(GetShaderSourceFilePath(
                                 TEXT("/Plugin/SandboxShaders/Private/WarpField/WarpField.ush"))));
        TestRunner->TestTrue(
            TEXT("Raymarched anomaly HLSL resolves"),
            FPaths::FileExists(GetShaderSourceFilePath(
                TEXT("/Plugin/SandboxShaders/Private/RaymarchedAnomaly/RaymarchedAnomaly.ush"))));
        TestRunner->TestTrue(
            TEXT("Engine exhaust HLSL resolves"),
            FPaths::FileExists(GetShaderSourceFilePath(
                TEXT("/Plugin/SandboxShaders/Private/EngineExhaust/EngineExhaust.ush"))));
        TestRunner->TestTrue(
            TEXT("Planet atmosphere HLSL resolves"),
            FPaths::FileExists(GetShaderSourceFilePath(
                TEXT("/Plugin/SandboxShaders/Private/PlanetAtmosphere/PlanetAtmosphere.ush"))));
        TestRunner->TestTrue(
            TEXT("Construction spawn HLSL resolves"),
            FPaths::FileExists(GetShaderSourceFilePath(
                TEXT("/Plugin/SandboxShaders/Private/ConstructionSpawn/ConstructionSpawn.ush"))));
        TestRunner->TestTrue(TEXT("Energy beam HLSL resolves"),
                             FPaths::FileExists(GetShaderSourceFilePath(TEXT(
                                 "/Plugin/SandboxShaders/Private/EnergyBeam/EnergyBeam.ush"))));
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
        TestRunner->TestNotNull(TEXT("Tactical scan material loads"),
                                LoadObject<UMaterial>(nullptr, tactical_scan_material_path));
        TestRunner->TestNotNull(TEXT("Warp field material loads"),
                                LoadObject<UMaterial>(nullptr, warp_field_material_path));
        TestRunner->TestNotNull(TEXT("Warp backdrop material loads"),
                                LoadObject<UMaterial>(nullptr, warp_backdrop_material_path));
        TestRunner->TestNotNull(TEXT("Raymarched anomaly material loads"),
                                LoadObject<UMaterial>(nullptr, anomaly_material_path));
        TestRunner->TestNotNull(TEXT("Engine exhaust material loads"),
                                LoadObject<UMaterial>(nullptr, exhaust_material_path));
        TestRunner->TestNotNull(TEXT("Planet surface material loads"),
                                LoadObject<UMaterial>(nullptr, planet_surface_material_path));
        TestRunner->TestNotNull(TEXT("Planet atmosphere material loads"),
                                LoadObject<UMaterial>(nullptr, planet_atmosphere_material_path));
        TestRunner->TestNotNull(TEXT("Construction spawn material loads"),
                                LoadObject<UMaterial>(nullptr, construction_material_path));
        TestRunner->TestNotNull(TEXT("Energy beam material loads"),
                                LoadObject<UMaterial>(nullptr, energy_beam_material_path));

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
        TestRunner->TestEqual(TEXT("Showcase contains one tactical scan experiment"),
                              count_actor_type<ATacticalScanExperimentActor>(*world),
                              1);
        TestRunner->TestEqual(TEXT("Showcase contains one warp field experiment"),
                              count_actor_type<AWarpFieldExperimentActor>(*world),
                              1);
        TestRunner->TestEqual(TEXT("Showcase contains one raymarched anomaly experiment"),
                              count_actor_type<ARaymarchedAnomalyExperimentActor>(*world),
                              1);
        TestRunner->TestEqual(TEXT("Showcase contains one engine exhaust experiment"),
                              count_actor_type<AEngineExhaustExperimentActor>(*world),
                              1);
        TestRunner->TestEqual(TEXT("Showcase contains one planet atmosphere experiment"),
                              count_actor_type<APlanetAtmosphereExperimentActor>(*world),
                              1);
        TestRunner->TestEqual(TEXT("Showcase contains one construction spawn experiment"),
                              count_actor_type<AConstructionSpawnExperimentActor>(*world),
                              1);
        TestRunner->TestEqual(TEXT("Showcase contains one energy beam experiment"),
                              count_actor_type<AEnergyBeamExperimentActor>(*world),
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
        auto* const tactical_scan_material{
            LoadObject<UMaterial>(nullptr, tactical_scan_material_path)};
        auto* const warp_field_material{LoadObject<UMaterial>(nullptr, warp_field_material_path)};
        auto* const anomaly_material{LoadObject<UMaterial>(nullptr, anomaly_material_path)};
        auto* const exhaust_material{LoadObject<UMaterial>(nullptr, exhaust_material_path)};
        auto* const planet_surface_material{
            LoadObject<UMaterial>(nullptr, planet_surface_material_path)};
        auto* const planet_atmosphere_material{
            LoadObject<UMaterial>(nullptr, planet_atmosphere_material_path)};
        auto* const construction_material{
            LoadObject<UMaterial>(nullptr, construction_material_path)};
        auto* const energy_beam_material{LoadObject<UMaterial>(nullptr, energy_beam_material_path)};
        if (!TestRunner->TestNotNull(TEXT("Energy shield material loads"), shield_material) ||
            !TestRunner->TestNotNull(TEXT("Space field material loads"), display_material) ||
            !TestRunner->TestNotNull(TEXT("Shield impact material loads"), impact_material) ||
            !TestRunner->TestNotNull(TEXT("Vertex ripple material loads"), ripple_material) ||
            !TestRunner->TestNotNull(TEXT("Radar display material loads"), radar_material) ||
            !TestRunner->TestNotNull(TEXT("Tactical scan material loads"),
                                     tactical_scan_material) ||
            !TestRunner->TestNotNull(TEXT("Warp field material loads"), warp_field_material) ||
            !TestRunner->TestNotNull(TEXT("Raymarched anomaly material loads"), anomaly_material) ||
            !TestRunner->TestNotNull(TEXT("Engine exhaust material loads"), exhaust_material) ||
            !TestRunner->TestNotNull(TEXT("Planet surface material loads"),
                                     planet_surface_material) ||
            !TestRunner->TestNotNull(TEXT("Planet atmosphere material loads"),
                                     planet_atmosphere_material) ||
            !TestRunner->TestNotNull(TEXT("Construction spawn material loads"),
                                     construction_material) ||
            !TestRunner->TestNotNull(TEXT("Energy beam material loads"), energy_beam_material)) {
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
        TestRunner->TestTrue(
            TEXT("Tactical scan shader compiles without errors"),
            UMaterialEditingLibrary::RecompileMaterial(tactical_scan_material).IsEmpty());
        TestRunner->TestTrue(
            TEXT("Warp field shader compiles without errors"),
            UMaterialEditingLibrary::RecompileMaterial(warp_field_material).IsEmpty());
        TestRunner->TestTrue(
            TEXT("Raymarched anomaly shader compiles without errors"),
            UMaterialEditingLibrary::RecompileMaterial(anomaly_material).IsEmpty());
        TestRunner->TestTrue(
            TEXT("Engine exhaust shader compiles without errors"),
            UMaterialEditingLibrary::RecompileMaterial(exhaust_material).IsEmpty());
        TestRunner->TestTrue(
            TEXT("Planet surface shader compiles without errors"),
            UMaterialEditingLibrary::RecompileMaterial(planet_surface_material).IsEmpty());
        TestRunner->TestTrue(
            TEXT("Planet atmosphere shader compiles without errors"),
            UMaterialEditingLibrary::RecompileMaterial(planet_atmosphere_material).IsEmpty());
        TestRunner->TestTrue(
            TEXT("Construction spawn shader compiles without errors"),
            UMaterialEditingLibrary::RecompileMaterial(construction_material).IsEmpty());
        TestRunner->TestTrue(
            TEXT("Energy beam shader compiles without errors"),
            UMaterialEditingLibrary::RecompileMaterial(energy_beam_material).IsEmpty());

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
        ATacticalScanExperimentActor* tactical_scan{nullptr};
        AWarpFieldExperimentActor* warp_field{nullptr};
        ARaymarchedAnomalyExperimentActor* anomaly{nullptr};
        AEngineExhaustExperimentActor* exhaust{nullptr};
        APlanetAtmosphereExperimentActor* planet_atmosphere{nullptr};
        AConstructionSpawnExperimentActor* construction{nullptr};
        AEnergyBeamExperimentActor* energy_beam{nullptr};
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
            if (tactical_scan == nullptr) {
                tactical_scan = Cast<ATacticalScanExperimentActor>(*actor_iterator);
            }
            if (warp_field == nullptr) {
                warp_field = Cast<AWarpFieldExperimentActor>(*actor_iterator);
            }
            if (anomaly == nullptr) {
                anomaly = Cast<ARaymarchedAnomalyExperimentActor>(*actor_iterator);
            }
            if (exhaust == nullptr) {
                exhaust = Cast<AEngineExhaustExperimentActor>(*actor_iterator);
            }
            if (planet_atmosphere == nullptr) {
                planet_atmosphere = Cast<APlanetAtmosphereExperimentActor>(*actor_iterator);
            }
            if (construction == nullptr) {
                construction = Cast<AConstructionSpawnExperimentActor>(*actor_iterator);
            }
            if (energy_beam == nullptr) {
                energy_beam = Cast<AEnergyBeamExperimentActor>(*actor_iterator);
            }
        }
        if (!TestRunner->TestNotNull(TEXT("Space field actor is active"), space_field) ||
            !TestRunner->TestNotNull(TEXT("Shield actor is active"), shield) ||
            !TestRunner->TestNotNull(TEXT("Shield impact actor is active"), impact) ||
            !TestRunner->TestNotNull(TEXT("Vertex ripple actor is active"), ripple) ||
            !TestRunner->TestNotNull(TEXT("Radar actor is active"), radar) ||
            !TestRunner->TestNotNull(TEXT("Tactical scan actor is active"), tactical_scan) ||
            !TestRunner->TestNotNull(TEXT("Warp field actor is active"), warp_field) ||
            !TestRunner->TestNotNull(TEXT("Raymarched anomaly actor is active"), anomaly) ||
            !TestRunner->TestNotNull(TEXT("Engine exhaust actor is active"), exhaust) ||
            !TestRunner->TestNotNull(TEXT("Planet atmosphere actor is active"),
                                     planet_atmosphere) ||
            !TestRunner->TestNotNull(TEXT("Construction spawn actor is active"), construction) ||
            !TestRunner->TestNotNull(TEXT("Energy beam actor is active"), energy_beam)) {
            return;
        }

        shield->RerunConstructionScripts();
        space_field->RerunConstructionScripts();
        impact->RerunConstructionScripts();
        ripple->RerunConstructionScripts();
        radar->RerunConstructionScripts();
        tactical_scan->RerunConstructionScripts();
        warp_field->RerunConstructionScripts();
        anomaly->RerunConstructionScripts();
        exhaust->RerunConstructionScripts();
        planet_atmosphere->RerunConstructionScripts();
        construction->RerunConstructionScripts();
        construction->reset_effect();
        TestRunner->TestEqual(TEXT("Construction reset control updates progress"),
                              construction->settings.progress,
                              0.0f);
        construction->complete_effect();
        energy_beam->RerunConstructionScripts();
        energy_beam->set_short_beam();
        TestRunner->TestEqual(TEXT("Short beam preset updates endpoint distance"),
                              energy_beam->beam_length(),
                              450.0f);
        energy_beam->set_long_beam();
        energy_beam->reverse_direction();
        energy_beam->Tick(1.0f / 60.0f);
        planet_atmosphere->settings.density = 3.25f;
        planet_atmosphere->RerunConstructionScripts();
        impact->settings.auto_repeat = false;
        impact->clear_impacts();
        impact->add_impact(FVector{-1.0, 0.25, 0.1}, 0.03f, 1.0f);
        impact->add_impact(FVector{-1.0, -0.3, 0.2}, 0.04f, 0.8f);
        impact->add_impact(FVector{-0.8, 0.1, -0.5}, 0.02f, 1.2f);
        impact->add_impact(FVector{-0.9, -0.2, -0.3}, 0.05f, 0.9f);
        impact->add_impact(FVector{-0.7, 0.5, 0.1}, 0.03f, 1.0f);
        TestRunner->TestEqual(
            TEXT("Shield impact buffer remains bounded"), impact->active_impact_count(), 4);
        impact->Tick(1.0f / 60.0f);
        tactical_scan->restart_scan();
        tactical_scan->Tick(1.0f / 60.0f);
        warp_field->restart_animation();
        warp_field->Tick(1.0f / 60.0f);
        anomaly->restart_animation();
        anomaly->Tick(1.0f / 60.0f);
        exhaust->set_half_throttle();
        exhaust->Tick(1.0f / 60.0f);
        TestRunner->TestEqual(TEXT("Exhaust half-throttle control updates settings"),
                              exhaust->settings.throttle,
                              0.5f);
        space_field->Tick(1.0f / 60.0f);
        FlushRenderingCommands();

        auto* const shield_mesh{Cast<UStaticMeshComponent>(shield->GetRootComponent())};
        auto* const field_mesh{Cast<UStaticMeshComponent>(space_field->GetRootComponent())};
        auto* const impact_mesh{Cast<UStaticMeshComponent>(impact->GetRootComponent())};
        auto* const ripple_mesh{Cast<UStaticMeshComponent>(ripple->GetRootComponent())};
        auto* const radar_mesh{Cast<UStaticMeshComponent>(radar->GetRootComponent())};
        auto* const warp_mesh{Cast<UStaticMeshComponent>(warp_field->GetRootComponent())};
        auto* const anomaly_mesh{Cast<UStaticMeshComponent>(anomaly->GetRootComponent())};
        auto* const exhaust_mesh{Cast<UStaticMeshComponent>(exhaust->GetRootComponent())};
        auto* const construction_mesh{Cast<UStaticMeshComponent>(construction->GetRootComponent())};
        auto* const energy_beam_mesh{energy_beam->FindComponentByClass<UStaticMeshComponent>()};
        auto* const scan_post_process{tactical_scan->FindComponentByClass<UPostProcessComponent>()};
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
        TestRunner->TestNotNull(TEXT("Warp field has a runtime material"),
                                IsValid(warp_mesh) ? warp_mesh->GetMaterial(0) : nullptr);
        TestRunner->TestNotNull(TEXT("Raymarched anomaly has a runtime material"),
                                IsValid(anomaly_mesh) ? anomaly_mesh->GetMaterial(0) : nullptr);
        TestRunner->TestNotNull(TEXT("Engine exhaust has a runtime material"),
                                IsValid(exhaust_mesh) ? exhaust_mesh->GetMaterial(0) : nullptr);
        TArray<UStaticMeshComponent*> planet_meshes;
        planet_atmosphere->GetComponents<UStaticMeshComponent>(planet_meshes);
        TestRunner->TestEqual(
            TEXT("Planet atmosphere owns surface and shell meshes"), planet_meshes.Num(), 2);
        UMaterialInstanceDynamic* planet_atmosphere_instance{nullptr};
        for (auto* const planet_mesh : planet_meshes) {
            auto* const candidate{IsValid(planet_mesh)
                                      ? Cast<UMaterialInstanceDynamic>(planet_mesh->GetMaterial(0))
                                      : nullptr};
            if (IsValid(candidate) &&
                candidate->K2_GetScalarParameterValue(TEXT("Density")) > 0.0f) {
                planet_atmosphere_instance = candidate;
            }
        }
        TestRunner->TestEqual(
            TEXT("Planet density reached its atmosphere shell material"),
            IsValid(planet_atmosphere_instance)
                ? planet_atmosphere_instance->K2_GetScalarParameterValue(TEXT("Density"))
                : -1.0f,
            3.25f);
        auto* const construction_instance{
            IsValid(construction_mesh)
                ? Cast<UMaterialInstanceDynamic>(construction_mesh->GetMaterial(0))
                : nullptr};
        TestRunner->TestEqual(
            TEXT("Construction progress reached its material"),
            IsValid(construction_instance)
                ? construction_instance->K2_GetScalarParameterValue(TEXT("Progress"))
                : -1.0f,
            1.0f);
        auto* const energy_beam_instance{
            IsValid(energy_beam_mesh)
                ? Cast<UMaterialInstanceDynamic>(energy_beam_mesh->GetMaterial(0))
                : nullptr};
        TestRunner->TestEqual(TEXT("Long beam preset scales cylinder to endpoint distance"),
                              IsValid(energy_beam_mesh)
                                  ? static_cast<float>(energy_beam_mesh->GetRelativeScale3D().Z)
                                  : -1.0f,
                              14.0f);
        TestRunner->TestEqual(
            TEXT("Energy beam width reached its material"),
            IsValid(energy_beam_instance)
                ? energy_beam_instance->K2_GetScalarParameterValue(TEXT("BeamWidth"))
                : -1.0f,
            energy_beam->settings.beam_width);
        TestRunner->TestTrue(
            TEXT("Distinct energy beam endpoints reached its material"),
            IsValid(energy_beam_instance) &&
                energy_beam_instance->K2_GetVectorParameterValue(TEXT("SourcePosition")) !=
                    energy_beam_instance->K2_GetVectorParameterValue(TEXT("DestinationPosition")));
        TestRunner->TestTrue(
            TEXT("Tactical scan owns a post-process blendable"),
            IsValid(scan_post_process) &&
                !scan_post_process->Settings.WeightedBlendables.Array.IsEmpty() &&
                IsValid(scan_post_process->Settings.WeightedBlendables.Array[0].Object));

        auto* const impact_instance{
            IsValid(impact_mesh) ? Cast<UMaterialInstanceDynamic>(impact_mesh->GetMaterial(0))
                                 : nullptr};
        auto* const warp_instance{IsValid(warp_mesh)
                                      ? Cast<UMaterialInstanceDynamic>(warp_mesh->GetMaterial(0))
                                      : nullptr};
        auto* const anomaly_instance{
            IsValid(anomaly_mesh) ? Cast<UMaterialInstanceDynamic>(anomaly_mesh->GetMaterial(0))
                                  : nullptr};
        auto* const exhaust_instance{
            IsValid(exhaust_mesh) ? Cast<UMaterialInstanceDynamic>(exhaust_mesh->GetMaterial(0))
                                  : nullptr};
        auto* const scan_instance{
            IsValid(scan_post_process) &&
                    !scan_post_process->Settings.WeightedBlendables.Array.IsEmpty()
                ? Cast<UMaterialInstanceDynamic>(
                      scan_post_process->Settings.WeightedBlendables.Array[0].Object)
                : nullptr};
        TestRunner->TestTrue(
            TEXT("Shield impact data reached its material"),
            IsValid(impact_instance) &&
                impact_instance->K2_GetVectorParameterValue(TEXT("ImpactState0")).A > 0.5f);
        TestRunner->TestTrue(TEXT("Tactical scan time reached its material"),
                             IsValid(scan_instance) && scan_instance->K2_GetScalarParameterValue(
                                                           TEXT("AnimationTime")) > 0.0f);
        TestRunner->TestEqual(TEXT("Warp strength reached its material"),
                              IsValid(warp_instance) ? warp_instance->K2_GetScalarParameterValue(
                                                           TEXT("DistortionStrength"))
                                                     : -1.0f,
                              warp_field->settings.distortion_strength);
        TestRunner->TestEqual(TEXT("Raymarch quality reached its material"),
                              IsValid(anomaly_instance)
                                  ? anomaly_instance->K2_GetScalarParameterValue(TEXT("StepCount"))
                                  : -1.0f,
                              64.0f);
        TestRunner->TestEqual(TEXT("Exhaust throttle reached its material"),
                              IsValid(exhaust_instance)
                                  ? exhaust_instance->K2_GetScalarParameterValue(TEXT("Throttle"))
                                  : -1.0f,
                              0.5f);
    }
};
