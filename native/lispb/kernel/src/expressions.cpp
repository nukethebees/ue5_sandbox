#include <kernel_codegen/expressions.h>

#include <codegen/sexpr/source_span.h>
#include <kernel_codegen/binary_operator.h>
#include <kernel_codegen/constant_kind.h>
#include <kernel_codegen/expression_id.h>
#include <kernel_codegen/expression_kind.h>
#include <kernel_codegen/indices.h>
#include <lispb/indices.h>
#include <lispb/strings.h>

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

namespace kernel_codegen::detail {

auto Expressions::operand(ExpressionId const id) const -> OperandId {
    assert(id.kind() == ExpressionKind::reference);
    return reference_operands_[id.index()];
}
auto Expressions::literal_id(ExpressionId const id) const -> lispb::StringId {
    assert(id.kind() == ExpressionKind::literal);
    return literal_spellings_[id.index()];
}
auto Expressions::literal(ExpressionId const id) const -> std::string_view {
    return strings_.view(literal_id(id));
}
auto Expressions::number(ExpressionId const id) const -> long double {
    assert(id.kind() == ExpressionKind::literal);
    return literal_numbers_[id.index()];
}
auto Expressions::constant(ExpressionId const id) const -> ConstantKind {
    assert(id.kind() == ExpressionKind::constant);
    return constant_kinds_[id.index()];
}
auto Expressions::operation(ExpressionId const id) const -> BinaryOperator {
    assert(id.kind() == ExpressionKind::binary);
    return binary_operators_[id.index()];
}
auto Expressions::left(ExpressionId const id) const -> ExpressionId {
    assert(id.kind() == ExpressionKind::binary);
    return binary_left_[id.index()];
}
auto Expressions::right(ExpressionId const id) const -> ExpressionId {
    assert(id.kind() == ExpressionKind::binary);
    return binary_right_[id.index()];
}
auto Expressions::span(ExpressionId const id) const -> lispb::codegen::sexpr::SourceSpan {
    switch (id.kind()) {
        case ExpressionKind::reference:
            return reference_spans_[id.index()];
        case ExpressionKind::literal:
            return literal_spans_[id.index()];
        case ExpressionKind::constant:
            return constant_spans_[id.index()];
        case ExpressionKind::binary:
            return binary_spans_[id.index()];
    }
    std::unreachable();
}

auto Expressions::reference(OperandId const operand, lispb::codegen::sexpr::SourceSpan const span)
    -> ExpressionId {
    auto const id{make_id(ExpressionKind::reference, reference_operands_.size())};
    reference_operands_.push_back(operand);
    reference_spans_.push_back(span);
    return id;
}
auto Expressions::literal(lispb::StringId const spelling,
                          long double const number,
                          lispb::codegen::sexpr::SourceSpan const span) -> ExpressionId {
    auto const id{make_id(ExpressionKind::literal, literal_spellings_.size())};
    literal_spellings_.push_back(spelling);
    literal_numbers_.push_back(number);
    literal_spans_.push_back(span);
    return id;
}
auto Expressions::constant(ConstantKind const kind, lispb::codegen::sexpr::SourceSpan const span)
    -> ExpressionId {
    auto const id{make_id(ExpressionKind::constant, constant_kinds_.size())};
    constant_kinds_.push_back(kind);
    constant_spans_.push_back(span);
    return id;
}
auto Expressions::binary(BinaryOperator const operation,
                         ExpressionId const left,
                         ExpressionId const right,
                         lispb::codegen::sexpr::SourceSpan const span) -> ExpressionId {
    auto const id{make_id(ExpressionKind::binary, binary_operators_.size())};
    binary_operators_.push_back(operation);
    binary_left_.push_back(left);
    binary_right_.push_back(right);
    binary_spans_.push_back(span);
    return id;
}
void Expressions::clear() {
    reference_operands_.clear();
    reference_spans_.clear();
    literal_spellings_.clear();
    literal_numbers_.clear();
    literal_spans_.clear();
    constant_kinds_.clear();
    constant_spans_.clear();
    binary_operators_.clear();
    binary_left_.clear();
    binary_right_.clear();
    binary_spans_.clear();
}
auto Expressions::make_id(ExpressionKind const kind, std::size_t const index) -> ExpressionId {
    assert(index <= 0x00ffffff);
    return {(static_cast<std::uint32_t>(kind) << 24) | static_cast<std::uint32_t>(index)};
}

}
