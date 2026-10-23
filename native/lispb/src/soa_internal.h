#pragma once

#include "lowered_soa.h"
#include "lowering_context.h"

#include <codegen/schema/declaration_handle.h>

namespace lispb::codegen::detail {
auto lower_single_allocation(LoweringContext& context, DeclarationHandle schema) -> LoweredSoa;
}
