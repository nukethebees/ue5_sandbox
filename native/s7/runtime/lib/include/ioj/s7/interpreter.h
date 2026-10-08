#pragma once

#include <ioj/s7/ast_limits.h>
#include <ioj/s7/ast_result.h>
#include <ioj/s7/interpreter_options.h>

#include <expected>
#include <memory>
#include <string>
#include <string_view>

namespace ioj::s7 {
using EvaluationResult = std::expected<std::string, std::string>;

// Own one isolated runtime. Returned text and AST data outlive this interpreter.
class Interpreter {
  public:
    Interpreter();
    explicit Interpreter(InterpreterOptions options);
    ~Interpreter();

    Interpreter(Interpreter const&) = delete;
    auto operator=(Interpreter const&) -> Interpreter& = delete;

    [[nodiscard]] auto evaluate(std::string_view expression) -> EvaluationResult;
    [[nodiscard]] auto evaluate_ast(std::string_view expression, AstLimits limits = {})
        -> AstResult;
  private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}
