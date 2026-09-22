#pragma once

#include <codegen/schema/soa_module_schema.h>

#include <span>
#include <vector>

namespace codegen {

void validate_soa_allocator_variants(SoaBackend backend,
                                     std::span<SoaSchema const> schemas,
                                     std::span<SoaAllocatorVariant const> variants);
auto expand_soa_allocator_variants(SoaBackend backend,
                                   std::span<SoaSchema const> schemas,
                                   std::span<SoaAllocatorVariant const> variants)
    -> std::vector<SoaSchema>;

} // namespace codegen
