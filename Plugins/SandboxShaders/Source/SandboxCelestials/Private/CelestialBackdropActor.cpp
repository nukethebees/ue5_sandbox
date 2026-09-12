#include "SandboxCelestials/CelestialBackdropActor.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

DEFINE_LOG_CATEGORY_STATIC(LogCelestialBackdrop, Log, All);

namespace SandboxCelestials::Private {
inline constexpr TCHAR surface_material_path[]{
    TEXT("/SandboxShaders/CelestialBackdrop/M_CelestialSurface.M_CelestialSurface")};
inline constexpr TCHAR cloud_material_path[]{
    TEXT("/SandboxShaders/CelestialBackdrop/M_CelestialClouds.M_CelestialClouds")};
inline constexpr TCHAR atmosphere_material_path[]{
    TEXT("/SandboxShaders/CelestialBackdrop/M_CelestialAtmosphere.M_CelestialAtmosphere")};
inline FVector const default_sun_direction{0.35, -0.45, 0.82};
inline constexpr float engine_sphere_radius{50.0f};

void configure_backdrop_mesh(UStaticMeshComponent& mesh) {
    mesh.SetCollisionEnabled(ECollisionEnabled::NoCollision);
    mesh.SetGenerateOverlapEvents(false);
    mesh.SetCanEverAffectNavigation(false);
    mesh.SetCastShadow(false);
    mesh.SetReceivesDecals(false);
    mesh.CanCharacterStepUpOn = ECB_No;
    mesh.bCastDynamicShadow = false;
    mesh.bCastStaticShadow = false;
    mesh.bAffectDistanceFieldLighting = false;
    mesh.bAffectDynamicIndirectLighting = false;
    mesh.bAffectIndirectLightingWhileHidden = false;
}

FLinearColor vector_parameter(FVector const& vector) {
    return FLinearColor{static_cast<float>(vector.X),
                        static_cast<float>(vector.Y),
                        static_cast<float>(vector.Z),
                        0.0f};
}

void apply_close_approach(UMaterialInstanceDynamic& instance,
                          FCelestialBackdropSettings const& settings,
                          float const outer_radius) {
    instance.SetScalarParameterValue(TEXT("CloseApproachEnabled"),
                                     settings.close_approach.enabled ? 1.0f : 0.0f);
    instance.SetScalarParameterValue(TEXT("CloseGuardOuterRadius"), outer_radius);
    instance.SetScalarParameterValue(
        TEXT("CloseGuardRatio"),
        FMath::Clamp(settings.close_approach.minimum_camera_distance_ratio, 1.05f, 4.0f));
}

FCelestialBackdropSurfaceSettings earth_surface() {
    return FCelestialBackdropSurfaceSettings{};
}

FCelestialBackdropCloudSettings earth_clouds() {
    return FCelestialBackdropCloudSettings{};
}

FCelestialBackdropAtmosphereSettings earth_atmosphere() {
    return FCelestialBackdropAtmosphereSettings{};
}
}

ACelestialBackdropActor::ACelestialBackdropActor() {
    PrimaryActorTick.bCanEverTick = false;
    SetActorEnableCollision(false);

    root_ = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    SetRootComponent(root_);

    surface_mesh_ = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Surface"));
    surface_mesh_->SetupAttachment(root_);
    SandboxCelestials::Private::configure_backdrop_mesh(*surface_mesh_);

    cloud_mesh_ = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Clouds"));
    cloud_mesh_->SetupAttachment(root_);
    SandboxCelestials::Private::configure_backdrop_mesh(*cloud_mesh_);
    cloud_mesh_->SetTranslucentSortPriority(0);

    atmosphere_mesh_ = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Atmosphere"));
    atmosphere_mesh_->SetupAttachment(root_);
    SandboxCelestials::Private::configure_backdrop_mesh(*atmosphere_mesh_);
    atmosphere_mesh_->SetTranslucentSortPriority(1);

    static ConstructorHelpers::FObjectFinder<UStaticMesh> const sphere_mesh{
        TEXT("/Engine/BasicShapes/Sphere.Sphere")};
    if (sphere_mesh.Succeeded()) {
        surface_mesh_->SetStaticMesh(sphere_mesh.Object);
        cloud_mesh_->SetStaticMesh(sphere_mesh.Object);
        atmosphere_mesh_->SetStaticMesh(sphere_mesh.Object);
    } else {
        UE_LOG(LogCelestialBackdrop, Error, TEXT("Could not load the engine sphere mesh."));
    }

    static ConstructorHelpers::FObjectFinder<UMaterialInterface> const surface_material{
        SandboxCelestials::Private::surface_material_path};
    if (surface_material.Succeeded()) {
        surface_material_ = surface_material.Object;
        surface_mesh_->SetMaterial(0, surface_material_);
    }

    static ConstructorHelpers::FObjectFinder<UMaterialInterface> const cloud_material{
        SandboxCelestials::Private::cloud_material_path};
    if (cloud_material.Succeeded()) {
        cloud_material_ = cloud_material.Object;
        cloud_mesh_->SetMaterial(0, cloud_material_);
    }

    static ConstructorHelpers::FObjectFinder<UMaterialInterface> const atmosphere_material{
        SandboxCelestials::Private::atmosphere_material_path};
    if (atmosphere_material.Succeeded()) {
        atmosphere_material_ = atmosphere_material.Object;
        atmosphere_mesh_->SetMaterial(0, atmosphere_material_);
    }
}

void ACelestialBackdropActor::OnConstruction(FTransform const& transform) {
    Super::OnConstruction(transform);
    apply_settings();
}

void ACelestialBackdropActor::PostRegisterAllComponents() {
    Super::PostRegisterAllComponents();
    apply_settings();
}

void ACelestialBackdropActor::apply_settings() {
    ensure_materials();

    auto const radius{FMath::Max(settings.body_radius, 1.0f)};
    auto const cloud_altitude{FMath::Clamp(settings.clouds.altitude, 0.0f, 0.25f)};
    auto const atmosphere_thickness{FMath::Clamp(settings.atmosphere.thickness, 0.0f, 0.35f)};
    auto const clouds_visible{settings.clouds.enabled && settings.clouds.amount > 0.0f &&
                              settings.clouds.opacity > 0.0f};
    auto const atmosphere_visible{settings.atmosphere.enabled && atmosphere_thickness > 0.0f &&
                                  settings.atmosphere.density > 0.0f &&
                                  settings.atmosphere.limb_intensity > 0.0f};

    surface_mesh_->SetRelativeScale3D(
        FVector{radius / SandboxCelestials::Private::engine_sphere_radius});
    cloud_mesh_->SetRelativeScale3D(FVector{radius * (1.0f + cloud_altitude) /
                                            SandboxCelestials::Private::engine_sphere_radius});
    atmosphere_mesh_->SetRelativeScale3D(FVector{radius * (1.0f + atmosphere_thickness) /
                                                 SandboxCelestials::Private::engine_sphere_radius});
    cloud_mesh_->SetVisibility(clouds_visible, true);
    cloud_mesh_->SetHiddenInGame(!clouds_visible, true);
    atmosphere_mesh_->SetVisibility(atmosphere_visible, true);
    atmosphere_mesh_->SetHiddenInGame(!atmosphere_visible, true);

    auto const outer_radius_ratio{
        FMath::Max(1.0f,
                   FMath::Max(clouds_visible ? 1.0f + cloud_altitude : 1.0f,
                              atmosphere_visible ? 1.0f + atmosphere_thickness : 1.0f))};
    auto const actor_scale{GetActorScale3D().GetAbsMax()};
    auto const outer_radius{radius * outer_radius_ratio * FMath::Max(actor_scale, UE_SMALL_NUMBER)};
    auto const sun_direction{settings.sun_direction.GetSafeNormal(
        UE_SMALL_NUMBER, SandboxCelestials::Private::default_sun_direction)};
    auto const sun_parameter{SandboxCelestials::Private::vector_parameter(sun_direction)};
    auto const terminator_softness{FMath::Clamp(settings.terminator_softness, 0.001f, 1.0f)};

    if (IsValid(surface_instance_)) {
        SandboxCelestials::Private::apply_close_approach(
            *surface_instance_, settings, outer_radius);
        surface_instance_->SetVectorParameterValue(TEXT("SunDirection"), sun_parameter);
        surface_instance_->SetVectorParameterValue(TEXT("PrimaryDayColour"),
                                                   settings.surface.primary_day_colour);
        surface_instance_->SetVectorParameterValue(TEXT("SecondaryDayColour"),
                                                   settings.surface.secondary_day_colour);
        surface_instance_->SetVectorParameterValue(TEXT("NightColour"),
                                                   settings.surface.night_colour);
        surface_instance_->SetVectorParameterValue(TEXT("EmissionColour"),
                                                   settings.surface.emission_colour);
        surface_instance_->SetScalarParameterValue(
            TEXT("BodyStyle"),
            settings.surface.style == ECelestialBackdropStyle::GasGiant ? 1.0f : 0.0f);
        surface_instance_->SetScalarParameterValue(TEXT("TerminatorSoftness"), terminator_softness);
        surface_instance_->SetScalarParameterValue(
            TEXT("NightBrightness"), FMath::Clamp(settings.surface.night_brightness, 0.0f, 2.0f));
        surface_instance_->SetScalarParameterValue(
            TEXT("DetailScale"), FMath::Clamp(settings.surface.detail_scale, 0.1f, 64.0f));
        surface_instance_->SetScalarParameterValue(
            TEXT("DetailStrength"), FMath::Clamp(settings.surface.detail_strength, 0.0f, 2.0f));
        surface_instance_->SetScalarParameterValue(
            TEXT("NoiseBreakup"), FMath::Clamp(settings.surface.noise_breakup, 0.0f, 1.0f));
        surface_instance_->SetScalarParameterValue(
            TEXT("StylisationSteps"),
            static_cast<float>(FMath::Clamp(settings.surface.stylisation_steps, 0, 12)));
        surface_instance_->SetScalarParameterValue(
            TEXT("PatternSeed"), static_cast<float>(settings.surface.pattern_seed));
        surface_instance_->SetScalarParameterValue(
            TEXT("BandCount"), FMath::Clamp(settings.surface.band_count, 1.0f, 64.0f));
        surface_instance_->SetScalarParameterValue(
            TEXT("BandStrength"), FMath::Clamp(settings.surface.band_strength, 0.0f, 1.0f));
        surface_instance_->SetScalarParameterValue(
            TEXT("BandWarp"), FMath::Clamp(settings.surface.band_warp, 0.0f, 1.0f));
        surface_instance_->SetScalarParameterValue(
            TEXT("EmissionIntensity"),
            FMath::Clamp(settings.surface.emission_intensity, 0.0f, 50.0f));
        surface_instance_->SetScalarParameterValue(
            TEXT("EmissionThreshold"),
            FMath::Clamp(settings.surface.emission_threshold, 0.0f, 1.0f));
    }

    if (IsValid(cloud_instance_)) {
        SandboxCelestials::Private::apply_close_approach(*cloud_instance_, settings, outer_radius);
        cloud_instance_->SetVectorParameterValue(TEXT("SunDirection"), sun_parameter);
        cloud_instance_->SetVectorParameterValue(TEXT("CloudColour"), settings.clouds.colour);
        cloud_instance_->SetVectorParameterValue(TEXT("CloudNightColour"),
                                                 settings.clouds.night_colour);
        cloud_instance_->SetScalarParameterValue(TEXT("TerminatorSoftness"), terminator_softness);
        cloud_instance_->SetScalarParameterValue(TEXT("CloudAmount"),
                                                 FMath::Clamp(settings.clouds.amount, 0.0f, 1.0f));
        cloud_instance_->SetScalarParameterValue(TEXT("CloudOpacity"),
                                                 FMath::Clamp(settings.clouds.opacity, 0.0f, 1.0f));
        cloud_instance_->SetScalarParameterValue(
            TEXT("CloudDetailScale"), FMath::Clamp(settings.clouds.detail_scale, 0.1f, 64.0f));
        cloud_instance_->SetScalarParameterValue(TEXT("CloudBreakup"),
                                                 FMath::Clamp(settings.clouds.breakup, 0.0f, 1.0f));
        cloud_instance_->SetScalarParameterValue(TEXT("PatternSeed"),
                                                 static_cast<float>(settings.surface.pattern_seed));
        cloud_instance_->SetScalarParameterValue(TEXT("CloudRotationPhaseDegrees"),
                                                 settings.clouds.rotation_phase_degrees);
        cloud_instance_->SetScalarParameterValue(TEXT("CloudRotationSpeedDegrees"),
                                                 settings.clouds.rotation_speed_degrees_per_second);
    }

    if (IsValid(atmosphere_instance_)) {
        SandboxCelestials::Private::apply_close_approach(
            *atmosphere_instance_, settings, outer_radius);
        atmosphere_instance_->SetVectorParameterValue(TEXT("SunDirection"), sun_parameter);
        atmosphere_instance_->SetVectorParameterValue(TEXT("AtmosphereColour"),
                                                      settings.atmosphere.colour);
        atmosphere_instance_->SetScalarParameterValue(TEXT("TerminatorSoftness"),
                                                      terminator_softness);
        atmosphere_instance_->SetScalarParameterValue(
            TEXT("AtmosphereDensity"), FMath::Clamp(settings.atmosphere.density, 0.0f, 8.0f));
        atmosphere_instance_->SetScalarParameterValue(
            TEXT("LimbIntensity"), FMath::Clamp(settings.atmosphere.limb_intensity, 0.0f, 50.0f));
        atmosphere_instance_->SetScalarParameterValue(
            TEXT("LimbFalloff"), FMath::Clamp(settings.atmosphere.limb_falloff, 0.5f, 12.0f));
    }
}

void ACelestialBackdropActor::apply_earth_like_preset() {
    settings.surface = SandboxCelestials::Private::earth_surface();
    settings.clouds = SandboxCelestials::Private::earth_clouds();
    settings.atmosphere = SandboxCelestials::Private::earth_atmosphere();
    settings.terminator_softness = 0.16f;
    apply_settings();
}

void ACelestialBackdropActor::apply_hive_world_preset() {
    auto surface{FCelestialBackdropSurfaceSettings{}};
    surface.primary_day_colour = FLinearColor{0.16f, 0.035f, 0.004f, 1.0f};
    surface.secondary_day_colour = FLinearColor{0.95f, 0.42f, 0.025f, 1.0f};
    surface.night_colour = FLinearColor{0.008f, 0.002f, 0.0f, 1.0f};
    surface.detail_scale = 6.5f;
    surface.detail_strength = 1.3f;
    surface.noise_breakup = 0.65f;
    surface.stylisation_steps = 5;
    surface.pattern_seed = 622;
    surface.emission_colour = FLinearColor{1.0f, 0.22f, 0.005f, 1.0f};
    surface.emission_intensity = 7.5f;
    surface.emission_threshold = 0.72f;
    settings.surface = surface;

    auto clouds{FCelestialBackdropCloudSettings{}};
    clouds.colour = FLinearColor{0.75f, 0.19f, 0.015f, 1.0f};
    clouds.night_colour = FLinearColor{0.08f, 0.008f, 0.0f, 1.0f};
    clouds.amount = 0.22f;
    clouds.opacity = 0.34f;
    clouds.detail_scale = 9.0f;
    clouds.rotation_speed_degrees_per_second = -0.28f;
    settings.clouds = clouds;

    auto atmosphere{FCelestialBackdropAtmosphereSettings{}};
    atmosphere.colour = FLinearColor{1.0f, 0.20f, 0.015f, 1.0f};
    atmosphere.thickness = 0.065f;
    atmosphere.density = 1.75f;
    atmosphere.limb_intensity = 8.5f;
    atmosphere.limb_falloff = 3.8f;
    settings.atmosphere = atmosphere;
    settings.terminator_softness = 0.10f;
    apply_settings();
}

void ACelestialBackdropActor::apply_dark_alien_preset() {
    auto surface{FCelestialBackdropSurfaceSettings{}};
    surface.primary_day_colour = FLinearColor{0.008f, 0.006f, 0.018f, 1.0f};
    surface.secondary_day_colour = FLinearColor{0.035f, 0.012f, 0.07f, 1.0f};
    surface.night_colour = FLinearColor{0.001f, 0.0f, 0.004f, 1.0f};
    surface.night_brightness = 0.08f;
    surface.detail_scale = 8.0f;
    surface.detail_strength = 1.45f;
    surface.noise_breakup = 0.78f;
    surface.stylisation_steps = 3;
    surface.pattern_seed = 911;
    surface.emission_colour = FLinearColor{0.12f, 0.8f, 1.0f, 1.0f};
    surface.emission_intensity = 12.0f;
    surface.emission_threshold = 0.76f;
    settings.surface = surface;

    auto clouds{FCelestialBackdropCloudSettings{}};
    clouds.enabled = false;
    settings.clouds = clouds;

    auto atmosphere{FCelestialBackdropAtmosphereSettings{}};
    atmosphere.colour = FLinearColor{0.05f, 0.75f, 1.0f, 1.0f};
    atmosphere.thickness = 0.11f;
    atmosphere.density = 1.2f;
    atmosphere.limb_intensity = 14.0f;
    atmosphere.limb_falloff = 5.2f;
    settings.atmosphere = atmosphere;
    settings.terminator_softness = 0.08f;
    apply_settings();
}

void ACelestialBackdropActor::apply_gas_giant_preset() {
    auto surface{FCelestialBackdropSurfaceSettings{}};
    surface.style = ECelestialBackdropStyle::GasGiant;
    surface.primary_day_colour = FLinearColor{0.12f, 0.025f, 0.005f, 1.0f};
    surface.secondary_day_colour = FLinearColor{0.92f, 0.38f, 0.08f, 1.0f};
    surface.night_colour = FLinearColor{0.012f, 0.003f, 0.002f, 1.0f};
    surface.night_brightness = 0.28f;
    surface.detail_scale = 3.0f;
    surface.detail_strength = 1.15f;
    surface.noise_breakup = 0.32f;
    surface.pattern_seed = 404;
    surface.band_count = 15.0f;
    surface.band_strength = 0.92f;
    surface.band_warp = 0.28f;
    settings.surface = surface;

    auto clouds{FCelestialBackdropCloudSettings{}};
    clouds.enabled = false;
    settings.clouds = clouds;

    auto atmosphere{FCelestialBackdropAtmosphereSettings{}};
    atmosphere.colour = FLinearColor{1.0f, 0.35f, 0.08f, 1.0f};
    atmosphere.thickness = 0.045f;
    atmosphere.density = 1.0f;
    atmosphere.limb_intensity = 4.5f;
    atmosphere.limb_falloff = 2.6f;
    settings.atmosphere = atmosphere;
    settings.terminator_softness = 0.24f;
    apply_settings();
}

void ACelestialBackdropActor::ensure_materials() {
    if (!IsValid(surface_material_)) {
        surface_material_ = LoadObject<UMaterialInterface>(
            nullptr, SandboxCelestials::Private::surface_material_path);
    }
    if (!IsValid(cloud_material_)) {
        cloud_material_ = LoadObject<UMaterialInterface>(
            nullptr, SandboxCelestials::Private::cloud_material_path);
    }
    if (!IsValid(atmosphere_material_)) {
        atmosphere_material_ = LoadObject<UMaterialInterface>(
            nullptr, SandboxCelestials::Private::atmosphere_material_path);
    }

    if (IsValid(surface_material_) && !IsValid(surface_instance_)) {
        surface_instance_ = surface_mesh_->CreateDynamicMaterialInstance(0, surface_material_);
    }
    if (IsValid(cloud_material_) && !IsValid(cloud_instance_)) {
        cloud_instance_ = cloud_mesh_->CreateDynamicMaterialInstance(0, cloud_material_);
    }
    if (IsValid(atmosphere_material_) && !IsValid(atmosphere_instance_)) {
        atmosphere_instance_ =
            atmosphere_mesh_->CreateDynamicMaterialInstance(0, atmosphere_material_);
    }

    if (!IsValid(surface_instance_) || !IsValid(cloud_instance_) ||
        !IsValid(atmosphere_instance_)) {
        UE_LOG(LogCelestialBackdrop,
               Warning,
               TEXT("Celestial backdrop materials are unavailable or failed to instantiate."));
    }
}
