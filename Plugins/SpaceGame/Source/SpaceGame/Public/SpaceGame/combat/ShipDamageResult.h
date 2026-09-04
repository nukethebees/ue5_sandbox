#pragma once

#include "CoreMinimal.h"

enum class EDamageResult : uint8 {
    NoEffect,
    Damaged,
    ComponentDestroyed,
    ActorKilled,
    AlreadyDestroyed,
};

struct FShipDamageResult {
    FShipDamageResult() = delete;
    FShipDamageResult(EDamageResult result_type)
        : result_type(result_type) {}

    bool was_killed() const;
  protected:
    EDamageResult result_type;
};
