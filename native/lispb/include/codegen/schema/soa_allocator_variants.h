#pragma once

#include <codegen/schema/indices.h>
#include <codegen/schema/schema_range.h>
#include <codegen/schema/soa_backend.h>
#include <codegen/schema/soa_schemas.h>

#include <span>
#include <vector>

namespace lispb::codegen {

class Declarations;

void validate_soa_allocator_variants(Declarations& declarations,
                                     SoaBackend backend,
                                     std::span<DeclarationHandle const> schemas,
                                     SchemaRange<SoaAllocatorVariantId> variants);
// Require validated variants; the resulting records reuse all authored member payloads.
auto expand_soa_allocator_variants(Declarations& declarations,
                                   std::span<DeclarationHandle const> schemas,
                                   SchemaRange<SoaAllocatorVariantId> variants)
    -> std::vector<DeclarationHandle>;

} // namespace lispb::codegen
