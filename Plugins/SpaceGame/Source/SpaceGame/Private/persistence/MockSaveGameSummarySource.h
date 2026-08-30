#pragma once

#include "SpaceGame/persistence/SaveGameBrowser.h"

namespace ml::ioj::detail {
auto make_mock_save_game_browser() -> FSaveGameBrowser;
}
