#pragma once

#include <ioj/levels/identifiers.h>

namespace ioj::session {
struct LevelSelectRequest {
    levels::LevelId preferred_level_id{};
};
}
