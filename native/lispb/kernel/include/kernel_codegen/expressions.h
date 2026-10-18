#pragma once

#include <codegen/sexpr/source_span.h>
#include <kernel_codegen/binary_operator.h>
#include <kernel_codegen/constant_kind.h>
#include <kernel_codegen/expression_id.h>
#include <kernel_codegen/expression_kind.h>
#include <kernel_codegen/indices.h>
#include <lispb/indices.h>

#include <cstddef>
#include <string_view>
#include <vector>
namespace lispb {
class Strings;
}

namespace kernel_codegen::detail {

class Expressions {
  public:
    explicit Expressions(lispb::Strings& strings)
        : strings_{strings} {}
    auto strings() const -> lispb::Strings const& { return strings_; }
    auto operand(ExpressionId id) const -> OperandId;
    auto literal(ExpressionId id) const -> std::string_view;
    auto literal_id(ExpressionId id) const -> lispb::StringId;
    auto number(ExpressionId id) const -> long double;
    auto constant(ExpressionId id) const -> ConstantKind;
    auto operation(ExpressionId id) const -> BinaryOperator;
    auto left(ExpressionId id) const -> ExpressionId;
    auto right(ExpressionId id) const -> ExpressionId;
    auto span(ExpressionId id) const -> lispb::codegen::sexpr::SourceSpan;
    auto reference(OperandId const operand, lispb::codegen::sexpr::SourceSpan const span)
        -> ExpressionId;
    auto literal(lispb::StringId const spelling,
                 long double const number,
                 lispb::codegen::sexpr::SourceSpan const span) -> ExpressionId;
    auto constant(ConstantKind const kind, lispb::codegen::sexpr::SourceSpan const span)
        -> ExpressionId;
    auto binary(BinaryOperator const operation,
                ExpressionId const left,
                ExpressionId const right,
                lispb::codegen::sexpr::SourceSpan const span) -> ExpressionId;
    void clear();
  private:
    static auto make_id(ExpressionKind const kind, std::size_t const index) -> ExpressionId;
    lispb::Strings& strings_;
    std::vector<OperandId> reference_operands_;
    std::vector<lispb::codegen::sexpr::SourceSpan> reference_spans_;
    std::vector<lispb::StringId> literal_spellings_;
    std::vector<long double> literal_numbers_;
    std::vector<lispb::codegen::sexpr::SourceSpan> literal_spans_;
    std::vector<ConstantKind> constant_kinds_;
    std::vector<lispb::codegen::sexpr::SourceSpan> constant_spans_;
    std::vector<BinaryOperator> binary_operators_;
    std::vector<ExpressionId> binary_left_;
    std::vector<ExpressionId> binary_right_;
    std::vector<lispb::codegen::sexpr::SourceSpan> binary_spans_;
};

}
