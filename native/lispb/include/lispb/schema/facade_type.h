#pragma once

#include <lispb/schema/facade_method.h>
#include <lispb/schema/resolved_type_ref.h>

#include <string>
#include <vector>

namespace lispb::schema {
struct FacadeType {
    ResolvedTypeRef target;
    std::string target_member_name;
    bool reference_target{};
    std::vector<FacadeMethod> methods;
};
}
