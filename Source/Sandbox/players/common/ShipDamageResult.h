#pragma once

#include "CoreMinimal.h"

enum class EDamageResult : uint8 { NoEffect, Damaged, Killed };

struct FShipDamageResult {
    FShipDamageResult() = delete;
    FShipDamageResult(EDamageResult result_type)
        : result_type(result_type) {}

    auto was_killed() const;
  protected:
    EDamageResult result_type;
};
