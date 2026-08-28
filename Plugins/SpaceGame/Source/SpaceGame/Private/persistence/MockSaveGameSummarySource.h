#pragma once

#include "SpaceGame/persistence/SaveGameBrowser.h"

namespace ml::ioj::detail {
auto discover_mock_save_game_summaries() -> TArray<FSaveGameSummary>;
}
