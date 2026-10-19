#pragma once
#include <codegen/sexpr/sexpr_ir.h>

#include <functional>
#include <optional>

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
    template <typename Key>
    auto flag(Key key, bool enabled) -> FormBuilder& {
        if (enabled) {
            field(key, ir_.atoms.true_value);
        }
        return *this;
    }
    template <typename Key, typename Value, typename Projection = std::identity>
    auto optional_field(Key key, std::optional<Value> const& value, Projection project = {})
        -> FormBuilder& {
        if (value) {
            field(key, project(*value));
        }
        return *this;
    }
    // Omit empty lists; use field(key, ir.list(...)) when an empty list is significant.
    template <typename Key, std::ranges::forward_range Range, typename Projection>
    auto list_field(Key key, Range const& values, Projection project) -> FormBuilder& {
        if (!std::ranges::empty(values)) {
            field(key, ir_.list(values, project));
        }
        return *this;
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
