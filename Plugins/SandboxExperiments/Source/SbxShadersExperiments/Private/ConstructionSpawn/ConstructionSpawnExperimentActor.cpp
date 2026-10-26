#include "SbxShadersExperiments/ConstructionSpawn/ConstructionSpawnExperimentActor.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

DEFINE_LOG_CATEGORY_STATIC(LogConstructionSpawnExperiment, Log, All);

namespace {
constexpr TCHAR construction_material_path[]{
    TEXT("/SandboxShaders/Experiments/ConstructionSpawn/M_ConstructionSpawn.M_ConstructionSpawn")};
}

AConstructionSpawnExperimentActor::AConstructionSpawnExperimentActor() {
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = true;

    construction_mesh_ = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ConstructionMesh"));
    SetRootComponent(construction_mesh_);
    construction_mesh_->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    construction_mesh_->SetCastShadow(false);
    construction_mesh_->SetRelativeScale3D(FVector{3.8, 3.8, 5.2});

    static ConstructorHelpers::FObjectFinder<UStaticMesh> const cube_mesh{
        TEXT("/Engine/BasicShapes/Cube.Cube")};
    if (cube_mesh.Succeeded()) {
        construction_mesh_->SetStaticMesh(cube_mesh.Object);
    } else {
        UE_LOG(LogConstructionSpawnExperiment, Error, TEXT("Could not load the engine cube mesh."));
    }

    static ConstructorHelpers::FObjectFinder<UMaterialInterface> const construction_material{
        construction_material_path};
    if (construction_material.Succeeded()) {
        construction_material_ = construction_material.Object;
        construction_mesh_->SetMaterial(0, construction_material_);
    }
}

void AConstructionSpawnExperimentActor::OnConstruction(FTransform const& transform) {
    Super::OnConstruction(transform);
    animated_progress_ = FMath::Clamp(settings.progress, 0.0f, 1.0f);
    ensure_material();
    apply_settings();
}

void AConstructionSpawnExperimentActor::Tick(float const delta_seconds) {
    Super::Tick(delta_seconds);
    if (!settings.animation_paused) {
        animation_time_ += FMath::Max(delta_seconds, 0.0f);
        if (settings.auto_animate) {
            animated_progress_ +=
                FMath::Max(delta_seconds, 0.0f) * FMath::Max(settings.animation_speed, 0.0f);
            if (animated_progress_ > 1.08f) {
                animated_progress_ = -0.08f;
            }
        } else {
            animated_progress_ = FMath::Clamp(settings.progress, 0.0f, 1.0f);
        }
    }
    ensure_material();
    apply_settings();
}

bool AConstructionSpawnExperimentActor::ShouldTickIfViewportsOnly() const {
    return true;
}

void AConstructionSpawnExperimentActor::reset_effect() {
    settings.auto_animate = false;
    settings.progress = 0.0f;
    animated_progress_ = 0.0f;
    apply_settings();
}

void AConstructionSpawnExperimentActor::complete_effect() {
    settings.auto_animate = false;
    settings.progress = 1.0f;
    animated_progress_ = 1.0f;
    apply_settings();
}

void AConstructionSpawnExperimentActor::restart_animation() {
    settings.auto_animate = true;
    settings.animation_paused = false;
    settings.progress = 0.0f;
    animated_progress_ = 0.0f;
    animation_time_ = 0.0f;
    apply_settings();
}

void AConstructionSpawnExperimentActor::ensure_material() {
    if (!IsValid(construction_material_)) {
        construction_material_ =
            LoadObject<UMaterialInterface>(nullptr, construction_material_path);
    }
    if (IsValid(construction_material_) && !IsValid(material_instance_)) {
        material_instance_ =
            construction_mesh_->CreateDynamicMaterialInstance(0, construction_material_);
    }
    if (!IsValid(material_instance_)) {
        UE_LOG(LogConstructionSpawnExperiment,
               Warning,
               TEXT("Construction material is unavailable; regenerate showcase assets."));
    }
}

void AConstructionSpawnExperimentActor::apply_settings() {
    if (!IsValid(material_instance_)) {
        return;
    }
    material_instance_->SetScalarParameterValue(TEXT("Progress"),
                                                FMath::Clamp(animated_progress_, -0.1f, 1.1f));
    material_instance_->SetScalarParameterValue(TEXT("AnimationTime"), animation_time_);
    material_instance_->SetScalarParameterValue(TEXT("EdgeWidth"),
                                                FMath::Clamp(settings.edge_width, 0.005f, 0.35f));
    material_instance_->SetScalarParameterValue(TEXT("GridScale"),
                                                FMath::Max(settings.grid_scale, 1.0f));
    material_instance_->SetScalarParameterValue(TEXT("NoiseScale"),
                                                FMath::Max(settings.noise_scale, 0.1f));
    material_instance_->SetScalarParameterValue(TEXT("EmissiveIntensity"),
                                                FMath::Max(settings.emissive_intensity, 0.0f));
    material_instance_->SetVectorParameterValue(TEXT("BaseColour"), settings.base_colour);
    material_instance_->SetVectorParameterValue(TEXT("EdgeColour"), settings.edge_colour);
}
