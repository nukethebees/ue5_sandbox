#pragma once
#include <codegen/sexpr/sexpr_ir.h>

namespace lispb::codegen::sexpr {
class FormBuilder {
  public:
    auto field(std::string_view key, Expr value) -> FormBuilder&;
    template <std::integral T>
        requires (!std::same_as<T, bool>)
    auto field(std::string_view key, T value) -> FormBuilder& {
        return field(key, ir_.atom(value));
    }
    template <Serialized Key, typename Value>
    auto field(Key key, Value const& value) -> FormBuilder& {
        return field(to_serialized_string(key), value);
    }
    auto add(Expr child) -> FormBuilder&;
    // Publish the collected rows; all resulting handles belong to this builder's IR.
    auto build() -> Expr;
  private:
    friend class SExprIR;
    FormBuilder(SExprIR& ir, Expr head, std::initializer_list<Expr> positional);
    SExprIR& ir_;
    std::vector<Expr> header_;
    std::vector<Expr> children_;
    std::vector<output_detail::Range> rows_;
};

template <Serialized T>
auto SExprIR::form(T head, std::initializer_list<Expr> positional) -> FormBuilder {
    return form(to_serialized_string(head), positional);
}
}
