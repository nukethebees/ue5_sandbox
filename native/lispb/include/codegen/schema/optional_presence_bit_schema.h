#pragma once

#include <codegen/schema/type_ref.h>

#include <string>

namespace codegen {

struct OptionalPresenceBitSchema {
    std::string name;
    TypeRef source;
};

} // namespace codegen
