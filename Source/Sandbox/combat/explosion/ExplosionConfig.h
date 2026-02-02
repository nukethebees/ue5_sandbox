#pragma once

#include "CoreMinimal.h"
#include "NiagaraDataChannel.h"

#include "Sandbox/health/HealthChange.h"

#include "ExplosionConfig.generated.h"

USTRUCT(BlueprintType)
struct FExplosionConfig {
    GENERATED_BODY()

    FExplosionConfig() = default;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Explosion")
    FHealthChange damage_config{25.0f, EHealthChangeType::Damage};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Explosion")
    float explosion_radius{300.0f};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Explosion")
    float explosion_force{5000.0f};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Explosion")
    TObjectPtr<UNiagaraDataChannelAsset> explosion_channel{};
};
