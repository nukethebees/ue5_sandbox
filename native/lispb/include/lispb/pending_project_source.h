#pragma once

#include <lispb/indices.h>
#include <lispb/target_id.h>

namespace lispb {

struct PendingProjectSource {
    TargetId target;
    StringId contents;
};

}
