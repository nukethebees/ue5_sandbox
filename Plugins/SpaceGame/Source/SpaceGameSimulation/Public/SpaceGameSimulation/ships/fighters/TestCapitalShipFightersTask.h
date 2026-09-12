#pragma once

#include "CoreMinimal.h"

#include <sandbox/simulation/fighter_types.h>

using ETestCapitalShipFightersTask = ml::simulation::CapitalShipFighterTask;

namespace ml::simulation {
inline auto LexToString(CapitalShipFighterTask const task) -> FString {
    auto const value{ml::simulation::to_string_view(task)};
    return FString{static_cast<int32>(value.size()), UTF8_TO_TCHAR(value.data())};
}
}

using ml::simulation::LexToString;
