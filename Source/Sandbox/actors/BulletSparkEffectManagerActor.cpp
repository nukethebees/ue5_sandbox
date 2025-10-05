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
    if (n == 0) {
        return;
    }

    logger.log_verbose(TEXT("Writing %d impacts."), n);

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
        writer->WritePosition(position_label, i, impacts[i].location);
        writer->WriteQuat(rotation_label, i, impacts[i].rotation.Quaternion());
    }
}
