#pragma once

#include <codegen/sexpr/handle_range.h>
#include <codegen/sexpr/source_span.h>
#include <kernel_codegen/indices.h>
#include <kernel_codegen/operation_id.h>
#include <lispb/indices.h>

#include <cstddef>
#include <span>
#include <vector>

namespace kernel_codegen::detail {
class Modules {
  public:
    // Finish appending child ranges before adding another entry.
    auto add(lispb::StringId name, lispb::codegen::sexpr::SourceSpan span) -> ModuleId;
    auto name(ModuleId const id) const -> lispb::StringId { return name_[id.value]; }
    auto span(ModuleId const id) const -> lispb::codegen::sexpr::SourceSpan {
        return span_[id.value];
    }
    auto type_sets(ModuleId id) const -> std::span<TypeSetId const>;
    void append_type_sets(ModuleId id, TypeSetId value);
    auto operations(ModuleId id) const -> std::span<OperationId const>;
    void append_operations(ModuleId id, OperationId value);
    auto emissions(ModuleId id) const -> std::span<EmissionId const>;
    void append_emissions(ModuleId id, EmissionId value);
    auto expanded_variants(ModuleId const id) const -> lispb::codegen::sexpr::HandleRange {
        return expanded_variants_[id.value];
    }
    auto expanded_variants(ModuleId const id) -> lispb::codegen::sexpr::HandleRange& {
        return expanded_variants_[id.value];
    }
    auto size() const -> std::size_t { return name_.size(); }
    void clear();
  private:
    std::vector<lispb::codegen::sexpr::HandleRange> expanded_variants_;
    std::vector<lispb::StringId> name_;
    std::vector<lispb::codegen::sexpr::SourceSpan> span_;
    std::vector<lispb::codegen::sexpr::HandleRange> type_sets_ranges_;
    std::vector<TypeSetId> type_sets_;
    std::vector<lispb::codegen::sexpr::HandleRange> operations_ranges_;
    std::vector<OperationId> operations_;
    std::vector<lispb::codegen::sexpr::HandleRange> emissions_ranges_;
    std::vector<EmissionId> emissions_;
};
}
