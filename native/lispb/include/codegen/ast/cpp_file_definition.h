#pragma once

#include <codegen/ast/cpp_file_definition_flags.h>
#include <codegen/ast/node_id.h>
#include <lispb/indices.h>

#include <vector>

namespace lispb::codegen {

struct CppFileDefinition {
    StringId path{};
    StringId source{};
    std::vector<NodeId> nodes;
    CppFileDefinitionFlags flags{};
    std::vector<StringId> prologue;
    std::vector<StringId> epilogue;
    std::vector<StringId> include_order;
};

}
