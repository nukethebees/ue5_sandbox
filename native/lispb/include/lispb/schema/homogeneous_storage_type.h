#pragma once

#include <lispb/schema/resolved_type_ref.h>

#include <optional>
#include <string>
#include <vector>

namespace lispb::schema {
struct HomogeneousStorageType {
    std::vector<std::string> components;
    ResolvedTypeRef value_type;
    std::optional<ResolvedTypeRef> equivalent_type;
    std::vector<std::string> input_members;
    std::vector<ResolvedTypeRef> input_types;
    std::string view_template_name;
};
}
