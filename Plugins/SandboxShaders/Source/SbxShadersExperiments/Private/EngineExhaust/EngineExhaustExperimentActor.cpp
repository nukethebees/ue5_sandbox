#include "SbxShadersExperiments/EngineExhaust/EngineExhaustExperimentActor.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

namespace {
constexpr TCHAR engine_exhaust_material_path[]{
    TEXT("/SandboxShaders/Experiments/EngineExhaust/M_EngineExhaust.M_EngineExhaust")};
}

AEngineExhaustExperimentActor::AEngineExhaustExperimentActor() {
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = true;

    exhaust_mesh_ = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ExhaustMesh"));
    SetRootComponent(exhaust_mesh_);
    exhaust_mesh_->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    exhaust_mesh_->SetCastShadow(false);

    static ConstructorHelpers::FObjectFinder<UStaticMesh> const cone_mesh{
        TEXT("/Engine/BasicShapes/Cone.Cone")};
    if (cone_mesh.Succeeded()) {
        exhaust_mesh_->SetStaticMesh(cone_mesh.Object);
    }
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> const exhaust_material{
        engine_exhaust_material_path};
    if (exhaust_material.Succeeded()) {
        exhaust_material_ = exhaust_material.Object;
        exhaust_mesh_->SetMaterial(0, exhaust_material_);
    }
}

void AEngineExhaustExperimentActor::OnConstruction(FTransform const& transform) {
    Super::OnConstruction(transform);
    ensure_material();
    apply_settings();
}

void AEngineExhaustExperimentActor::Tick(float const delta_seconds) {
    Super::Tick(delta_seconds);
    if (!settings.animation_paused) {
        animation_time_ += FMath::Max(delta_seconds, 0.0f);
    }
    ensure_material();
    apply_settings();
}

bool AEngineExhaustExperimentActor::ShouldTickIfViewportsOnly() const {
    return true;
}

void AEngineExhaustExperimentActor::set_idle() {
    settings.throttle = 0.0f;
    apply_settings();
}

void AEngineExhaustExperimentActor::set_half_throttle() {
    settings.throttle = 0.5f;
    apply_settings();
}

void AEngineExhaustExperimentActor::set_full_throttle() {
    settings.throttle = 1.0f;
    apply_settings();
}

void AEngineExhaustExperimentActor::ensure_material() {
    if (!IsValid(exhaust_material_)) {
        exhaust_material_ = LoadObject<UMaterialInterface>(nullptr, engine_exhaust_material_path);
    }
    if (IsValid(exhaust_material_) && !IsValid(material_instance_)) {
        material_instance_ = exhaust_mesh_->CreateDynamicMaterialInstance(0, exhaust_material_);
    }
}

void AEngineExhaustExperimentActor::apply_settings() {
    auto const throttle{FMath::Clamp(settings.throttle, 0.0f, 1.0f)};
    auto const visible_throttle{FMath::Lerp(0.08f, 1.0f, throttle)};
    auto const display_scale{FMath::Max(settings.display_scale, 0.01f)};
    exhaust_mesh_->SetRelativeScale3D(
        FVector{settings.radius / 50.0f * visible_throttle * display_scale,
                settings.radius / 50.0f * visible_throttle * display_scale,
                settings.exhaust_length / 100.0f * visible_throttle * display_scale});
    exhaust_mesh_->SetVisibility(throttle > UE_SMALL_NUMBER);
    if (!IsValid(material_instance_)) {
        return;
    }

    material_instance_->SetVectorParameterValue(TEXT("CoreColour"), settings.core_colour);
    material_instance_->SetVectorParameterValue(TEXT("TailColour"), settings.tail_colour);
    material_instance_->SetScalarParameterValue(TEXT("AnimationTime"), animation_time_);
    material_instance_->SetScalarParameterValue(TEXT("Throttle"), throttle);
    material_instance_->SetScalarParameterValue(TEXT("ThrustIntensity"),
                                                FMath::Max(settings.thrust_intensity, 0.0f));
    material_instance_->SetScalarParameterValue(TEXT("FlowSpeed"),
                                                FMath::Max(settings.flow_speed, 0.0f));
    material_instance_->SetScalarParameterValue(TEXT("Turbulence"),
                                                FMath::Clamp(settings.turbulence, 0.0f, 2.0f));
    material_instance_->SetScalarParameterValue(TEXT("EmissiveIntensity"),
                                                FMath::Max(settings.emissive_intensity, 0.0f));
}
