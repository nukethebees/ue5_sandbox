#pragma once

#include "SpaceGame/persistence/SaveGameBrowser.h"

namespace ioj::detail {
auto make_mock_save_game_browser() -> FSaveGameBrowser;
}
