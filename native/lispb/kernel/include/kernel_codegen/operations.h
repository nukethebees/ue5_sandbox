#pragma once

#include <codegen/sexpr/handle_range.h>
#include <codegen/sexpr/source_span.h>
#include <kernel_codegen/aliasing.h>
#include <kernel_codegen/expression_id.h>
#include <kernel_codegen/indices.h>
#include <kernel_codegen/operation_id.h>
#include <lispb/indices.h>

#include <cstddef>
#include <span>
#include <vector>

namespace kernel_codegen::detail {
class Operations {
  public:
    // Finish appending child ranges before adding another entry.
    auto add(OperationKind kind, lispb::StringId name, lispb::codegen::sexpr::SourceSpan span)
        -> OperationId;
    auto name(OperationId const id) const -> lispb::StringId { return name_[id.index()]; }
    auto span(OperationId const id) const -> lispb::codegen::sexpr::SourceSpan {
        return span_[id.index()];
    }
    auto type_set(OperationId const id) const -> TypeSetId { return type_set_[id.index()]; }
    auto type_set(OperationId const id) -> TypeSetId& { return type_set_[id.index()]; }
    auto output(OperationId const id) const -> lispb::StringId { return output_[id.index()]; }
    auto output(OperationId const id) -> lispb::StringId& { return output_[id.index()]; }
    auto expression(OperationId const id) const -> ExpressionId { return expression_[id.index()]; }
    auto expression(OperationId const id) -> ExpressionId& { return expression_[id.index()]; }
    auto aliasing(OperationId const id) const -> Aliasing { return aliasing_[id.index()]; }
    auto aliasing(OperationId const id) -> Aliasing& { return aliasing_[id.index()]; }
    auto operands(OperationId id) const -> std::span<OperandId const>;
    void append_operands(OperationId id, OperandId value);
    auto components(OperationId id) const -> std::span<lispb::StringId const>;
    void append_components(OperationId id, lispb::StringId value);
    auto variants(OperationId id) const -> std::span<VariantId const>;
    void append_variants(OperationId id, VariantId value);
    auto size() const -> std::size_t { return name_.size(); }
    void clear();
  private:
    std::vector<lispb::StringId> name_;
    std::vector<lispb::codegen::sexpr::SourceSpan> span_;
    std::vector<TypeSetId> type_set_;
    std::vector<lispb::StringId> output_;
    std::vector<ExpressionId> expression_;
    std::vector<Aliasing> aliasing_;
    std::vector<lispb::codegen::sexpr::HandleRange> operands_ranges_;
    std::vector<OperandId> operands_;
    std::vector<lispb::codegen::sexpr::HandleRange> components_ranges_;
    std::vector<lispb::StringId> components_;
    std::vector<lispb::codegen::sexpr::HandleRange> variants_ranges_;
    std::vector<VariantId> variants_;
};
}
