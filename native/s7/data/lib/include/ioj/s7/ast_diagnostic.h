#pragma once

#include <ioj/s7/ast_error_code.h>

#include <string>

namespace ioj::s7 {
struct AstDiagnostic {
    AstErrorCode code{};
    std::string node_path{};
    std::string message{};
};
}
