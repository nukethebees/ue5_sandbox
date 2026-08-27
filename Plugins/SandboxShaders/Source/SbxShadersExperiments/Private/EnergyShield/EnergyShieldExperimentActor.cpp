#include "SbxShadersExperiments/EnergyShield/EnergyShieldExperimentActor.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

DEFINE_LOG_CATEGORY_STATIC(LogEnergyShieldExperiment, Log, All);

AEnergyShieldExperimentActor::AEnergyShieldExperimentActor() {
    PrimaryActorTick.bCanEverTick = false;

    shield_mesh_ = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ShieldMesh"));
    SetRootComponent(shield_mesh_);
    shield_mesh_->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    shield_mesh_->SetCastShadow(false);
    shield_mesh_->SetRelativeScale3D(FVector{4.0});

    static ConstructorHelpers::FObjectFinder<UStaticMesh> const sphere_mesh{
        TEXT("/Engine/BasicShapes/Sphere.Sphere")};
    if (sphere_mesh.Succeeded()) {
        shield_mesh_->SetStaticMesh(sphere_mesh.Object);
    } else {
        UE_LOG(LogEnergyShieldExperiment, Error, TEXT("Could not load the engine sphere mesh."));
    }

    static ConstructorHelpers::FObjectFinder<UMaterialInterface> const shield_material{
        TEXT("/SandboxShaders/Experiments/EnergyShield/M_EnergyShield.M_EnergyShield")};
    if (shield_material.Succeeded()) {
        shield_material_ = shield_material.Object;
        shield_mesh_->SetMaterial(0, shield_material_);
    }
}

void AEnergyShieldExperimentActor::OnConstruction(FTransform const& transform) {
    Super::OnConstruction(transform);

    if (!IsValid(shield_material_)) {
        shield_material_ = LoadObject<UMaterialInterface>(
            nullptr,
            TEXT("/SandboxShaders/Experiments/EnergyShield/M_EnergyShield.M_EnergyShield"));
    }
    if (!IsValid(shield_material_)) {
        UE_LOG(LogEnergyShieldExperiment,
               Warning,
               TEXT("Energy shield material is unavailable; regenerate the showcase assets."));
        return;
    }

    material_instance_ = shield_mesh_->CreateDynamicMaterialInstance(0, shield_material_);
    if (!IsValid(material_instance_)) {
        UE_LOG(LogEnergyShieldExperiment,
               Error,
               TEXT("Could not create the shield material instance."));
        return;
    }

    apply_settings();
}

void AEnergyShieldExperimentActor::apply_settings() {
    if (!IsValid(material_instance_)) {
        return;
    }

    material_instance_->SetVectorParameterValue(TEXT("BaseColour"), settings.base_colour);
    material_instance_->SetVectorParameterValue(TEXT("EdgeColour"), settings.edge_colour);
    material_instance_->SetScalarParameterValue(TEXT("HexScale"),
                                                FMath::Max(settings.hex_scale, 1.0f));
    material_instance_->SetScalarParameterValue(TEXT("GridWidth"),
                                                FMath::Clamp(settings.grid_width, 0.005f, 0.45f));
    material_instance_->SetScalarParameterValue(TEXT("FresnelPower"),
                                                FMath::Max(settings.fresnel_power, 0.1f));
    material_instance_->SetScalarParameterValue(TEXT("ScanSpeed"),
                                                FMath::Max(settings.scan_speed, 0.0f));
    material_instance_->SetScalarParameterValue(TEXT("ScanWidth"),
                                                FMath::Clamp(settings.scan_width, 0.005f, 0.5f));
    material_instance_->SetScalarParameterValue(TEXT("Distortion"),
                                                FMath::Clamp(settings.distortion, 0.0f, 1.0f));
    material_instance_->SetScalarParameterValue(TEXT("AnimationSpeed"),
                                                FMath::Max(settings.animation_speed, 0.0f));
    material_instance_->SetScalarParameterValue(TEXT("EmissiveIntensity"),
                                                FMath::Max(settings.emissive_intensity, 0.0f));
    material_instance_->SetScalarParameterValue(TEXT("ShieldOpacity"),
                                                FMath::Clamp(settings.opacity, 0.0f, 1.0f));
}
