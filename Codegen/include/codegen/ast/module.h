#pragma once

#include <codegen/ast/cpp_file.h>

#include <optional>
#include <string>

namespace codegen {

struct Module {
    std::string name;
    std::optional<CppFile> header;
    std::optional<CppFile> source;
};

} // namespace codegen
