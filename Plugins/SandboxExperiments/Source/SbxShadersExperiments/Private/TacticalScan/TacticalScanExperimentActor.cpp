#include "SbxShadersExperiments/TacticalScan/TacticalScanExperimentActor.h"

#include "Components/BoxComponent.h"
#include "Components/PostProcessComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

DEFINE_LOG_CATEGORY_STATIC(LogTacticalScanExperiment, Log, All);

namespace {
constexpr TCHAR tactical_scan_material_path[]{
    TEXT("/SandboxShaders/Experiments/TacticalScan/M_TacticalScan.M_TacticalScan")};
}

ATacticalScanExperimentActor::ATacticalScanExperimentActor() {
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = true;

    scan_bounds_ = CreateDefaultSubobject<UBoxComponent>(TEXT("ScanBounds"));
    SetRootComponent(scan_bounds_);
    scan_bounds_->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    scan_bounds_->SetBoxExtent(settings.volume_extent);

    post_process_ = CreateDefaultSubobject<UPostProcessComponent>(TEXT("ScanPostProcess"));
    post_process_->SetupAttachment(scan_bounds_);
    post_process_->bUnbound = false;
    post_process_->BlendRadius = 80.0f;
    post_process_->BlendWeight = 1.0f;

    static ConstructorHelpers::FObjectFinder<UMaterialInterface> const scan_material{
        tactical_scan_material_path};
    if (scan_material.Succeeded()) {
        scan_material_ = scan_material.Object;
    }
}

void ATacticalScanExperimentActor::OnConstruction(FTransform const& transform) {
    Super::OnConstruction(transform);
    ensure_material();
    apply_settings();
}

void ATacticalScanExperimentActor::Tick(float const delta_seconds) {
    Super::Tick(delta_seconds);
    if (!settings.animation_paused) {
        animation_time_ += FMath::Max(delta_seconds, 0.0f);
    }
    ensure_material();
    apply_settings();
}

bool ATacticalScanExperimentActor::ShouldTickIfViewportsOnly() const {
    return true;
}

void ATacticalScanExperimentActor::restart_scan() {
    animation_time_ = 0.0f;
    apply_settings();
}

void ATacticalScanExperimentActor::ensure_material() {
    if (!IsValid(scan_material_)) {
        scan_material_ = LoadObject<UMaterialInterface>(nullptr, tactical_scan_material_path);
    }
    if (!IsValid(scan_material_)) {
        return;
    }
    if (!IsValid(material_instance_)) {
        material_instance_ = UMaterialInstanceDynamic::Create(scan_material_, this);
        if (IsValid(material_instance_)) {
            post_process_->Settings.WeightedBlendables.Array.Reset();
            post_process_->Settings.AddBlendable(material_instance_, 1.0f);
        }
    }
}

void ATacticalScanExperimentActor::apply_settings() {
    scan_bounds_->SetBoxExtent(settings.volume_extent.ComponentMax(FVector{10.0}));
    if (!IsValid(material_instance_)) {
        return;
    }

    auto const transform{GetActorTransform()};
    auto const display_scale{FMath::Max(transform.GetScale3D().GetAbsMax(), UE_SMALL_NUMBER)};
    material_instance_->SetVectorParameterValue(TEXT("ScanOrigin"),
                                                FLinearColor{transform.GetLocation()});
    material_instance_->SetVectorParameterValue(TEXT("ScanDirection"),
                                                FLinearColor{transform.GetUnitAxis(EAxis::X)});
    material_instance_->SetVectorParameterValue(TEXT("GridAxisU"),
                                                FLinearColor{transform.GetUnitAxis(EAxis::Y)});
    material_instance_->SetVectorParameterValue(TEXT("GridAxisV"),
                                                FLinearColor{transform.GetUnitAxis(EAxis::Z)});
    material_instance_->SetVectorParameterValue(TEXT("ScanColour"), settings.scan_colour);
    material_instance_->SetScalarParameterValue(TEXT("AnimationTime"), animation_time_);
    material_instance_->SetScalarParameterValue(
        TEXT("ScanRange"), FMath::Max(settings.scan_range, 100.0f) * display_scale);
    material_instance_->SetScalarParameterValue(
        TEXT("ScanSpeed"), FMath::Max(settings.scan_speed, 0.0f) * display_scale);
    material_instance_->SetScalarParameterValue(
        TEXT("ScanWidth"), FMath::Max(settings.scan_width, 1.0f) * display_scale);
    material_instance_->SetScalarParameterValue(TEXT("ScanIntensity"),
                                                FMath::Max(settings.intensity, 0.0f));
    material_instance_->SetScalarParameterValue(TEXT("ScanFalloff"),
                                                FMath::Max(settings.falloff, 1.0f));
    material_instance_->SetScalarParameterValue(
        TEXT("GridScale"), FMath::Max(settings.grid_scale, 5.0f) * display_scale);
}
