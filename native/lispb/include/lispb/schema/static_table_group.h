#pragma once

#include <lispb/schema/resolved_type_ref.h>

#include <string>
#include <vector>

namespace lispb::schema {
struct StaticTableGroup {
    std::string name;
    ResolvedTypeRef result_type;
    std::vector<std::string> columns;
};
}
