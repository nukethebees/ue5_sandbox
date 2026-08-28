#include "SbxShadersExperiments/RaymarchedAnomaly/RaymarchedAnomalyExperimentActor.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

namespace {
constexpr TCHAR raymarched_anomaly_material_path[]{
    TEXT("/SandboxShaders/Experiments/RaymarchedAnomaly/M_RaymarchedAnomaly.M_RaymarchedAnomaly")};
}

ARaymarchedAnomalyExperimentActor::ARaymarchedAnomalyExperimentActor() {
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = true;

    display_mesh_ = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("AnomalyDisplay"));
    SetRootComponent(display_mesh_);
    display_mesh_->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    display_mesh_->SetCastShadow(false);
    display_mesh_->SetRelativeScale3D(FVector{7.0, 7.0, 1.0});

    static ConstructorHelpers::FObjectFinder<UStaticMesh> const plane_mesh{
        TEXT("/Engine/BasicShapes/Plane.Plane")};
    if (plane_mesh.Succeeded()) {
        display_mesh_->SetStaticMesh(plane_mesh.Object);
    }
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> const display_material{
        raymarched_anomaly_material_path};
    if (display_material.Succeeded()) {
        display_material_ = display_material.Object;
        display_mesh_->SetMaterial(0, display_material_);
    }
}

void ARaymarchedAnomalyExperimentActor::OnConstruction(FTransform const& transform) {
    Super::OnConstruction(transform);
    ensure_material();
    apply_settings();
}

void ARaymarchedAnomalyExperimentActor::Tick(float const delta_seconds) {
    Super::Tick(delta_seconds);
    if (!settings.animation_paused) {
        animation_time_ += FMath::Max(delta_seconds, 0.0f);
    }
    ensure_material();
    apply_settings();
}

bool ARaymarchedAnomalyExperimentActor::ShouldTickIfViewportsOnly() const {
    return true;
}

void ARaymarchedAnomalyExperimentActor::restart_animation() {
    animation_time_ = 0.0f;
    apply_settings();
}

void ARaymarchedAnomalyExperimentActor::ensure_material() {
    if (!IsValid(display_material_)) {
        display_material_ =
            LoadObject<UMaterialInterface>(nullptr, raymarched_anomaly_material_path);
    }
    if (IsValid(display_material_) && !IsValid(material_instance_)) {
        material_instance_ = display_mesh_->CreateDynamicMaterialInstance(0, display_material_);
    }
}

void ARaymarchedAnomalyExperimentActor::apply_settings() {
    if (!IsValid(material_instance_)) {
        return;
    }
    material_instance_->SetVectorParameterValue(TEXT("ColourA"), settings.colour_a);
    material_instance_->SetVectorParameterValue(TEXT("ColourB"), settings.colour_b);
    material_instance_->SetScalarParameterValue(TEXT("AnimationTime"), animation_time_);
    material_instance_->SetScalarParameterValue(
        TEXT("StepCount"), static_cast<float>(FMath::Clamp(settings.step_count, 8, 96)));
    material_instance_->SetScalarParameterValue(TEXT("AnomalyScale"),
                                                FMath::Max(settings.scale, 0.1f));
    material_instance_->SetScalarParameterValue(TEXT("Deformation"),
                                                FMath::Clamp(settings.deformation, 0.0f, 1.5f));
    material_instance_->SetScalarParameterValue(TEXT("AnimationSpeed"),
                                                FMath::Max(settings.animation_speed, 0.0f));
    material_instance_->SetScalarParameterValue(TEXT("NoiseFrequency"),
                                                FMath::Max(settings.noise_frequency, 0.1f));
    material_instance_->SetScalarParameterValue(TEXT("EmissiveStrength"),
                                                FMath::Max(settings.emissive_strength, 0.0f));
    material_instance_->SetScalarParameterValue(TEXT("MaxDistance"),
                                                FMath::Clamp(settings.max_distance, 1.0f, 20.0f));
}
