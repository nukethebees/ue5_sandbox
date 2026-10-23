#pragma once

#include <codegen/sexpr/source_span.h>
#include <kernel_codegen/indices.h>
#include <kernel_codegen/variant_kind.h>
#include <lispb/indices.h>

#include <cstddef>
#include <optional>
#include <vector>

namespace kernel_codegen::detail {
class Variants {
  public:
    auto add(VariantKind kind,
             lispb::StringId name,
             std::optional<OperandId> target,
             lispb::codegen::sexpr::SourceSpan span) -> VariantId;
    auto kind(VariantId const id) const -> VariantKind { return kind_[id.value]; }
    auto name(VariantId const id) const -> lispb::StringId { return name_[id.value]; }
    auto target(VariantId const id) const -> std::optional<OperandId> { return target_[id.value]; }
    auto span(VariantId const id) const -> lispb::codegen::sexpr::SourceSpan {
        return span_[id.value];
    }
    auto size() const -> std::size_t { return kind_.size(); }
    void clear();
  private:
    std::vector<VariantKind> kind_;
    std::vector<lispb::StringId> name_;
    std::vector<std::optional<OperandId>> target_;
    std::vector<lispb::codegen::sexpr::SourceSpan> span_;
};
}
