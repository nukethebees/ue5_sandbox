#include "SbxShadersExperiments/ShieldImpact/ShieldImpactExperimentActor.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

DEFINE_LOG_CATEGORY_STATIC(LogShieldImpactExperiment, Log, All);

namespace ml::shaders::shield_impact {
constexpr TCHAR material_path[]{
    TEXT("/SandboxShaders/Experiments/ShieldImpact/M_ShieldImpact.M_ShieldImpact")};
}

AShieldImpactExperimentActor::AShieldImpactExperimentActor() {
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = true;

    shield_mesh_ = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ShieldMesh"));
    SetRootComponent(shield_mesh_);
    shield_mesh_->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    shield_mesh_->SetCastShadow(false);
    shield_mesh_->SetRelativeScale3D(FVector{3.0});

    static ConstructorHelpers::FObjectFinder<UStaticMesh> const sphere_mesh{
        TEXT("/Engine/BasicShapes/Sphere.Sphere")};
    if (sphere_mesh.Succeeded()) {
        shield_mesh_->SetStaticMesh(sphere_mesh.Object);
    } else {
        UE_LOG(LogShieldImpactExperiment, Error, TEXT("Could not load the engine sphere mesh."));
    }

    static ConstructorHelpers::FObjectFinder<UMaterialInterface> const impact_material{
        ml::shaders::shield_impact::material_path};
    if (impact_material.Succeeded()) {
        shield_material_ = impact_material.Object;
        shield_mesh_->SetMaterial(0, shield_material_);
    }
}

void AShieldImpactExperimentActor::OnConstruction(FTransform const& transform) {
    Super::OnConstruction(transform);
    ensure_material();
    apply_settings();
}

void AShieldImpactExperimentActor::Tick(float const delta_seconds) {
    Super::Tick(delta_seconds);

    ensure_material();
    if (!IsValid(material_instance_)) {
        return;
    }

    auto const pulse_duration{FMath::Max(settings.pulse_duration, 0.1f)};
    if (impact_active_) {
        elapsed_since_impact_ += FMath::Max(delta_seconds, 0.0f);
        if (elapsed_since_impact_ <= pulse_duration) {
            apply_impact_phase(elapsed_since_impact_ / pulse_duration);
        } else if (settings.auto_repeat &&
                   elapsed_since_impact_ >=
                       pulse_duration + FMath::Max(settings.repeat_delay, 0.0f)) {
            trigger_impact();
        } else {
            apply_impact_phase(2.0f);
            impact_active_ = settings.auto_repeat;
        }
    }
}

bool AShieldImpactExperimentActor::ShouldTickIfViewportsOnly() const {
    return true;
}

void AShieldImpactExperimentActor::trigger_impact() {
    elapsed_since_impact_ = 0.0f;
    impact_active_ = true;
    apply_settings();
    apply_impact_phase(0.0f);
}

void AShieldImpactExperimentActor::trigger_impact_at_local_position(
    FVector const local_impact_centre) {
    settings.impact_centre_local = local_impact_centre;
    trigger_impact();
}

void AShieldImpactExperimentActor::ensure_material() {
    if (!IsValid(shield_material_)) {
        shield_material_ =
            LoadObject<UMaterialInterface>(nullptr, ml::shaders::shield_impact::material_path);
    }
    if (!IsValid(shield_material_)) {
        return;
    }
    if (!IsValid(material_instance_)) {
        material_instance_ = shield_mesh_->CreateDynamicMaterialInstance(0, shield_material_);
        if (!IsValid(material_instance_)) {
            UE_LOG(LogShieldImpactExperiment,
                   Error,
                   TEXT("Could not create the shield-impact material instance."));
        }
    }
}

void AShieldImpactExperimentActor::apply_settings() {
    if (!IsValid(material_instance_)) {
        return;
    }

    auto const impact_direction{
        settings.impact_centre_local.GetSafeNormal(UE_SMALL_NUMBER, FVector{-1.0, 0.0, 0.0})};
    material_instance_->SetVectorParameterValue(TEXT("BaseColour"), settings.base_colour);
    material_instance_->SetVectorParameterValue(TEXT("ImpactColour"), settings.impact_colour);
    material_instance_->SetVectorParameterValue(TEXT("ImpactCentre"),
                                                FLinearColor{impact_direction});
    material_instance_->SetScalarParameterValue(TEXT("PulseWidth"),
                                                FMath::Clamp(settings.pulse_width, 0.005f, 0.5f));
    material_instance_->SetScalarParameterValue(TEXT("ImpactIntensity"),
                                                FMath::Max(settings.impact_intensity, 0.0f));
    material_instance_->SetScalarParameterValue(TEXT("FresnelPower"),
                                                FMath::Max(settings.fresnel_power, 0.1f));
    material_instance_->SetScalarParameterValue(TEXT("NoiseBreakup"),
                                                FMath::Clamp(settings.noise_breakup, 0.0f, 1.0f));
    material_instance_->SetScalarParameterValue(TEXT("ShieldOpacity"),
                                                FMath::Clamp(settings.opacity, 0.0f, 1.0f));
}

void AShieldImpactExperimentActor::apply_impact_phase(float const phase) {
    if (IsValid(material_instance_)) {
        material_instance_->SetScalarParameterValue(TEXT("ImpactPhase"), phase);
    }
}
