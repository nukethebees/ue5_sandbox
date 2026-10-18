#pragma once

#include <codegen/sexpr/handle_range.h>
#include <codegen/sexpr/indices.h>
#include <codegen/sexpr/token_range.h>

#include <cstddef>
#include <vector>

namespace lispb::codegen::sexpr {
class Syntax;

class SyntaxParser {
  public:
    // Supply a complete lexer-produced range, including its final end token.
    SyntaxParser(Syntax& syntax, TokenRange tokens);
    auto parse() -> HandleRange;
  private:
    auto form() -> FormId;
    Syntax& syntax_;
    TokenRange tokens_;
    std::size_t index_{};
    std::vector<FormId> children_;
};

}
