#pragma once

#include <codegen/schema/soa_backend.h>
#include <codegen/schema/soa_schema.h>

#include <span>
#include <vector>

namespace codegen {

struct SoaAllocatorVariant {
    std::string prefix;
    TypeRef allocator;
};

void validate_soa_allocator_variants(SoaBackend backend,
                                     std::span<SoaSchema const> schemas,
                                     std::span<SoaAllocatorVariant const> variants);
auto expand_soa_allocator_variants(SoaBackend backend,
                                   std::span<SoaSchema const> schemas,
                                   std::span<SoaAllocatorVariant const> variants)
    -> std::vector<SoaSchema>;

} // namespace codegen
