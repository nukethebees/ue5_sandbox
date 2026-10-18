#pragma once

#include <lispb/indices.h>
#include <material_gen/Diagnostic.h>
#include <material_gen/MaterialSettings.h>
#include <material_gen/NamedNodes.h>
#include <material_gen/Nodes.h>
#include <material_gen/Parameters.h>

#include <string_view>
#include <vector>
namespace lispb {
class Strings;
}

namespace material_synth {

struct MaterialIR {
    explicit MaterialIR(lispb::Strings& storage)
        : strings{&storage} {}

    // The compiler or decoded artifact owns the backing text for this graph.
    lispb::Strings* strings;
    MaterialSettings settings;
    Parameters parameters;
    Nodes nodes;
    NamedNodes bindings;
    NamedNodes outputs;
    std::vector<lispb::StringId> texture_dependencies;

    auto text(lispb::StringId id) const -> std::string_view;
    auto intern(std::string_view value) -> lispb::StringId;
};

auto validate(MaterialIR const& material) -> std::vector<Diagnostic>;

}
