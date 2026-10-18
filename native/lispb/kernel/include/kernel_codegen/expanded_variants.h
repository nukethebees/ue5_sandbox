#pragma once

#include <codegen/sexpr/handle_range.h>
#include <kernel_codegen/indices.h>
#include <kernel_codegen/operation_id.h>
#include <kernel_codegen/scalar_type.h>
#include <kernel_codegen/storage_kind.h>

#include <cstddef>
#include <span>
#include <vector>

namespace kernel_codegen::detail {
struct Document;

class ExpandedVariants {
  public:
    auto expand(Document const& document, ModuleId module) -> lispb::codegen::sexpr::HandleRange;
    void clear();
    auto size() const -> std::size_t { return operations_.size(); }
    auto operation(ExpandedVariantId const id) const -> OperationId {
        return operations_[id.value];
    }
    auto variant(ExpandedVariantId const id) const -> VariantId { return variants_[id.value]; }
    auto type(ExpandedVariantId const id) const -> ScalarType { return types_[id.value]; }
    auto storage(ExpandedVariantId id) const -> std::span<StorageKind const>;
  private:
    void expand_storage(Document const& document,
                        OperationId operation,
                        VariantId variant,
                        std::size_t operand_index,
                        std::vector<StorageKind>& storage);
    std::vector<OperationId> operations_;
    std::vector<VariantId> variants_;
    std::vector<ScalarType> types_;
    std::vector<lispb::codegen::sexpr::HandleRange> storage_ranges_;
    std::vector<StorageKind> storage_;
};

}
