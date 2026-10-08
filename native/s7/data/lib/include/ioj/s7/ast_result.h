#pragma once

#include <ioj/s7/ast.h>
#include <ioj/s7/ast_diagnostic.h>

#include <expected>
#include <vector>

namespace ioj::s7 {
using AstDiagnostics = std::vector<AstDiagnostic>;
using AstResult = std::expected<Ast, AstDiagnostics>;
}
