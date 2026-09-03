#include "SbxShadersExperiments/NebulaVolume/NebulaVolumeExperimentActor.h"

#include "NebulaDensityRenderer.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/TextureRenderTargetVolume.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "RenderingThread.h"
#include "UObject/ConstructorHelpers.h"

DEFINE_LOG_CATEGORY_STATIC(LogNebulaVolumeExperiment, Log, All);

namespace {
constexpr TCHAR nebula_volume_material_path[]{
    TEXT("/SandboxShaders/Experiments/NebulaVolume/M_NebulaVolume.M_NebulaVolume")};
}

ANebulaVolumeExperimentActor::ANebulaVolumeExperimentActor() {
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = true;

    scene_root_ = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
    SetRootComponent(scene_root_);

    volume_mesh_ = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("NebulaVolume"));
    volume_mesh_->SetupAttachment(scene_root_);
    volume_mesh_->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    volume_mesh_->SetCastShadow(false);

    static ConstructorHelpers::FObjectFinder<UStaticMesh> const cube_mesh{
        TEXT("/Engine/BasicShapes/Cube.Cube")};
    if (cube_mesh.Succeeded()) {
        volume_mesh_->SetStaticMesh(cube_mesh.Object);
    } else {
        UE_LOG(LogNebulaVolumeExperiment, Error, TEXT("Could not load the engine cube mesh."));
    }

    static ConstructorHelpers::FObjectFinder<UMaterialInterface> const volume_material{
        nebula_volume_material_path};
    if (volume_material.Succeeded()) {
        volume_material_ = volume_material.Object;
        volume_mesh_->SetMaterial(0, volume_material_);
    }
}

void ANebulaVolumeExperimentActor::OnConstruction(FTransform const& transform) {
    Super::OnConstruction(transform);
    ensure_material();
    ensure_density_volume();
    apply_settings();
    generate_density_if_needed();
}

void ANebulaVolumeExperimentActor::Tick(float const delta_seconds) {
    Super::Tick(delta_seconds);
    if (!settings.animation_paused) {
        animation_time_ += FMath::Max(delta_seconds, 0.0f);
    }
    ensure_material();
    ensure_density_volume();
    apply_settings();
    generate_density_if_needed();
}

bool ANebulaVolumeExperimentActor::ShouldTickIfViewportsOnly() const {
    return true;
}

void ANebulaVolumeExperimentActor::set_low_quality() {
    settings.step_count = 12;
    apply_settings();
}

void ANebulaVolumeExperimentActor::set_balanced_quality() {
    settings.step_count = 24;
    apply_settings();
}

void ANebulaVolumeExperimentActor::set_high_quality() {
    settings.step_count = 40;
    apply_settings();
}

void ANebulaVolumeExperimentActor::restart_animation() {
    settings.animation_paused = false;
    animation_time_ = 0.0f;
    apply_settings();
}

void ANebulaVolumeExperimentActor::regenerate_density() {
    generated_resolution_ = 0;
    ensure_density_volume();
    generate_density_if_needed();
}

void ANebulaVolumeExperimentActor::ensure_material() {
    if (!IsValid(volume_material_)) {
        volume_material_ = LoadObject<UMaterialInterface>(nullptr, nebula_volume_material_path);
    }
    if (IsValid(volume_material_) && !IsValid(material_instance_)) {
        material_instance_ = volume_mesh_->CreateDynamicMaterialInstance(0, volume_material_);
    }
    if (!IsValid(material_instance_)) {
        UE_LOG(LogNebulaVolumeExperiment,
               Warning,
               TEXT("Nebula volume material is unavailable; regenerate showcase assets."));
    }
}

void ANebulaVolumeExperimentActor::ensure_density_volume() {
    auto const requested_resolution{FMath::Clamp(settings.volume_resolution, 32, 256)};
    if (IsValid(density_volume_) && density_volume_->SizeX == requested_resolution &&
        density_volume_->SizeY == requested_resolution &&
        density_volume_->SizeZ == requested_resolution) {
        return;
    }

    density_volume_ = NewObject<UTextureRenderTargetVolume>(this, TEXT("NebulaDensityVolume"));
    if (!IsValid(density_volume_)) {
        UE_LOG(LogNebulaVolumeExperiment, Error, TEXT("Could not allocate the nebula density volume."));
        return;
    }

    density_volume_->bSupportsUAV = true;
    density_volume_->bForceLinearGamma = true;
    density_volume_->ClearColor = FLinearColor::Black;
    density_volume_->Filter = TF_Trilinear;
    density_volume_->Init(requested_resolution, requested_resolution, requested_resolution, PF_G8);
    density_volume_->UpdateResourceImmediate(true);
    generated_resolution_ = 0;
}

void ANebulaVolumeExperimentActor::generate_density_if_needed() {
    if (!IsValid(density_volume_) || GUsingNullRHI) {
        return;
    }

    auto const extent{FVector{FMath::Max(settings.extent.X, 100.0),
                              FMath::Max(settings.extent.Y, 100.0),
                              FMath::Max(settings.extent.Z, 100.0)}};
    auto const feature_size{FMath::Max(settings.feature_size, 100.0f)};
    auto const detail_size{FMath::Max(settings.detail_size, 25.0f)};
    auto const resolution{density_volume_->SizeX};
    if (generated_extent_.Equals(extent) && generated_feature_size_ == feature_size &&
        generated_detail_size_ == detail_size && generated_seed_ == settings.seed &&
        generated_resolution_ == resolution) {
        return;
    }

    auto* const output_resource{density_volume_->GameThread_GetRenderTargetResource()};
    if (output_resource == nullptr) {
        UE_LOG(LogNebulaVolumeExperiment,
               Error,
               TEXT("Nebula density volume has no render resource."));
        return;
    }

    auto const maximum_feature_period{static_cast<float>(resolution / 8)};
    auto const maximum_detail_period{static_cast<float>(resolution / 4)};
    auto const make_period = [](FVector const& requested_period, float const maximum_period) {
        return FVector3f{FMath::Clamp(FMath::RoundToFloat(static_cast<float>(requested_period.X)),
                                     1.0f,
                                     maximum_period),
                         FMath::Clamp(FMath::RoundToFloat(static_cast<float>(requested_period.Y)),
                                     1.0f,
                                     maximum_period),
                         FMath::Clamp(FMath::RoundToFloat(static_cast<float>(requested_period.Z)),
                                     1.0f,
                                     maximum_period)};
    };
    FNebulaDensityRenderParameters const parameters{
        .output_size = FIntVector{resolution, resolution, resolution},
        .feature_period = make_period(extent / feature_size, maximum_feature_period),
        .detail_period = make_period(extent / detail_size, maximum_detail_period),
        .seed = static_cast<float>(settings.seed),
    };

    ENQUEUE_RENDER_COMMAND(GenerateNebulaDensity)
    ([parameters, output_resource](FRHICommandListImmediate& rhi_command_list) {
        render_nebula_density(rhi_command_list, parameters, output_resource);
    });

    generated_extent_ = extent;
    generated_feature_size_ = feature_size;
    generated_detail_size_ = detail_size;
    generated_seed_ = settings.seed;
    generated_resolution_ = resolution;
}

void ANebulaVolumeExperimentActor::apply_settings() {
    auto const extent{FVector{FMath::Max(settings.extent.X, 100.0),
                              FMath::Max(settings.extent.Y, 100.0),
                              FMath::Max(settings.extent.Z, 100.0)}};
    volume_mesh_->SetRelativeScale3D(extent / 100.0);

    if (!IsValid(material_instance_)) {
        return;
    }

    if (IsValid(density_volume_)) {
        material_instance_->SetTextureParameterValue(TEXT("DensityVolume"), density_volume_);
    }
    material_instance_->SetVectorParameterValue(TEXT("ShadowColour"), settings.shadow_colour);
    material_instance_->SetVectorParameterValue(TEXT("EmissionColour"), settings.emission_colour);
    material_instance_->SetVectorParameterValue(
        TEXT("VolumeExtent"),
        FLinearColor{static_cast<float>(extent.X),
                     static_cast<float>(extent.Y),
                     static_cast<float>(extent.Z),
                     0.0f});
    material_instance_->SetScalarParameterValue(TEXT("AnimationTime"), animation_time_);
    material_instance_->SetScalarParameterValue(TEXT("Density"),
                                                FMath::Clamp(settings.density, 0.0f, 4.0f));
    material_instance_->SetScalarParameterValue(TEXT("Extinction"),
                                                FMath::Clamp(settings.extinction, 0.0f, 8.0f));
    material_instance_->SetScalarParameterValue(TEXT("EmissiveStrength"),
                                                FMath::Max(settings.emissive_strength, 0.0f));
    material_instance_->SetScalarParameterValue(TEXT("FeatureSize"),
                                                FMath::Max(settings.feature_size, 100.0f));
    material_instance_->SetScalarParameterValue(
        TEXT("FlowStrength"), FMath::Clamp(settings.flow_strength, 0.0f, 2.0f));
    material_instance_->SetScalarParameterValue(TEXT("DriftSpeed"),
                                                FMath::Max(settings.drift_speed, 0.0f));
    material_instance_->SetScalarParameterValue(
        TEXT("StepCount"), static_cast<float>(FMath::Clamp(settings.step_count, 4, 48)));
}
