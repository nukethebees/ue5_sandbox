#include "reference_evaluator.h"

#include "lowering.h"

#include <kernel_codegen/binary_operator.h>
#include <kernel_codegen/constant_kind.h>
#include <kernel_codegen/expression_id.h>
#include <kernel_codegen/expression_kind.h>
#include <kernel_codegen/scalar_type.h>
#include <kernel_codegen/storage_kind.h>

#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>
#include <system_error>
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
        constexpr std::array<long double, 12> values{1.0L,
                                                     -2.0L,
                                                     0.25L,
                                                     -0.5L,
                                                     3.0L,
                                                     -4.0L,
                                                     1.0e10L,
                                                     -1.0e10L,
                                                     1.0e-10L,
                                                     -1.0e-10L,
                                                     7.0L,
                                                     -8.0L};
        return static_cast<T>(values[(operand_index * 3 + element_index * 5) % values.size()]);
    }
}

template <typename T>
auto apply_integral(BinaryOperator const operation, T const lhs, T const rhs) -> T {
    if (operation == BinaryOperator::divide) {
        if (rhs == 0) {
            throw std::invalid_argument{"generated reference fixture divides an integer by zero"};
        }
        if constexpr (std::is_signed_v<T>) {
            if (lhs == std::numeric_limits<T>::min() && rhs == T{-1}) {
                throw std::invalid_argument{
                    "generated reference fixture overflows signed division"};
            }
        }
        return static_cast<T>(lhs / rhs);
    }

    if constexpr (std::is_unsigned_v<T>) {
        if (operation == BinaryOperator::add) {
            return static_cast<T>(lhs + rhs);
        }
        if (operation == BinaryOperator::subtract) {
            return static_cast<T>(lhs - rhs);
        }
        return static_cast<T>(lhs * rhs);
    } else {
        std::int64_t result{};
        if (operation == BinaryOperator::add) {
            result = static_cast<std::int64_t>(lhs) + static_cast<std::int64_t>(rhs);
        } else if (operation == BinaryOperator::subtract) {
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
auto evaluate(ExpressionId const expression,
              Document const& document,
              ExpandedVariantId const id,
              std::size_t const element_index) -> T {
    auto const& expanded{document.expanded_variants};
    if (expression.kind() == ExpressionKind::literal) {
        return static_cast<T>(document.expressions.number(expression));
    }
    if (expression.kind() == ExpressionKind::reference) {
        auto const operand_index{document.operand_index(expanded.operation(id),
                                                        document.expressions.operand(expression))};
        auto const value_index{expanded.storage(id)[operand_index] == StorageKind::array
                                   ? element_index
                                   : std::size_t{0}};
        return fixture_value<T>(operand_index, value_index);
    }
    if (expression.kind() == ExpressionKind::constant) {
        if constexpr (std::is_floating_point_v<T>) {
            if (document.expressions.constant(expression) == ConstantKind::nan) {
                return std::numeric_limits<T>::quiet_NaN();
            }
            if (document.expressions.constant(expression) == ConstantKind::infinity) {
                return std::numeric_limits<T>::infinity();
            }
            return -std::numeric_limits<T>::infinity();
        } else {
            throw std::logic_error{"integral kernel expression contains a named constant"};
        }
    }

    auto const lhs{evaluate<T>(document.expressions.left(expression), document, id, element_index)};
    auto const rhs{
        evaluate<T>(document.expressions.right(expression), document, id, element_index)};
    if constexpr (std::is_integral_v<T>) {
        return apply_integral(document.expressions.operation(expression), lhs, rhs);
    } else {
        if (document.expressions.operation(expression) == BinaryOperator::add) {
            return static_cast<T>(lhs + rhs);
        }
        if (document.expressions.operation(expression) == BinaryOperator::subtract) {
            return static_cast<T>(lhs - rhs);
        }
        if (document.expressions.operation(expression) == BinaryOperator::multiply) {
            return static_cast<T>(lhs * rhs);
        }
        return static_cast<T>(lhs / rhs);
    }
}

template <typename T>
auto render_value(T const value) -> ReferenceValue {
    if constexpr (std::is_integral_v<T>) {
        return std::to_string(value);
    } else {
        if (std::isnan(value)) {
            return ConstantKind::nan;
        }
        if (std::isinf(value)) {
            return std::signbit(value) ? ConstantKind::negative_infinity : ConstantKind::infinity;
        }
        if (value == T{} && std::signbit(value)) {
            return std::string{"-0.0"};
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
        return std::string{buffer.data(), end};
    }
}

template <typename T>
auto make_typed_fixture(Document const& document,
                        ExpandedVariantId const id,
                        std::size_t const count) -> ReferenceFixture {
    auto const& expanded{document.expanded_variants};
    ReferenceFixture result;
    for (std::size_t operand_index{};
         operand_index < document.operations.operands(expanded.operation(id)).size();
         ++operand_index) {
        auto const value_count{
            expanded.storage(id)[operand_index] == StorageKind::array ? count : std::size_t{1}};
        std::vector<ReferenceValue> operand_literals;
        operand_literals.reserve(value_count);
        for (std::size_t element_index{}; element_index < value_count; ++element_index) {
            auto const value{fixture_value<T>(operand_index, element_index)};
            operand_literals.push_back(render_value(value));
        }
        result.operands.push_back(std::move(operand_literals));
    }

    result.expected.reserve(count);
    for (std::size_t element_index{}; element_index < count; ++element_index) {
        result.expected.push_back(render_value(evaluate<T>(
            document.operations.expression(expanded.operation(id)), document, id, element_index)));
    }
    return result;
}

}

auto make_reference_fixture(Document const& document,
                            ExpandedVariantId const id,
                            std::size_t const count) -> ReferenceFixture {
    auto const& expanded{document.expanded_variants};
    if (expanded.type(id) == ScalarType::int32) {
        return make_typed_fixture<std::int32_t>(document, id, count);
    }
    if (expanded.type(id) == ScalarType::uint32) {
        return make_typed_fixture<std::uint32_t>(document, id, count);
    }
    if (expanded.type(id) == ScalarType::float32) {
        return make_typed_fixture<float>(document, id, count);
    }
    if (expanded.type(id) == ScalarType::float64) {
        return make_typed_fixture<double>(document, id, count);
    }
    std::unreachable();
}

}
