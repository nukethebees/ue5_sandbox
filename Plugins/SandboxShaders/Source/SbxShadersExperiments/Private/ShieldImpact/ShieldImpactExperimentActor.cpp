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
    if (active_impacts_.IsEmpty()) {
        add_impact(
            settings.impact_centre_local, settings.preview_radius, settings.preview_strength);
    } else {
        apply_impacts();
    }
}

void AShieldImpactExperimentActor::Tick(float const delta_seconds) {
    Super::Tick(delta_seconds);

    ensure_material();
    if (!IsValid(material_instance_)) {
        return;
    }

    auto const safe_delta_seconds{FMath::Max(delta_seconds, 0.0f)};
    for (auto& impact : active_impacts_) {
        impact.age += safe_delta_seconds;
    }
    auto const duration{FMath::Max(settings.pulse_duration, 0.1f)};
    active_impacts_.RemoveAllSwap(
        [duration](FActiveImpact const& impact) { return impact.age > duration; },
        EAllowShrinking::No);

    auto_repeat_elapsed_ += safe_delta_seconds;
    auto const repeat_delay{FMath::Max(settings.repeat_delay, 0.05f)};
    if (settings.auto_repeat && auto_repeat_elapsed_ >= repeat_delay) {
        auto const angle{static_cast<float>(auto_repeat_sequence_) * 2.3999632f};
        FVector const centre{-0.75, FMath::Cos(angle) * 0.62, FMath::Sin(angle) * 0.62};
        ++auto_repeat_sequence_;
        add_impact(centre, settings.preview_radius, settings.preview_strength);
    }
    apply_impacts();
}

bool AShieldImpactExperimentActor::ShouldTickIfViewportsOnly() const {
    return true;
}

void AShieldImpactExperimentActor::trigger_impact() {
    add_impact(settings.impact_centre_local, settings.preview_radius, settings.preview_strength);
}

void AShieldImpactExperimentActor::trigger_impact_at_local_position(
    FVector const local_impact_centre) {
    settings.impact_centre_local = local_impact_centre;
    trigger_impact();
}

bool AShieldImpactExperimentActor::add_impact(FVector const local_impact_centre,
                                              float const radius,
                                              float const strength) {
    auto const centre{local_impact_centre.GetSafeNormal(UE_SMALL_NUMBER, FVector{-1.0, 0.0, 0.0})};
    FActiveImpact const impact{
        centre, FMath::Clamp(radius, 0.005f, 0.5f), 0.0f, FMath::Max(strength, 0.0f)};
    constexpr int32 maximum_impact_count{4};
    if (active_impacts_.Num() < maximum_impact_count) {
        active_impacts_.Add(impact);
    } else {
        auto oldest_index{0};
        for (int32 index{1}; index < maximum_impact_count; ++index) {
            if (active_impacts_[index].age > active_impacts_[oldest_index].age) {
                oldest_index = index;
            }
        }
        active_impacts_[oldest_index] = impact;
    }

    auto_repeat_elapsed_ = 0.0f;
    ensure_material();
    apply_settings();
    apply_impacts();
    return true;
}

void AShieldImpactExperimentActor::clear_impacts() {
    active_impacts_.Reset();
    auto_repeat_elapsed_ = 0.0f;
    apply_impacts();
}

int32 AShieldImpactExperimentActor::active_impact_count() const {
    return active_impacts_.Num();
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
    material_instance_->SetScalarParameterValue(TEXT("ImpactDuration"),
                                                FMath::Max(settings.pulse_duration, 0.1f));
    material_instance_->SetScalarParameterValue(TEXT("ExpansionSpeed"),
                                                FMath::Max(settings.expansion_speed, 0.0f));
    material_instance_->SetScalarParameterValue(TEXT("ImpactIntensity"),
                                                FMath::Max(settings.impact_intensity, 0.0f));
    material_instance_->SetScalarParameterValue(TEXT("FresnelPower"),
                                                FMath::Max(settings.fresnel_power, 0.1f));
    material_instance_->SetScalarParameterValue(TEXT("NoiseBreakup"),
                                                FMath::Clamp(settings.noise_breakup, 0.0f, 1.0f));
    material_instance_->SetScalarParameterValue(TEXT("ShieldOpacity"),
                                                FMath::Clamp(settings.opacity, 0.0f, 1.0f));
}

void AShieldImpactExperimentActor::apply_impacts() {
    if (!IsValid(material_instance_)) {
        return;
    }

    constexpr int32 maximum_impact_count{4};
    for (int32 index{0}; index < maximum_impact_count; ++index) {
        FLinearColor impact_data{0.0f, 0.0f, 1.0f, 0.0f};
        FLinearColor impact_state{0.0f, 0.0f, 0.0f, 0.0f};
        if (active_impacts_.IsValidIndex(index)) {
            auto const& impact{active_impacts_[index]};
            impact_data = FLinearColor{static_cast<float>(impact.centre.X),
                                       static_cast<float>(impact.centre.Y),
                                       static_cast<float>(impact.centre.Z),
                                       impact.radius};
            impact_state = FLinearColor{impact.age, impact.strength, 0.0f, 1.0f};
        }
        material_instance_->SetVectorParameterValue(*FString::Printf(TEXT("ImpactData%d"), index),
                                                    impact_data);
        material_instance_->SetVectorParameterValue(*FString::Printf(TEXT("ImpactState%d"), index),
                                                    impact_state);
    }
}
