#pragma once

#include <codegen/schema/type_ref.h>

#include <string>

namespace codegen {

struct OptionalSentinelSchema {
    std::string name;
    TypeRef source;
    std::string sentinel;
};

} // namespace codegen
