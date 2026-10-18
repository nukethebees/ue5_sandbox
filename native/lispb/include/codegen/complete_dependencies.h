#pragma once

#include <lispb/schema/indices.h>

#include <cstdint>
#include <span>
#include <vector>

namespace lispb::schema {
class TypeGraph;
}

namespace lispb::codegen::detail {

struct CompleteDependencies {
    std::vector<std::uint32_t> offsets;
    std::vector<schema::TypeId> targets;

    auto of(schema::TypeId type) const -> std::span<schema::TypeId const>;
};

auto is_record_signature(schema::TypeUseId use, schema::TypeGraph const& graph) -> bool;
auto complete_dependencies(schema::TypeGraph const& graph) -> CompleteDependencies;
void validate_complete_definition_cycles(schema::TypeGraph const& graph,
                                         CompleteDependencies const& dependencies);

}
