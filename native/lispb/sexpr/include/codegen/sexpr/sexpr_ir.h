#pragma once
#include <codegen/sexpr/common_atoms.h>
#include <codegen/sexpr/dialect.h>
#include <codegen/sexpr/output/indices.h>
#include <codegen/sexpr/output/lists.h>
#include <codegen/sexpr/output/texts.h>
#include <codegen/sexpr/serialized.h>

#include <concepts>
#include <format>
#include <initializer_list>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace lispb::codegen::sexpr {
class FormBuilder;

// Handles belong to this IR and remain valid as it grows. The source string store must outlive
// the IR and retain referenced IDs. New spellings are owned by the IR, without growing that store.
// Atoms must be valid tokens; single-line comments belong to form rows or document roots.
class SExprIR {
  public:
    explicit SExprIR(Strings const& strings);
    auto atom(std::string_view value) -> Expr;
    auto atom(StringId value) -> Expr;
    auto atom(IdentifierId value) -> Expr { return atom(string_id(value)); }
    template <Serialized T>
    auto atom(T value) -> Expr {
        return atom(to_serialized_string(value));
    }
    template <std::integral T>
        requires (!std::same_as<T, bool>)
    auto atom(T value) -> Expr {
        return atom(std::format("{}", value));
    }
    auto string(std::string_view value) -> Expr;
    auto string(StringId value) -> Expr;
    auto string(IdentifierId value) -> Expr { return string(string_id(value)); }
    auto comment(std::string_view value) -> Expr;
    auto quote(Expr value) -> Expr;
    auto quote(std::string_view value) -> Expr { return quote(atom(value)); }
    template <Serialized T>
    auto quote(T value) -> Expr {
        return quote(atom(value));
    }
    auto list(std::span<Expr const> values) -> Expr;
    auto list(std::initializer_list<Expr> values) -> Expr;
    auto form(std::string_view head, std::initializer_list<Expr> positional = {}) -> FormBuilder;
    template <Serialized T>
    auto form(T head, std::initializer_list<Expr> positional = {}) -> FormBuilder;
    auto render(Expr value, Dialect dialect = Dialect::lispb) const -> std::string;
    auto render_document(std::span<Expr const> roots, Dialect dialect = Dialect::lispb) const
        -> std::string;
    auto render_document(std::initializer_list<Expr> roots, Dialect dialect = Dialect::lispb) const
        -> std::string;

    CommonAtoms atoms;
  private:
    friend class FormBuilder;
    using Range = output_detail::Range;
    using TextId = output_detail::TextId;
    using ListId = output_detail::ListId;

    static auto handle(ExprKind kind, std::size_t index) -> Expr;
    auto text_expr(ExprKind kind, std::string_view value) -> Expr;
    auto text_expr(ExprKind kind, StringId value) -> Expr;
    auto text(TextId id) const -> std::string_view;
    auto save_children(std::span<Expr const> values) -> Range;
    auto children(Range range) const -> std::span<Expr const>;
    auto save_list(std::span<Expr const> header,
                   std::span<Expr const> values,
                   std::span<Range const> rows) -> Expr;
    static void append_string(std::string& output, std::string_view value, Dialect dialect);
    void append_values(std::string& output,
                       std::span<Expr const> values,
                       std::size_t depth,
                       Dialect dialect) const;
    void append(std::string& output, Expr value, std::size_t depth, Dialect dialect) const;

    Strings const& strings_;
    output_detail::Texts texts_;
    output_detail::Lists lists_;
    std::vector<Expr> children_;
    std::vector<Range> rows_;
    std::vector<Expr> quotes_;
};
}
