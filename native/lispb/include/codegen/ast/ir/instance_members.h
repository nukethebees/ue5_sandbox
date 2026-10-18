#pragma once

#include <codegen/ast/expr.h>
#include <codegen/ast/indices.h>
#include <lispb/indices.h>

#include <vector>

namespace lispb::codegen::ir {

struct InstanceMembers {
    std::vector<CppTypeId> type;
    std::vector<IdentifierId> name;
    std::vector<Expr> initializer;
};

}
