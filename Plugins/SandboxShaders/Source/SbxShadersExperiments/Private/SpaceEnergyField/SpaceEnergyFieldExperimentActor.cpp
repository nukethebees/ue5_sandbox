#include "SbxShadersExperiments/SpaceEnergyField/SpaceEnergyFieldExperimentActor.h"

#include "SpaceEnergyFieldRenderer.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "RenderingThread.h"
#include "UObject/ConstructorHelpers.h"

DEFINE_LOG_CATEGORY_STATIC(LogSpaceEnergyFieldExperiment, Log, All);

ASpaceEnergyFieldExperimentActor::ASpaceEnergyFieldExperimentActor() {
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = true;

    display_mesh_ = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DisplayMesh"));
    SetRootComponent(display_mesh_);
    display_mesh_->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    display_mesh_->SetCastShadow(false);
    display_mesh_->SetRelativeScale3D(FVector{8.0, 4.5, 1.0});

    static ConstructorHelpers::FObjectFinder<UStaticMesh> const plane_mesh{
        TEXT("/Engine/BasicShapes/Plane.Plane")};
    if (plane_mesh.Succeeded()) {
        display_mesh_->SetStaticMesh(plane_mesh.Object);
    } else {
        UE_LOG(LogSpaceEnergyFieldExperiment, Error, TEXT("Could not load the engine plane mesh."));
    }

    static ConstructorHelpers::FObjectFinder<UMaterialInterface> const display_material{
        TEXT("/SandboxShaders/Experiments/SpaceEnergyField/"
             "M_SpaceEnergyFieldDisplay.M_SpaceEnergyFieldDisplay")};
    if (display_material.Succeeded()) {
        display_material_ = display_material.Object;
        display_mesh_->SetMaterial(0, display_material_);
    }
}

void ASpaceEnergyFieldExperimentActor::OnConstruction(FTransform const& transform) {
    Super::OnConstruction(transform);
    ensure_render_resources();
}

void ASpaceEnergyFieldExperimentActor::Tick(float const delta_seconds) {
    Super::Tick(delta_seconds);

    ensure_render_resources();
    auto const time_seconds{GetWorld() != nullptr ? GetWorld()->GetTimeSeconds() : 0.0f};
    submit_render(time_seconds * FMath::Max(settings.animation_speed, 0.0f));
}

bool ASpaceEnergyFieldExperimentActor::ShouldTickIfViewportsOnly() const {
    return true;
}

void ASpaceEnergyFieldExperimentActor::ensure_render_resources() {
    auto const requested_resolution{FMath::Clamp(settings.resolution, 64, 2048)};
    if (!IsValid(output_texture_) || output_texture_->SizeX != requested_resolution ||
        output_texture_->SizeY != requested_resolution) {
        output_texture_ = NewObject<UTextureRenderTarget2D>(this, TEXT("SpaceEnergyFieldOutput"));
        if (!IsValid(output_texture_)) {
            UE_LOG(LogSpaceEnergyFieldExperiment,
                   Error,
                   TEXT("Could not allocate the space field render target."));
            return;
        }

        output_texture_->bSupportsUAV = true;
        output_texture_->ClearColor = FLinearColor::Black;
        output_texture_->Filter = TF_Bilinear;
        output_texture_->AddressX = TA_Clamp;
        output_texture_->AddressY = TA_Clamp;
        output_texture_->RenderTargetFormat = RTF_RGBA16f;
        output_texture_->InitAutoFormat(requested_resolution, requested_resolution);
        output_texture_->UpdateResourceImmediate(true);
    }

    if (!IsValid(display_material_)) {
        display_material_ = LoadObject<UMaterialInterface>(
            nullptr,
            TEXT("/SandboxShaders/Experiments/SpaceEnergyField/"
                 "M_SpaceEnergyFieldDisplay.M_SpaceEnergyFieldDisplay"));
    }
    if (!IsValid(material_instance_) && IsValid(display_material_)) {
        material_instance_ = display_mesh_->CreateDynamicMaterialInstance(0, display_material_);
    }
    if (IsValid(material_instance_)) {
        reported_missing_material_ = false;
        material_instance_->SetTextureParameterValue(TEXT("FieldTexture"), output_texture_);
    } else if (!IsValid(display_material_) && !reported_missing_material_) {
        reported_missing_material_ = true;
        UE_LOG(
            LogSpaceEnergyFieldExperiment,
            Warning,
            TEXT("Space field display material is unavailable; regenerate the showcase assets."));
    }
}

void ASpaceEnergyFieldExperimentActor::submit_render(float const time_seconds) {
    if (!IsValid(output_texture_) || GUsingNullRHI) {
        return;
    }

    auto* const output_resource{output_texture_->GameThread_GetRenderTargetResource()};
    if (output_resource == nullptr) {
        UE_LOG(LogSpaceEnergyFieldExperiment,
               Error,
               TEXT("Space field render target has no resource."));
        return;
    }

    FSpaceEnergyFieldRenderParameters const parameters{
        .output_size = FIntPoint{output_texture_->SizeX, output_texture_->SizeY},
        .time_seconds = time_seconds,
        .warp_scale = FMath::Max(settings.warp_scale, 0.1f),
        .warp_strength = FMath::Clamp(settings.warp_strength, 0.0f, 3.0f),
        .star_density = FMath::Clamp(settings.star_density, 0.0f, 1.0f),
        .star_intensity = FMath::Max(settings.star_intensity, 0.0f),
        .plasma_intensity = FMath::Max(settings.plasma_intensity, 0.0f),
        .colour_a = FVector4f{settings.colour_a},
        .colour_b = FVector4f{settings.colour_b},
    };

    ENQUEUE_RENDER_COMMAND(RenderSpaceEnergyField)
    ([parameters, output_resource](FRHICommandListImmediate& rhi_command_list) {
        render_space_energy_field(rhi_command_list, parameters, output_resource);
    });
}
