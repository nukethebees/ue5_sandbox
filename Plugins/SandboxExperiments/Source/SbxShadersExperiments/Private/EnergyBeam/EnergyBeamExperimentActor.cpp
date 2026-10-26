#include "SbxShadersExperiments/EnergyBeam/EnergyBeamExperimentActor.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

DEFINE_LOG_CATEGORY_STATIC(LogEnergyBeamExperiment, Log, All);

namespace {
constexpr TCHAR beam_material_path[]{
    TEXT("/SandboxShaders/Experiments/EnergyBeam/M_EnergyBeam.M_EnergyBeam")};

FLinearColor as_parameter(FVector const value) {
    return FLinearColor{static_cast<float>(value.X),
                        static_cast<float>(value.Y),
                        static_cast<float>(value.Z),
                        0.0f};
}
}

AEnergyBeamExperimentActor::AEnergyBeamExperimentActor() {
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = true;

    root_ = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    SetRootComponent(root_);

    beam_mesh_ = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BeamMesh"));
    beam_mesh_->SetupAttachment(root_);
    beam_mesh_->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    beam_mesh_->SetCastShadow(false);

    static ConstructorHelpers::FObjectFinder<UStaticMesh> const cylinder_mesh{
        TEXT("/Engine/BasicShapes/Cylinder.Cylinder")};
    if (cylinder_mesh.Succeeded()) {
        beam_mesh_->SetStaticMesh(cylinder_mesh.Object);
    } else {
        UE_LOG(LogEnergyBeamExperiment, Error, TEXT("Could not load the engine cylinder mesh."));
    }

    static ConstructorHelpers::FObjectFinder<UMaterialInterface> const beam_material{
        beam_material_path};
    if (beam_material.Succeeded()) {
        beam_material_ = beam_material.Object;
        beam_mesh_->SetMaterial(0, beam_material_);
    }
}

void AEnergyBeamExperimentActor::OnConstruction(FTransform const& transform) {
    Super::OnConstruction(transform);
    ensure_material();
    update_beam_transform();
    apply_settings();
}

void AEnergyBeamExperimentActor::Tick(float const delta_seconds) {
    Super::Tick(delta_seconds);
    if (!settings.animation_paused) {
        animation_time_ += FMath::Max(delta_seconds, 0.0f);
    }
    ensure_material();
    update_beam_transform();
    apply_settings();
}

bool AEnergyBeamExperimentActor::ShouldTickIfViewportsOnly() const {
    return true;
}

void AEnergyBeamExperimentActor::reverse_direction() {
    Swap(settings.source_offset, settings.destination_offset);
    update_beam_transform();
    apply_settings();
}

void AEnergyBeamExperimentActor::set_short_beam() {
    settings.destination_offset = settings.source_offset + FVector{0.0, 0.0, 450.0};
    update_beam_transform();
    apply_settings();
}

void AEnergyBeamExperimentActor::set_long_beam() {
    settings.destination_offset = settings.source_offset + FVector{0.0, 0.0, 1400.0};
    update_beam_transform();
    apply_settings();
}

void AEnergyBeamExperimentActor::restart_animation() {
    settings.animation_paused = false;
    animation_time_ = 0.0f;
    apply_settings();
}

float AEnergyBeamExperimentActor::beam_length() const {
    return static_cast<float>(
        FVector::Distance(settings.source_offset, settings.destination_offset));
}

void AEnergyBeamExperimentActor::ensure_material() {
    if (!IsValid(beam_material_)) {
        beam_material_ = LoadObject<UMaterialInterface>(nullptr, beam_material_path);
    }
    if (IsValid(beam_material_) && !IsValid(material_instance_)) {
        material_instance_ = beam_mesh_->CreateDynamicMaterialInstance(0, beam_material_);
    }
    if (!IsValid(material_instance_)) {
        UE_LOG(LogEnergyBeamExperiment,
               Warning,
               TEXT("Energy beam material is unavailable; regenerate showcase assets."));
    }
}

void AEnergyBeamExperimentActor::update_beam_transform() {
    auto const delta{settings.destination_offset - settings.source_offset};
    auto const length{delta.Size()};
    if (length <= UE_SMALL_NUMBER) {
        beam_mesh_->SetVisibility(false);
        return;
    }

    beam_mesh_->SetVisibility(true);
    auto const direction{delta / length};
    auto const midpoint{(settings.source_offset + settings.destination_offset) * 0.5};
    auto const rotation{FQuat::FindBetweenNormals(FVector::UpVector, direction)};
    auto const width_scale{FMath::Max(settings.beam_width, 1.0f) / 100.0f};
    beam_mesh_->SetRelativeLocation(midpoint);
    beam_mesh_->SetRelativeRotation(rotation);
    beam_mesh_->SetRelativeScale3D(FVector{width_scale, width_scale, length / 100.0});
}

void AEnergyBeamExperimentActor::apply_settings() {
    if (!IsValid(material_instance_)) {
        return;
    }
    auto const actor_transform{GetActorTransform()};
    auto const source_world{actor_transform.TransformPosition(settings.source_offset)};
    auto const destination_world{actor_transform.TransformPosition(settings.destination_offset)};
    material_instance_->SetVectorParameterValue(TEXT("SourcePosition"), as_parameter(source_world));
    material_instance_->SetVectorParameterValue(TEXT("DestinationPosition"),
                                                as_parameter(destination_world));
    material_instance_->SetVectorParameterValue(TEXT("CoreColour"), settings.core_colour);
    material_instance_->SetVectorParameterValue(TEXT("SheathColour"), settings.sheath_colour);
    material_instance_->SetScalarParameterValue(TEXT("AnimationTime"), animation_time_);
    material_instance_->SetScalarParameterValue(TEXT("BeamWidth"),
                                                FMath::Max(settings.beam_width, 1.0f));
    material_instance_->SetScalarParameterValue(TEXT("FlowSpeed"),
                                                FMath::Max(settings.flow_speed, 0.0f));
    material_instance_->SetScalarParameterValue(TEXT("Turbulence"),
                                                FMath::Clamp(settings.turbulence, 0.0f, 2.0f));
    material_instance_->SetScalarParameterValue(TEXT("EmissiveIntensity"),
                                                FMath::Max(settings.emissive_intensity, 0.0f));
}
