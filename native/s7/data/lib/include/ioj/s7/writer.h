#pragma once

#include <ioj/constrained_value.h>
#include <ioj/enum_traits.h>
#include <ioj/frequency.h>
#include <ioj/s7/source_format.h>
#include <ioj/vector.h>

#include <codegen/sexpr/form_builder.h>
#include <codegen/sexpr/sexpr_renderer.h>
#include <lispb/strings.h>

#include <algorithm>
#include <array>
#include <concepts>
#include <optional>
#include <string>
#include <type_traits>
#include <vector>

namespace ioj::s7 {
using lispb::codegen::sexpr::Expr;
using lispb::codegen::sexpr::SExprIR;

template <typename T>
struct TypeWriter;

template <typename T>
auto write(SExprIR& ir, T const& value, T const* base = nullptr) -> Expr {
    return TypeWriter<T>::write(ir, value, base);
}

template <typename T>
auto equivalent(T const& value, T const& base) -> bool {
    if constexpr (requires { TypeWriter<T>::equivalent(value, base); }) {
        return TypeWriter<T>::equivalent(value, base);
    } else {
        return value == base;
    }
}

template <typename Record, bool Positional = false>
class PropertyWriter {
  public:
    PropertyWriter(SExprIR& ir,
                   lispb::codegen::sexpr::FormBuilder& form,
                   Record const& source,
                   Record const* base)
        : ir_{ir}
        , form_{form}
        , source_{source}
        , base_{base} {}

    template <typename T>
    void operator()(std::string_view key, T Record::* member) const {
        auto const& value{source_.*member};
        if constexpr (Positional) {
            form_.add(s7::write(ir_, value));
        } else {
            if constexpr (requires { TypeWriter<T>::omitted(value); }) {
                if (!base_ && TypeWriter<T>::omitted(value)) {
                    return;
                }
            }
            auto const* base{base_ ? &(base_->*member) : nullptr};
            if (!base || !s7::equivalent(value, *base)) {
                form_.field(key, s7::write(ir_, value, base));
            }
        }
    }

  private:
    SExprIR& ir_;
    lispb::codegen::sexpr::FormBuilder& form_;
    Record const& source_;
    Record const* base_;
};

template <typename T, bool Positional = false>
struct RecordWriter {
    static auto write(SExprIR& ir, T const& value, T const* base) -> Expr {
        auto form{ir.form(TypeWriter<T>::tag())};
        PropertyWriter<T, Positional> writer{ir, form, value, base};
        TypeWriter<T>::properties(writer);
        return form.build();
    }

    static auto equivalent(T const& value, T const& base) -> bool {
        auto result{true};
        auto compare{[&](std::string_view, auto member) {
            result = result && s7::equivalent(value.*member, base.*member);
        }};
        TypeWriter<T>::properties(compare);
        return result;
    }
};

template <std::integral T>
struct TypeWriter<T> {
    static auto write(SExprIR& ir, T const value, T const*) -> Expr {
        if constexpr (std::same_as<T, bool>) {
            return ir.atom(value ? literals::true_value : literals::false_value);
        } else {
            return ir.atom(value);
        }
    }
};

template <std::floating_point T>
struct TypeWriter<T> {
    static auto write(SExprIR& ir, T const value, T const*) -> Expr {
        return ir.atom(format_real(value));
    }
};

template <typename T>
    requires std::is_enum_v<T>
struct TypeWriter<T> {
    static auto write(SExprIR& ir, T const value, T const*) -> Expr {
        return ir.quote(to_serialized_string(value));
    }
};

template <>
struct TypeWriter<std::string> {
    static auto write(SExprIR& ir, std::string const& value, std::string const*) -> Expr {
        return ir.string(value);
    }
};

template <typename T, ConstrainedValuePolicy Policy>
struct TypeWriter<ConstrainedValue<T, Policy>> {
    static auto write(SExprIR& ir,
                      ConstrainedValue<T, Policy> const& value,
                      ConstrainedValue<T, Policy> const*) -> Expr {
        return s7::write(ir, value.get());
    }
    static auto equivalent(ConstrainedValue<T, Policy> const& value,
                           ConstrainedValue<T, Policy> const& base) -> bool {
        return s7::equivalent(value.get(), base.get());
    }
};

template <typename T>
    requires requires(typename T::value_type value, T const object) {
        requires std::integral<typename T::value_type>;
        { T::is_valid_value(value) } -> std::same_as<bool>;
        { object.value() } -> std::same_as<typename T::value_type>;
        object.raw_value();
    }
struct TypeWriter<T> {
    static auto write(SExprIR& ir, T const value, T const*) -> Expr {
        return s7::write(ir, value.value());
    }
};

template <typename T>
struct TypeWriter<std::optional<T>> {
    static auto write(SExprIR& ir, std::optional<T> const& value, std::optional<T> const* base)
        -> Expr {
        return value ? s7::write(ir, *value, base && *base ? &**base : nullptr) : ir.atom(":none");
    }
    static auto equivalent(std::optional<T> const& value, std::optional<T> const& base) -> bool {
        return value.has_value() == base.has_value() && (!value || s7::equivalent(*value, *base));
    }
};

template <typename T>
struct TypeWriter<std::vector<T>> {
    static auto write(SExprIR& ir, std::vector<T> const& values, std::vector<T> const*) -> Expr {
        auto result{ir.form("list")};
        for (auto const& value : values) {
            result.add(s7::write(ir, value));
        }
        return result.build();
    }
    static auto equivalent(std::vector<T> const& value, std::vector<T> const& base) -> bool {
        return std::ranges::equal(
            value, base, [](T const& left, T const& right) { return s7::equivalent(left, right); });
    }
};

template <typename T, std::size_t N>
struct TypeWriter<std::array<T, N>> {
    static auto write(SExprIR& ir, std::array<T, N> const& values, std::array<T, N> const*)
        -> Expr {
        auto result{ir.form("list")};
        for (auto const& value : values) {
            result.add(s7::write(ir, value));
        }
        return result.build();
    }
    static auto equivalent(std::array<T, N> const& value, std::array<T, N> const& base) -> bool {
        return std::ranges::equal(
            value, base, [](T const& left, T const& right) { return s7::equivalent(left, right); });
    }
};

template <typename T, std::size_t N>
struct TypeWriter<Vector<T, N>> {
    static auto write(SExprIR& ir, Vector<T, N> const& value, Vector<T, N> const*) -> Expr {
        return ir.quote(
            ir.list(value.elements, [&](auto element) { return s7::write(ir, element); }));
    }
};

template <>
struct TypeWriter<Frequency> {
    static auto write(SExprIR& ir, Frequency const value, Frequency const*) -> Expr {
        return s7::write(ir, value.hz());
    }
};

template <typename T>
auto serialize(T const& value, T const* base = nullptr) -> std::string {
    lispb::Strings strings;
    SExprIR ir{strings};
    auto const expression{write(ir, value, base)};
    return lispb::codegen::sexpr::SExprRenderer{ir, lispb::codegen::sexpr::Dialect::scheme}.render(
        expression);
}

template <typename T>
auto serialize(T const& value, T const& base) -> std::string {
    return serialize(value, &base);
}
}
