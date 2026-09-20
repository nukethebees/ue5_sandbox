#pragma once

#include <codegen/schema/module_settings.h>
#include <codegen/schema/tagged_union_schema.h>
#include <codegen/schema/union_schema.h>

#include <vector>

namespace codegen {

struct UnionModuleSchema {
    ModuleSettings settings;
    std::vector<UnionSchema> unions;
    std::vector<TaggedUnionSchema> tagged_unions;
};

} // namespace codegen
