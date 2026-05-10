#pragma once

#include "Sandbox/core/Cooldown.h"

#include "CoreMinimal.h"

#include "ActorLoggingConfig.generated.h"

UENUM()
enum class EActorLoggingVerbosity : uint8 {
    None,
    Basic,
    Verbose,
    VeryVerbose,
};

USTRUCT()
struct FActorLoggingConfig {
    GENERATED_BODY()

    FActorLoggingConfig() = default;
    FActorLoggingConfig(EActorLoggingVerbosity verbosity, float cooldown);
    FActorLoggingConfig(float cooldown);

    void reset();
    void tick(float dt);
    bool on_tick_end();

    bool can_log(EActorLoggingVerbosity msg_verbosity) const;
    bool can_tick_log(EActorLoggingVerbosity msg_verbosity) const;

    UPROPERTY(EditAnywhere)
    EActorLoggingVerbosity verbosity{EActorLoggingVerbosity::Basic};
    UPROPERTY(EditAnywhere)
    FCooldown tick_cooldown{2.f};
};
