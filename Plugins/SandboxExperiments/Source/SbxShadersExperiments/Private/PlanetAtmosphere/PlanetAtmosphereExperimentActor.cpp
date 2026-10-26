#include "SbxShadersExperiments/PlanetAtmosphere/PlanetAtmosphereExperimentActor.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

DEFINE_LOG_CATEGORY_STATIC(LogPlanetAtmosphereExperiment, Log, All);

namespace {
constexpr TCHAR surface_material_path[]{
    TEXT("/SandboxShaders/Experiments/PlanetAtmosphere/M_PlanetSurface.M_PlanetSurface")};
constexpr TCHAR atmosphere_material_path[]{
    TEXT("/SandboxShaders/Experiments/PlanetAtmosphere/M_PlanetAtmosphere.M_PlanetAtmosphere")};
FVector const default_sun_direction{0.35, -0.45, 0.82};
}

APlanetAtmosphereExperimentActor::APlanetAtmosphereExperimentActor() {
    PrimaryActorTick.bCanEverTick = false;

    root_ = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    SetRootComponent(root_);

    surface_mesh_ = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PlanetSurface"));
    surface_mesh_->SetupAttachment(root_);
    surface_mesh_->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    surface_mesh_->SetRelativeScale3D(FVector{3.5});

    atmosphere_mesh_ = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("AtmosphereShell"));
    atmosphere_mesh_->SetupAttachment(root_);
    atmosphere_mesh_->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    atmosphere_mesh_->SetCastShadow(false);
    atmosphere_mesh_->SetRelativeScale3D(FVector{3.82});

    static ConstructorHelpers::FObjectFinder<UStaticMesh> const sphere_mesh{
        TEXT("/Engine/BasicShapes/Sphere.Sphere")};
    if (sphere_mesh.Succeeded()) {
        surface_mesh_->SetStaticMesh(sphere_mesh.Object);
        atmosphere_mesh_->SetStaticMesh(sphere_mesh.Object);
    } else {
        UE_LOG(
            LogPlanetAtmosphereExperiment, Error, TEXT("Could not load the engine sphere mesh."));
    }

    static ConstructorHelpers::FObjectFinder<UMaterialInterface> const surface_material{
        surface_material_path};
    if (surface_material.Succeeded()) {
        surface_material_ = surface_material.Object;
        surface_mesh_->SetMaterial(0, surface_material_);
    }
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> const atmosphere_material{
        atmosphere_material_path};
    if (atmosphere_material.Succeeded()) {
        atmosphere_material_ = atmosphere_material.Object;
        atmosphere_mesh_->SetMaterial(0, atmosphere_material_);
    }
}

void APlanetAtmosphereExperimentActor::OnConstruction(FTransform const& transform) {
    Super::OnConstruction(transform);
    ensure_materials();
    apply_settings();
}

void APlanetAtmosphereExperimentActor::reset_sun_direction() {
    settings.sun_direction = default_sun_direction;
    apply_settings();
}

void APlanetAtmosphereExperimentActor::ensure_materials() {
    if (!IsValid(surface_material_)) {
        surface_material_ = LoadObject<UMaterialInterface>(nullptr, surface_material_path);
    }
    if (!IsValid(atmosphere_material_)) {
        atmosphere_material_ = LoadObject<UMaterialInterface>(nullptr, atmosphere_material_path);
    }
    if (IsValid(surface_material_) && !IsValid(surface_instance_)) {
        surface_instance_ = surface_mesh_->CreateDynamicMaterialInstance(0, surface_material_);
    }
    if (IsValid(atmosphere_material_) && !IsValid(atmosphere_instance_)) {
        atmosphere_instance_ =
            atmosphere_mesh_->CreateDynamicMaterialInstance(0, atmosphere_material_);
    }
    if (!IsValid(surface_instance_) || !IsValid(atmosphere_instance_)) {
        UE_LOG(LogPlanetAtmosphereExperiment,
               Warning,
               TEXT("Planet atmosphere materials are unavailable; regenerate showcase assets."));
    }
}

void APlanetAtmosphereExperimentActor::apply_settings() {
    auto const sun_direction{
        settings.sun_direction.GetSafeNormal(UE_SMALL_NUMBER, default_sun_direction)};
    if (IsValid(surface_instance_)) {
        surface_instance_->SetVectorParameterValue(TEXT("SunDirection"), sun_direction);
        surface_instance_->SetVectorParameterValue(TEXT("DayColour"), settings.surface_day_colour);
        surface_instance_->SetVectorParameterValue(TEXT("NightColour"),
                                                   settings.surface_night_colour);
        surface_instance_->SetScalarParameterValue(TEXT("NightFloor"),
                                                   FMath::Clamp(settings.night_floor, 0.0f, 1.0f));
    }
    if (IsValid(atmosphere_instance_)) {
        atmosphere_instance_->SetVectorParameterValue(TEXT("SunDirection"), sun_direction);
        atmosphere_instance_->SetVectorParameterValue(TEXT("AtmosphereColour"),
                                                      settings.atmosphere_colour);
        atmosphere_instance_->SetScalarParameterValue(TEXT("Density"),
                                                      FMath::Clamp(settings.density, 0.01f, 8.0f));
        atmosphere_instance_->SetScalarParameterValue(
            TEXT("LimbPower"), FMath::Clamp(settings.limb_power, 0.5f, 12.0f));
        atmosphere_instance_->SetScalarParameterValue(
            TEXT("ScaleHeight"), FMath::Clamp(settings.scale_height, 0.01f, 1.0f));
        atmosphere_instance_->SetScalarParameterValue(
            TEXT("EmissiveIntensity"), FMath::Max(settings.emissive_intensity, 0.0f));
    }
}
