#include "Sandbox/actors/BulletSparkEffectManagerActor.h"

#include "NiagaraDataChannel.h"
#include "NiagaraDataChannelAccessor.h"
#include "NiagaraDataInterface.h"
#include "NiagaraDataInterfaceArrayFunctionLibrary.h"
#include "NiagaraFunctionLibrary.h"

#include "Sandbox/subsystems/world/BulletSparkEffectSubsystem.h"

#include "Sandbox/macros/null_checks.hpp"

ABulletSparkEffectManagerActor::ABulletSparkEffectManagerActor() {
    PrimaryActorTick.bCanEverTick = false;

    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

    // niagara_component = CreateDefaultSubobject<UNiagaraComponent>(TEXT("NiagaraComponent"));
    // RootComponent = niagara_component;
    // niagara_component->bAutoActivate = true;
    // niagara_component->SetAutoDestroy(false);
}

void ABulletSparkEffectManagerActor::BeginPlay() {
    constexpr auto logger{NestedLogger<"BeginPlay">()};

    Super::BeginPlay();

    TRY_INIT_PTR(subsystem, GetWorld()->GetSubsystem<UBulletSparkEffectSubsystem>());
    subsystem->register_actor(this);
}

void ABulletSparkEffectManagerActor::consume_impacts(std::span<FSparkEffectTransform> impacts) {
    constexpr auto logger{NestedLogger<"consume_impacts">()};

    RETURN_IF_NULLPTR(ndc_asset);

    auto const n{static_cast<int32>(impacts.size())};
    impact_positions.SetNum(n);
    impact_rotations.SetNum(n);

    if (n == 0) {
        return;
    }

    for (int32 i{0}; i < n; ++i) {
        impact_positions[i] = impacts[i].location;
        impact_rotations[i] = impacts[i].rotation.Quaternion();
        logger.log_verbose(TEXT("Copying impact %d to %s / %s"),
                           i,
                           *impact_positions[i].ToString(),
                           *impact_rotations[i].ToString());
    }

    logger.log_verbose(TEXT("Writing %d impacts."), n);
    auto n_pos{impact_positions.Num()};
    auto n_rot{impact_rotations.Num()};
    if (n_pos != n) {
        logger.log_verbose(TEXT("%d impact_positions"), n_pos);
    }
    if (n_rot != n) {
        logger.log_verbose(TEXT("%d impact_rotations"), n_rot);
    }

    if (!niagara_component) {
        logger.log_verbose(TEXT("Component is off"));
    }

    // if (!niagara_component->IsActive()) {
    //     logger.log_verbose(TEXT("Is not active"));
    // }
    // if (!niagara_component->IsRegistered()) {
    //     logger.log_verbose(TEXT("Is not registered"));
    // }

    auto const n_bursts_label{FName("n_bursts")};
    auto const impact_positions_label{FName("impact_positions")};
    auto const impact_rotations_label{FName("impact_rotations")};

    using NL = UNiagaraDataInterfaceArrayFunctionLibrary;

    constexpr bool bVisibleToGame{true};
    constexpr bool bVisibleToCPU{true};
    constexpr bool bVisibleToGPU{true};
    FString DebugSource{};

    FNiagaraDataChannelSearchParameters search_params{RootComponent};
    // This calls init write for us
    auto* writer{UNiagaraDataChannelLibrary::WriteToNiagaraDataChannel(GetWorld(),
                                                                       ndc_asset,
                                                                       search_params,
                                                                       n,
                                                                       bVisibleToGame,
                                                                       bVisibleToCPU,
                                                                       bVisibleToGPU,
                                                                       DebugSource)};

    auto const position_label{FName("position")};
    auto const rotation_label{FName("rotation")};
    for (int32 i{0}; i < n; ++i) {
        logger.log_verbose(TEXT("Writing %d [%s/%s"),
                           i,
                           *impact_positions[i].ToString(),
                           *impact_rotations[i].ToString());
        writer->WritePosition(position_label, i, impact_positions[i]);
        writer->WriteQuat(rotation_label, i, impact_rotations[i]);
    }

    // niagara_component->Deactivate();
    // niagara_component->DeactivateImmediate();

    // NL::SetNiagaraArrayPosition(niagara_component, impact_positions_label, impact_positions);
    // NL::SetNiagaraArrayQuat(niagara_component, impact_rotations_label, impact_rotations);
    // niagara_component->SetIntParameter(n_bursts_label, n);
    // niagara_component->SetNiagaraVariableInt(n_bursts_label.ToString(), n);
    // niagara_component->SetVariableInt(n_bursts_label, n);
    // niagara_component->Activate();
    // niagara_component->ActivateSystem();
}
