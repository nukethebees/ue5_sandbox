#pragma once

#include <lispb/schema/union_alternative.h>

#include <vector>

namespace lispb::schema {
struct UnionType {
    std::vector<UnionAlternative> alternatives;
};
}
