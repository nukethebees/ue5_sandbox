#pragma once

#include <lispb/indices.h>

#include <vector>

namespace ioj::layout {
struct FrozenSources {
    std::vector<lispb::IdentifierId> paths;
    std::vector<lispb::StringId> texts;
};
}
