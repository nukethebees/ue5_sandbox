#include "SandboxShaders/SpaceDust/SpaceDustComponent.h"
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
constexpr TCHAR celestial_analytic_material_path[]{
    TEXT("/SandboxShaders/CelestialBackdrop/M_CelestialAnalytic.M_CelestialAnalytic")};
constexpr TCHAR construction_material_path[]{
    TEXT("/SandboxShaders/Experiments/ConstructionSpawn/M_ConstructionSpawn.M_ConstructionSpawn")};
constexpr TCHAR energy_beam_material_path[]{
    TEXT("/SandboxShaders/Experiments/EnergyBeam/M_EnergyBeam.M_EnergyBeam")};
constexpr TCHAR space_dust_material_path[]{
    TEXT("/SandboxShaders/Generated/Materials/M_SpaceDust.M_SpaceDust")};
constexpr TCHAR showcase_object_path[]{
    TEXT("/SandboxExperiments/ShaderShowcase/SandboxShaders_Showcase.SandboxShaders_Showcase")};
constexpr TCHAR showcase_package_path[]{TEXT("/SandboxExperiments/ShaderShowcase/SandboxShaders_Showcase")};

template <typename ActorType>
void find_first_actor(AActor* candidate, ActorType*& found) {
    if (found == nullptr) {
        found = Cast<ActorType>(candidate);
    }
}

UMaterialInstanceDynamic* dynamic_material(UStaticMeshComponent* mesh) {
    return IsValid(mesh) ? Cast<UMaterialInstanceDynamic>(mesh->GetMaterial(0)) : nullptr;
}

int32 count_actor_type(UWorld const& world, UClass* actor_class) {
    auto const level{world.PersistentLevel};
    if (!IsValid(level)) {
        return 0;
    }

    int32 count{0};
    for (AActor const* const actor : level->Actors) {
        if (IsValid(actor) && actor->IsA(actor_class)) {
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
    }

    TEST_METHOD(LoadsMaterialsAndComposedShowcase)
    {
        struct FAssetCase {
            TCHAR const* description;
            UObject* asset;
        };
        FAssetCase const assets[]{
            {TEXT("Energy shield material loads"), LoadObject<UMaterial>(nullptr, shield_material_path)},
            {TEXT("Space field display material loads"), LoadObject<UMaterial>(nullptr, display_material_path)},
            {TEXT("Shield impact material loads"), LoadObject<UMaterial>(nullptr, impact_material_path)},
            {TEXT("Vertex ripple material loads"), LoadObject<UMaterial>(nullptr, ripple_material_path)},
            {TEXT("Vertex ripple grid loads"), LoadObject<UStaticMesh>(nullptr, ripple_mesh_path)},
            {TEXT("Radar display material loads"), LoadObject<UMaterial>(nullptr, radar_material_path)},
            {TEXT("Tactical scan material loads"), LoadObject<UMaterial>(nullptr, tactical_scan_material_path)},
            {TEXT("Warp field material loads"), LoadObject<UMaterial>(nullptr, warp_field_material_path)},
            {TEXT("Warp backdrop material loads"), LoadObject<UMaterial>(nullptr, warp_backdrop_material_path)},
            {TEXT("Raymarched anomaly material loads"), LoadObject<UMaterial>(nullptr, anomaly_material_path)},
            {TEXT("Engine exhaust material loads"), LoadObject<UMaterial>(nullptr, exhaust_material_path)},
            {TEXT("Planet surface material loads"), LoadObject<UMaterial>(nullptr, planet_surface_material_path)},
            {TEXT("Planet atmosphere material loads"), LoadObject<UMaterial>(nullptr, planet_atmosphere_material_path)},
            {TEXT("Construction spawn material loads"), LoadObject<UMaterial>(nullptr, construction_material_path)},
            {TEXT("Energy beam material loads"), LoadObject<UMaterial>(nullptr, energy_beam_material_path)},
        };
        for (auto const& asset : assets) {
            TestRunner->TestNotNull(asset.description, asset.asset);
        }

        auto* const world{LoadObject<UWorld>(nullptr, showcase_object_path)};
        if (!TestRunner->TestNotNull(TEXT("Showcase map loads"), world)) {
            return;
        }
        struct FActorCase {
            TCHAR const* description;
            UClass* actor_class;
        };
        FActorCase const actors[]{
            {TEXT("Showcase contains one shield experiment"), AEnergyShieldExperimentActor::StaticClass()},
            {TEXT("Showcase contains one space field experiment"), ASpaceEnergyFieldExperimentActor::StaticClass()},
            {TEXT("Showcase contains one shield impact experiment"), AShieldImpactExperimentActor::StaticClass()},
            {TEXT("Showcase contains one vertex ripple experiment"), AVertexRippleExperimentActor::StaticClass()},
            {TEXT("Showcase contains one radar display experiment"), ARadarDisplayExperimentActor::StaticClass()},
            {TEXT("Showcase contains one tactical scan experiment"), ATacticalScanExperimentActor::StaticClass()},
            {TEXT("Showcase contains one warp field experiment"), AWarpFieldExperimentActor::StaticClass()},
            {TEXT("Showcase contains one raymarched anomaly experiment"), ARaymarchedAnomalyExperimentActor::StaticClass()},
            {TEXT("Showcase contains one engine exhaust experiment"), AEngineExhaustExperimentActor::StaticClass()},
            {TEXT("Showcase contains one planet atmosphere experiment"), APlanetAtmosphereExperimentActor::StaticClass()},
            {TEXT("Showcase contains one construction spawn experiment"), AConstructionSpawnExperimentActor::StaticClass()},
            {TEXT("Showcase contains one energy beam experiment"), AEnergyBeamExperimentActor::StaticClass()},
            {TEXT("Showcase contains one camera"), ACameraActor::StaticClass()},
        };
        for (auto const& actor : actors) {
            TestRunner->TestEqual(actor.description, count_actor_type(*world, actor.actor_class), 1);
        }
    }

    TEST_METHOD(UpdatesSpaceDustBoundsWhenVolumeChanges)
    {
        auto* const component{NewObject<USpaceDustComponent>()};
        component->UpdateBounds();

        auto settings{component->get_settings()};
        settings.volume_dimensions = FVector{1000.0, 2000.0, 3000.0};
        component->apply_settings(settings);

        TestRunner->TestTrue(
            TEXT("Updated volume changes component bounds"),
            component->GetBounds().BoxExtent.Equals(FVector{500.0, 1000.0, 1500.0}));

        auto const rotated_transform{
            FTransform{FRotator{0.0, 90.0, 0.0}, FVector{125.0, -250.0, 300.0}}};
        auto const rotated_bounds{component->CalcBounds(rotated_transform)};
        TestRunner->TestTrue(TEXT("Dust bounds remain centred on the camera"),
                             rotated_bounds.Origin.Equals(rotated_transform.GetLocation()));
        TestRunner->TestTrue(TEXT("Dust volume remains world-axis aligned"),
                             rotated_bounds.BoxExtent.Equals(FVector{500.0, 1000.0, 1500.0}));
    }

    TEST_METHOD(IgnoresSpaceDustColourAlpha)
    {
        FSpaceDustSettings settings;
        settings.colour.A = 0.25f;

        auto const normalised{normalise_space_dust_settings(settings)};

        TestRunner->TestEqual(TEXT("Colour alpha is canonical opaque"), normalised.colour.A, 1.0f);
    }
};

TEST_CLASS(MaterialCompilation, "SandboxShaders.MaterialCompilation")
{
    TEST_METHOD(CompilesMaterialsAndDispatchesSpaceField)
    {
        if (!TestRunner->TestFalse(TEXT("Material compilation test uses a real RHI"),
                                   GUsingNullRHI)) {
            return;
        }

        struct FMaterialCase {
            TCHAR const* load_description;
            TCHAR const* compile_description;
            UMaterial* material;
        };
        FMaterialCase const materials[]{
            {TEXT("Energy shield material loads"),
             TEXT("Energy shield shader compiles without errors"),
             LoadObject<UMaterial>(nullptr, shield_material_path)},
            {TEXT("Space field material loads"),
             TEXT("Space field display shader compiles without errors"),
             LoadObject<UMaterial>(nullptr, display_material_path)},
            {TEXT("Shield impact material loads"),
             TEXT("Shield impact shader compiles without errors"),
             LoadObject<UMaterial>(nullptr, impact_material_path)},
            {TEXT("Vertex ripple material loads"),
             TEXT("Vertex ripple shader compiles without errors"),
             LoadObject<UMaterial>(nullptr, ripple_material_path)},
            {TEXT("Radar display material loads"),
             TEXT("Radar display shader compiles without errors"),
             LoadObject<UMaterial>(nullptr, radar_material_path)},
            {TEXT("Tactical scan material loads"),
             TEXT("Tactical scan shader compiles without errors"),
             LoadObject<UMaterial>(nullptr, tactical_scan_material_path)},
            {TEXT("Warp field material loads"),
             TEXT("Warp field shader compiles without errors"),
             LoadObject<UMaterial>(nullptr, warp_field_material_path)},
            {TEXT("Raymarched anomaly material loads"),
             TEXT("Raymarched anomaly shader compiles without errors"),
             LoadObject<UMaterial>(nullptr, anomaly_material_path)},
            {TEXT("Engine exhaust material loads"),
             TEXT("Engine exhaust shader compiles without errors"),
             LoadObject<UMaterial>(nullptr, exhaust_material_path)},
            {TEXT("Planet surface material loads"),
             TEXT("Planet surface shader compiles without errors"),
             LoadObject<UMaterial>(nullptr, planet_surface_material_path)},
            {TEXT("Planet atmosphere material loads"),
             TEXT("Planet atmosphere shader compiles without errors"),
             LoadObject<UMaterial>(nullptr, planet_atmosphere_material_path)},
            {TEXT("Celestial analytic material loads"),
             TEXT("Celestial analytic shader compiles without errors"),
             LoadObject<UMaterial>(nullptr, celestial_analytic_material_path)},
            {TEXT("Construction spawn material loads"),
             TEXT("Construction spawn shader compiles without errors"),
             LoadObject<UMaterial>(nullptr, construction_material_path)},
            {TEXT("Energy beam material loads"),
             TEXT("Energy beam shader compiles without errors"),
             LoadObject<UMaterial>(nullptr, energy_beam_material_path)},
            {TEXT("Space dust material loads"),
             TEXT("Space dust shader compiles without errors"),
             LoadObject<UMaterial>(nullptr, space_dust_material_path)},
        };
        for (auto const& material : materials) {
            if (!TestRunner->TestNotNull(material.load_description, material.material)) {
                return;
            }
        }
        for (auto const& material : materials) {
            TestRunner->TestTrue(material.compile_description,
                                 UMaterialEditingLibrary::RecompileMaterial(material.material).IsEmpty());
        }

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
            find_first_actor(*actor_iterator, space_field);
            find_first_actor(*actor_iterator, shield);
            find_first_actor(*actor_iterator, impact);
            find_first_actor(*actor_iterator, ripple);
            find_first_actor(*actor_iterator, radar);
            find_first_actor(*actor_iterator, tactical_scan);
            find_first_actor(*actor_iterator, warp_field);
            find_first_actor(*actor_iterator, anomaly);
            find_first_actor(*actor_iterator, exhaust);
            find_first_actor(*actor_iterator, planet_atmosphere);
            find_first_actor(*actor_iterator, construction);
            find_first_actor(*actor_iterator, energy_beam);
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
            auto* const candidate{dynamic_material(planet_mesh)};
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
            dynamic_material(construction_mesh)};
        TestRunner->TestEqual(
            TEXT("Construction progress reached its material"),
            IsValid(construction_instance)
                ? construction_instance->K2_GetScalarParameterValue(TEXT("Progress"))
                : -1.0f,
            1.0f);
        auto* const energy_beam_instance{
            dynamic_material(energy_beam_mesh)};
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
            dynamic_material(impact_mesh)};
        auto* const warp_instance{dynamic_material(warp_mesh)};
        auto* const anomaly_instance{
            dynamic_material(anomaly_mesh)};
        auto* const exhaust_instance{
            dynamic_material(exhaust_mesh)};
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
