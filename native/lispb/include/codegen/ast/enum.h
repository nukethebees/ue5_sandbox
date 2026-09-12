#pragma once

#include <codegen/ast/cpp_type.h>
#include <codegen/ast/enumerator.h>

#include <optional>
#include <string>
#include <vector>

namespace codegen {

struct Enum {
    std::string name;
    CppType underlying_type;
    std::vector<Enumerator> values;
    std::optional<std::string> export_specifier;
    bool scoped{true};
};

} // namespace codegen
