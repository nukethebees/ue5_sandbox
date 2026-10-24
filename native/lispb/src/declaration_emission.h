#pragma once

#include <codegen/ast/indices.h>
#include <codegen/ast/nodes.h>
#include <lispb/indices.h>

#include <optional>
#include <string>
#include <vector>

namespace lispb::codegen::detail {

struct DeclarationEmission {
    Nodes header_prefix;
    Nodes header_generated_include;
    Nodes header;
    Nodes header_after;
    Nodes source;
    Nodes header_global;
    Nodes header_tail;
    Nodes source_global;
    Nodes source_tail;
    Nodes reader_declarations;
    std::vector<StringId> reader_includes;
    std::vector<ModuleId> additional_modules;
    std::optional<std::string> tail_namespace;
    bool source_dependencies{true};
    bool format_generated{};
};

}
