#pragma once

#include <ioj/levels/identifiers.h>

#include <string>

namespace ioj::session {
struct LevelSelectRequest {
    levels::LevelId preferred_level_id{};
    std::string launch_error;
};
}
