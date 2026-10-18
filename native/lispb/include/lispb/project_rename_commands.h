#pragma once

#include <lispb/indices.h>
#include <lispb/target_id.h>

#include <vector>
namespace lispb {

struct ProjectRenameCommands {
    std::vector<TargetId> targets;
    std::vector<StringId> sources;
    std::vector<StringId> destinations;
};

}
