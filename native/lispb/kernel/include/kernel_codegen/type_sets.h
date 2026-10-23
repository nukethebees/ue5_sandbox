#pragma once

#include <codegen/sexpr/handle_range.h>
#include <codegen/sexpr/source_span.h>
#include <kernel_codegen/indices.h>
#include <kernel_codegen/scalar_type.h>
#include <lispb/indices.h>

#include <cstddef>
#include <span>
#include <vector>

namespace kernel_codegen::detail {
class TypeSets {
  public:
    // Finish appending child ranges before adding another entry.
    auto add(lispb::StringId name, lispb::codegen::sexpr::SourceSpan span) -> TypeSetId;
    auto name(TypeSetId const id) const -> lispb::StringId { return name_[id.value]; }
    auto span(TypeSetId const id) const -> lispb::codegen::sexpr::SourceSpan {
        return span_[id.value];
    }
    auto types(TypeSetId id) const -> std::span<ScalarType const>;
    void append_types(TypeSetId id, ScalarType value);
    auto size() const -> std::size_t { return name_.size(); }
    void clear();
  private:
    std::vector<lispb::StringId> name_;
    std::vector<lispb::codegen::sexpr::SourceSpan> span_;
    std::vector<lispb::codegen::sexpr::HandleRange> types_ranges_;
    std::vector<ScalarType> types_;
};
}
