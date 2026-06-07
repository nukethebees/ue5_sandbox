#pragma once

#include "Sandbox/core/Cooldown.h"

#include "CoreMinimal.h"

#include "ActorLoggingConfig.generated.h"

UENUM()
enum class EActorLogVerbosity : uint8 {
    None,
    Basic,
    Verbose,
    VeryVerbose,
};

USTRUCT()
struct FActorLoggingConfig {
    GENERATED_BODY()

    FActorLoggingConfig() = default;
    FActorLoggingConfig(EActorLogVerbosity verbosity, float cooldown);
    FActorLoggingConfig(float cooldown);

    void reset();
    void tick(float dt);
    bool on_tick_end();

    bool can_log(EActorLogVerbosity msg_verbosity) const;
    bool can_tick_log(EActorLogVerbosity msg_verbosity) const;

    UPROPERTY(EditAnywhere)
    EActorLogVerbosity verbosity{EActorLogVerbosity::Basic};
    UPROPERTY(EditAnywhere)
    FCooldown tick_cooldown{2.f};
};
