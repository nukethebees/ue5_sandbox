#pragma once

#include <lispb/index.h>

namespace kernel_codegen::detail {

class Emissions;
class ExpandedVariants;
class Modules;
class Operands;
class TypeSets;
class Variants;

using EmissionId = lispb::Index<Emissions>;
using ExpandedVariantId = lispb::Index<ExpandedVariants>;
using ModuleId = lispb::Index<Modules>;
using OperandId = lispb::Index<Operands>;
using TypeSetId = lispb::Index<TypeSets, 0xffffffffU>;
using VariantId = lispb::Index<Variants>;

} // namespace kernel_codegen::detail
