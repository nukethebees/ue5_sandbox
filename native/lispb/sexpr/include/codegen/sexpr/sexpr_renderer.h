#pragma once

#include <codegen/sexpr/dialect.h>
#include <codegen/sexpr/expr.h>

#include <cstddef>
#include <initializer_list>
#include <span>
#include <string>
#include <string_view>

namespace lispb::codegen::sexpr {
class SExprIR;

// The IR and its source string store must outlive the renderer.
class SExprRenderer {
  public:
    explicit SExprRenderer(SExprIR const& ir, Dialect dialect = Dialect::lispb);
    auto render(Expr value) const -> std::string;
    auto render_document(std::span<Expr const> roots) const -> std::string;
    auto render_document(std::initializer_list<Expr> roots) const -> std::string;
  private:
    void append_string(std::string& output, std::string_view value) const;
    void append_values(std::string& output, std::span<Expr const> values, std::size_t depth) const;
    void append(std::string& output, Expr value, std::size_t depth) const;

    SExprIR const& ir_;
    Dialect dialect_;
};
}
