#pragma once

#include <lispb/indices.h>
#include <lispb/target_id.h>

#include <vector>
namespace lispb {

struct ProjectSourceCommands {
    std::vector<TargetId> targets;
    std::vector<StringId> sources;
};

}
