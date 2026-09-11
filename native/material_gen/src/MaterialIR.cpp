#include <material_gen/MaterialIR.h>

#include <algorithm>
#include <cmath>
#include <set>

namespace material_synth {
namespace {

void report(std::vector<Diagnostic>& diagnostics,
            codegen::sexpr::SourceSpan const& span,
            std::string message) {
    diagnostics.push_back({span.path, span.line, span.column, std::move(message)});
}

auto valid_handle(MaterialIR const& material, NodeHandle const handle) -> bool {
    return handle.index < material.nodes.size();
}

auto promoted(ValueType const left, ValueType const right) -> ValueType {
    if (!is_numeric(left) || !is_numeric(right)) {
        return ValueType::invalid;
    }
    if (left == right) {
        return left;
    }
    if (left == ValueType::float1) {
        return right;
    }
    if (right == ValueType::float1) {
        return left;
    }
    return ValueType::invalid;
}

}

auto is_numeric(ValueType const type) -> bool {
    return type >= ValueType::float1 && type <= ValueType::float4;
}

auto component_count(ValueType const type) -> std::size_t {
    if (!is_numeric(type)) {
        return 0;
    }
    return static_cast<std::size_t>(type) - static_cast<std::size_t>(ValueType::float1) + 1;
}

auto validate(MaterialIR const& material) -> std::vector<Diagnostic> {
    std::vector<Diagnostic> diagnostics;
    std::set<std::string> names;

    auto validate_named{[&](NamedNode const& named, std::string_view const category) {
        if (!names.insert(named.name).second) {
            report(diagnostics,
                   named.span,
                   "duplicate " + std::string{category} + " name '" + named.name + "'");
        }
        if (!valid_handle(material, named.node)) {
            report(diagnostics, named.span, "invalid node handle for '" + named.name + "'");
        }
    }};

    for (std::size_t index{}; index < material.parameters.size(); ++index) {
        auto const& parameter{material.parameters[index]};
        if (!names.insert(parameter.name).second) {
            report(
                diagnostics, parameter.span, "duplicate parameter name '" + parameter.name + "'");
        }
        if (!valid_handle(material, parameter.node) ||
            material.nodes[parameter.node.index].kind != NodeKind::parameter ||
            material.nodes[parameter.node.index].parameter_index != index ||
            material.nodes[parameter.node.index].type != parameter.type) {
            report(diagnostics,
                   parameter.span,
                   "malformed parameter node for '" + parameter.name + "'");
        }
    }
    for (auto const& binding : material.bindings) {
        validate_named(binding, "binding");
    }

    names.clear();
    for (auto const& output : material.outputs) {
        validate_named(output, "output");
    }

    auto const node_count{material.nodes.size()};
    for (std::size_t index{}; index < node_count; ++index) {
        auto const& node{material.nodes[index]};
        for (auto const input : node.inputs) {
            if (!valid_handle(material, input) || input.index >= index) {
                report(diagnostics, node.span, "node input must reference an earlier valid node");
            }
        }
        if (node.kind == NodeKind::constant) {
            if (!is_numeric(node.type) || node.component_count != component_count(node.type) ||
                !node.inputs.empty() ||
                !std::all_of(node.constant.begin(),
                             node.constant.begin() + node.component_count,
                             [](double const value) { return std::isfinite(value); })) {
                report(diagnostics, node.span, "malformed constant node");
            }
        } else if (node.kind == NodeKind::parameter) {
            if (node.parameter_index >= material.parameters.size() || !node.inputs.empty() ||
                (node.parameter_index < material.parameters.size() &&
                 material.parameters[node.parameter_index].type != node.type)) {
                report(diagnostics, node.span, "malformed parameter node");
            }
        } else if (node.kind == NodeKind::texture_coordinate) {
            if (node.type != ValueType::float2 || node.coordinate_index > 7 ||
                !node.inputs.empty()) {
                report(diagnostics, node.span, "malformed texture-coordinate node");
            }
        } else if (node.kind >= NodeKind::add && node.kind <= NodeKind::divide) {
            if (node.inputs.size() != 2 || !valid_handle(material, node.inputs[0]) ||
                !valid_handle(material, node.inputs[1]) ||
                promoted(material.nodes[node.inputs[0].index].type,
                         material.nodes[node.inputs[1].index].type) != node.type) {
                report(diagnostics, node.span, "malformed arithmetic node");
            }
        } else if (node.kind == NodeKind::lerp) {
            if (node.inputs.size() != 3 || !valid_handle(material, node.inputs[0]) ||
                !valid_handle(material, node.inputs[1]) ||
                !valid_handle(material, node.inputs[2]) ||
                (valid_handle(material, node.inputs[0]) && valid_handle(material, node.inputs[1]) &&
                 promoted(material.nodes[node.inputs[0].index].type,
                          material.nodes[node.inputs[1].index].type) != node.type) ||
                (valid_handle(material, node.inputs[2]) &&
                 material.nodes[node.inputs[2].index].type != ValueType::float1 &&
                 material.nodes[node.inputs[2].index].type != node.type)) {
                report(diagnostics, node.span, "malformed lerp node");
            }
        } else if (node.kind == NodeKind::saturate) {
            if (node.inputs.size() != 1 || !valid_handle(material, node.inputs[0]) ||
                (valid_handle(material, node.inputs[0]) &&
                 material.nodes[node.inputs[0].index].type != node.type) ||
                !is_numeric(node.type)) {
                report(diagnostics, node.span, "malformed saturate node");
            }
        } else if (node.kind == NodeKind::sample) {
            if (node.inputs.size() != 2 || !valid_handle(material, node.inputs[0]) ||
                !valid_handle(material, node.inputs[1]) ||
                material.nodes[node.inputs[0].index].type != ValueType::texture ||
                material.nodes[node.inputs[1].index].type != ValueType::float2 ||
                node.type != ValueType::float4) {
                report(diagnostics, node.span, "malformed texture sample node");
            }
        } else if (node.kind == NodeKind::custom) {
            std::set<std::string> input_names;
            if (!is_numeric(node.type) || node.code.empty() ||
                node.custom_inputs.size() != node.inputs.size()) {
                report(diagnostics, node.span, "malformed custom node");
            }
            for (std::size_t input_index{}; input_index < node.custom_inputs.size();
                 ++input_index) {
                auto const& input{node.custom_inputs[input_index]};
                if (!input_names.insert(input.name).second || !valid_handle(material, input.node) ||
                    input.node != node.inputs[input_index] ||
                    (valid_handle(material, input.node) &&
                     material.nodes[input.node.index].type != input.type)) {
                    report(diagnostics, node.span, "malformed custom input '" + input.name + "'");
                }
            }
        }
    }

    std::set<std::string> dependencies;
    for (auto const& dependency : material.texture_dependencies) {
        if (dependency.empty() || !dependencies.insert(dependency).second) {
            diagnostics.push_back({{}, 1, 1, "invalid or duplicate texture dependency"});
        }
    }

    for (auto const& output : material.outputs) {
        if (output.name != "emissive" || !valid_handle(material, output.node) ||
            (valid_handle(material, output.node) &&
             material.nodes[output.node.index].type != ValueType::float3 &&
             material.nodes[output.node.index].type != ValueType::float4)) {
            report(diagnostics,
                   output.span,
                   "UI materials require a float3 or float4 emissive output");
        }
    }
    if (material.outputs.size() != 1) {
        diagnostics.push_back({{}, 1, 1, "UI material requires exactly one emissive output"});
    }
    return diagnostics;
}

}
