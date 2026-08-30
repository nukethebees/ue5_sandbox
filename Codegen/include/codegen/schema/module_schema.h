#pragma once

#include <codegen/schema/enum_module_schema.h>
#include <codegen/schema/facade_module_schema.h>
#include <codegen/schema/homogeneous_module_schema.h>
#include <codegen/schema/soa_module_schema.h>
#include <codegen/schema/umbrella_module_schema.h>
#include <codegen/schema/vector_module_schema.h>

#include <variant>

namespace codegen {

using ModuleSchema = std::variant<EnumModuleSchema,
                                  SoaModuleSchema,
                                  HomogeneousModuleSchema,
                                  VectorModuleSchema,
                                  FacadeModuleSchema,
                                  UmbrellaModuleSchema>;

} // namespace codegen
