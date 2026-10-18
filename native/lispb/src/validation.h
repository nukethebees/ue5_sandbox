#pragma once

#include <codegen/schema.h>
#include <codegen/schema/manifest.h>

namespace lispb::schema {
class TypeGraph;
}

namespace lispb::codegen::detail {

// Validate authored and registered domains before constructing semantic integer definitions.
void validate_integer_domains(Manifest const& manifest, lispb::schema::TypeGraph const& graph);
void validate_manifest(Manifest const& manifest, lispb::schema::TypeGraph const& graph);

} // namespace lispb::codegen::detail
