#include "reference_evaluator.h"

#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace kernel_codegen::detail {
namespace {

template <typename T>
auto fixture_value(std::size_t const operand_index, std::size_t const element_index) -> T {
    if constexpr (std::is_same_v<T, std::uint32_t>) {
        constexpr std::array<std::uint32_t, 7> values{1, 2, 3, 4, 5, 6, 7};
        return values[(operand_index * 3 + element_index * 5) % values.size()];
    } else if constexpr (std::is_same_v<T, std::int32_t>) {
        constexpr std::array<std::int32_t, 8> values{1, -2, 3, -4, 5, -6, 7, -8};
        return values[(operand_index * 3 + element_index * 5) % values.size()];
    } else {
        constexpr std::array<long double, 12> values{
            1.0L, -2.0L, 0.25L, -0.5L, 3.0L, -4.0L,
            1.0e10L, -1.0e10L, 1.0e-10L, -1.0e-10L, 7.0L, -8.0L};
        return static_cast<T>(values[(operand_index * 3 + element_index * 5) % values.size()]);
    }
}

template <typename T>
auto parse_literal(std::string const& spelling) -> T {
    auto const value{std::stold(spelling)};
    return static_cast<T>(value);
}

template <typename T>
auto apply_integral(std::string const& operation, T const lhs, T const rhs) -> T {
    if (operation == "/") {
        if (rhs == 0) {
            throw std::invalid_argument{"generated reference fixture divides an integer by zero"};
        }
        if constexpr (std::is_signed_v<T>) {
            if (lhs == std::numeric_limits<T>::min() && rhs == T{-1}) {
                throw std::invalid_argument{"generated reference fixture overflows signed division"};
            }
        }
        return static_cast<T>(lhs / rhs);
    }

    if constexpr (std::is_unsigned_v<T>) {
        if (operation == "+") {
            return static_cast<T>(lhs + rhs);
        }
        if (operation == "-") {
            return static_cast<T>(lhs - rhs);
        }
        return static_cast<T>(lhs * rhs);
    } else {
        std::int64_t result{};
        if (operation == "+") {
            result = static_cast<std::int64_t>(lhs) + static_cast<std::int64_t>(rhs);
        } else if (operation == "-") {
            result = static_cast<std::int64_t>(lhs) - static_cast<std::int64_t>(rhs);
        } else {
            result = static_cast<std::int64_t>(lhs) * static_cast<std::int64_t>(rhs);
        }
        if (result < std::numeric_limits<T>::min() || result > std::numeric_limits<T>::max()) {
            throw std::invalid_argument{"generated reference fixture overflows signed arithmetic"};
        }
        return static_cast<T>(result);
    }
}

template <typename T>
auto evaluate(Expression const& expression,
              ExpandedVariant const& expanded,
              std::vector<std::vector<T>> const& operands,
              std::size_t const element_index) -> T {
    if (expression.kind == ExpressionKind::literal) {
        return parse_literal<T>(expression.value);
    }
    if (expression.kind == ExpressionKind::reference) {
        auto const found{std::ranges::find_if(
            expanded.operation->operands,
            [&](auto const& operand) { return operand.name == expression.value; })};
        auto const operand_index{
            static_cast<std::size_t>(found - expanded.operation->operands.begin())};
        auto const value_index{expanded.storage[operand_index] == StorageKind::array
                                   ? element_index
                                   : std::size_t{0}};
        return operands[operand_index][value_index];
    }
    if (expression.kind == ExpressionKind::constant) {
        if constexpr (std::is_floating_point_v<T>) {
            if (expression.constant == ConstantKind::nan) {
                return std::numeric_limits<T>::quiet_NaN();
            }
            if (expression.constant == ConstantKind::infinity) {
                return std::numeric_limits<T>::infinity();
            }
            return -std::numeric_limits<T>::infinity();
        } else {
            throw std::logic_error{"integral kernel expression contains a named constant"};
        }
    }

    auto const lhs{evaluate<T>(expression.arguments[0], expanded, operands, element_index)};
    auto const rhs{evaluate<T>(expression.arguments[1], expanded, operands, element_index)};
    if constexpr (std::is_integral_v<T>) {
        return apply_integral(expression.value, lhs, rhs);
    } else {
        if (expression.value == "+") {
            return static_cast<T>(lhs + rhs);
        }
        if (expression.value == "-") {
            return static_cast<T>(lhs - rhs);
        }
        if (expression.value == "*") {
            return static_cast<T>(lhs * rhs);
        }
        return static_cast<T>(lhs / rhs);
    }
}

template <typename T>
auto render_value(T const value) -> std::string {
    auto const type{standard_type(std::is_same_v<T, std::int32_t>    ? "int32"
                                  : std::is_same_v<T, std::uint32_t> ? "uint32"
                                  : std::is_same_v<T, float>         ? "float"
                                                                    : "double")};
    if constexpr (std::is_integral_v<T>) {
        return "static_cast<" + type + ">(" + std::to_string(value) + ")";
    } else {
        if (std::isnan(value)) {
            return "std::numeric_limits<" + type + ">::quiet_NaN()";
        }
        if (std::isinf(value)) {
            return (std::signbit(value) ? "-" : "") +
                   ("std::numeric_limits<" + type + ">::infinity()");
        }
        if (value == T{} && std::signbit(value)) {
            return "static_cast<" + type + ">(-0.0)";
        }

        std::array<char, 64> buffer{};
        auto const [end, error]{std::to_chars(buffer.data(),
                                              buffer.data() + buffer.size(),
                                              value,
                                              std::chars_format::general,
                                              std::numeric_limits<T>::max_digits10)};
        if (error != std::errc{}) {
            throw std::runtime_error{"could not render generated reference value"};
        }
        return "static_cast<" + type + ">(" + std::string{buffer.data(), end} + ")";
    }
}

template <typename T>
auto make_typed_fixture(ExpandedVariant const& expanded, std::size_t const count)
    -> ReferenceFixture {
    std::vector<std::vector<T>> values;
    ReferenceFixture result;
    for (std::size_t operand_index{}; operand_index < expanded.operation->operands.size();
         ++operand_index) {
        auto const value_count{expanded.storage[operand_index] == StorageKind::array
                                   ? count
                                   : std::size_t{1}};
        std::vector<T> operand_values;
        std::vector<std::string> operand_literals;
        operand_values.reserve(value_count);
        operand_literals.reserve(value_count);
        for (std::size_t element_index{}; element_index < value_count; ++element_index) {
            auto const value{fixture_value<T>(operand_index, element_index)};
            operand_values.push_back(value);
            operand_literals.push_back(render_value(value));
        }
        values.push_back(std::move(operand_values));
        result.operands.push_back(std::move(operand_literals));
    }

    result.expected.reserve(count);
    for (std::size_t element_index{}; element_index < count; ++element_index) {
        result.expected.push_back(render_value(
            evaluate<T>(expanded.operation->expression, expanded, values, element_index)));
    }
    return result;
}

}

auto make_reference_fixture(ExpandedVariant const& expanded, std::size_t const count)
    -> ReferenceFixture {
    if (expanded.type == "int32") {
        return make_typed_fixture<std::int32_t>(expanded, count);
    }
    if (expanded.type == "uint32") {
        return make_typed_fixture<std::uint32_t>(expanded, count);
    }
    if (expanded.type == "float") {
        return make_typed_fixture<float>(expanded, count);
    }
    if (expanded.type == "double") {
        return make_typed_fixture<double>(expanded, count);
    }
    throw std::invalid_argument{"unsupported generated reference type '" + expanded.type + "'"};
}

}
