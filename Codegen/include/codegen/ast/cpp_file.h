#pragma once

#include <codegen/ast/node.h>

#include <filesystem>
#include <string>
#include <vector>

namespace codegen {

struct CppFile {
    std::filesystem::path path;
    Nodes nodes;
    bool pragma_once{true};
    bool clang_format_off{false};
    std::vector<std::string> prologue;
    std::vector<std::string> epilogue;
    std::vector<std::string> include_order;
};

} // namespace codegen
