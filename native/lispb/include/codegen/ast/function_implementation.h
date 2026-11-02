#pragma once

#include <codegen/ast/function_implementation_kind.h>
#include <codegen/ast/indices.h>
#include <codegen/ast/node_id.h>

#include <span>
#include <vector>

namespace lispb::codegen {

struct FunctionImplementation {
    FunctionImplementationKind kind{FunctionImplementationKind::none};
    std::vector<NodeId> body;
    std::vector<MemberInitializerId> initializers;
};

struct FunctionImplementationView {
    FunctionImplementationKind kind{FunctionImplementationKind::none};
    std::span<NodeId const> body;
    std::span<MemberInitializerId const> initializers;
};

}
