#pragma once

#include <lispb/schema/facade_parameter.h>
#include <lispb/schema/resolved_type_ref.h>

#include <string>
#include <vector>

namespace lispb::schema {
struct FacadeMethod {
    std::string name;
    ResolvedTypeRef return_type;
    std::vector<FacadeParameter> parameters;
    std::string target_name;
    bool is_const{};
    bool is_noexcept{};
};
}
