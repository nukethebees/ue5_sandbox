#pragma once

#include <codegen/schema/declaration_handle.h>

namespace lispb::codegen {
class Declarations;

void order_enum_values(Declarations& declarations, DeclarationHandle enumeration);
}
