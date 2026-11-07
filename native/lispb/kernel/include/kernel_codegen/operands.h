#pragma once

#include <codegen/sexpr/handle_range.h>
#include <codegen/sexpr/source_span.h>
#include <kernel_codegen/indices.h>
#include <kernel_codegen/storage_kind.h>
#include <lispb/indices.h>

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace kernel_codegen::detail {
class Operands {
  public:
    // Finish appending child ranges before adding another entry.
    auto add(lispb::StringId name, lispb::codegen::sexpr::SourceSpan span, bool is_component)
        -> OperandId;
    auto name(OperandId const id) const -> lispb::StringId { return name_[id.value]; }
    auto span(OperandId const id) const -> lispb::codegen::sexpr::SourceSpan {
        return span_[id.value];
    }
    auto is_component(OperandId const id) const -> bool { return is_component_[id.value]; }
    auto storage(OperandId id) const -> std::span<StorageKind const>;
    void append_storage(OperandId id, StorageKind value);
    auto size() const -> std::size_t { return name_.size(); }
    void clear();
  private:
    std::vector<lispb::StringId> name_;
    std::vector<lispb::codegen::sexpr::SourceSpan> span_;
    std::vector<std::uint8_t> is_component_;
    std::vector<lispb::codegen::sexpr::HandleRange> storage_ranges_;
    std::vector<StorageKind> storage_;
};
}
