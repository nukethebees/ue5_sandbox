#pragma once

#include <lispb/indices.h>

#include <vector>

namespace ioj::layout {
struct FrozenSources {
    std::vector<lispb::StringId> paths;
    std::vector<lispb::StringId> texts;
};
}
