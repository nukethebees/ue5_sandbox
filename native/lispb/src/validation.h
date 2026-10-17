#pragma once

#include <codegen/schema.h>
#include <codegen/schema/manifest.h>

namespace codegen::detail {

void validate_manifest(Manifest const& manifest);

} // namespace codegen::detail
