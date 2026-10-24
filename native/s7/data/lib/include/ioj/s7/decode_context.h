#pragma once

#include <ioj/s7/ast_result.h>
#include <ioj/s7/read_mode.h>

#include <string_view>

namespace ioj::s7 {
class Reader;

struct DecodeContext {
    Reader const& reader;
    NodeIndex node;
    std::string_view path;
    ReadMode mode;
    AstDiagnostics& errors;
};
}
