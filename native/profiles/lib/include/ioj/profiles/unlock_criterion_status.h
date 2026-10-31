#pragma once

#include <ioj/levels/identifiers.h>

namespace ioj::profiles {
struct UnlockCriterionStatus {
    levels::LevelId level_id{};
    bool satisfied{};
};
}
