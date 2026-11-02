#pragma once

#include <codegen/ast/indices.h>
#include <codegen/ast/syntax.h>
#include <lispb/indices.h>

#include <vector>

namespace lispb::codegen::ir {

struct InstanceMembers {
    std::vector<CppTypeId> type;
    std::vector<StringId> name;
    std::vector<Syntax> initializer;
};

}
