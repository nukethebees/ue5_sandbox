#pragma once

#include <lispb/schema/resolved_type_ref.h>

#include <optional>
#include <string>

namespace lispb::schema {
struct FacadeParameter {
    std::string name;
    ResolvedTypeRef type;
    std::optional<std::string> default_value;
};
}
