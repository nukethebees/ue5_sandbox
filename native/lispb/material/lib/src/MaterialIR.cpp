#include <material_gen/MaterialIR.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <set>

namespace material_synth {
namespace {

void report(std::vector<Diagnostic>& diagnostics, SourceSpan const& span, std::string message) {
    diagnostics.push_back({span.path, span.line, span.column, std::move(message)});
}

auto valid_handle(MaterialIR const& material, NodeHandle const handle) -> bool {
    return handle.index < material.nodes.size();
}

auto valid_identifier(std::string_view const value) -> bool {
    if (value.empty() ||
        !(std::isalpha(static_cast<unsigned char>(value.front())) != 0 || value.front() == '_')) {
        return false;
    }
    return std::ranges::all_of(value.substr(1), [](unsigned char const character) {
        return std::isalnum(character) != 0 || character == '_';
    });
}

auto valid_generated_path(MaterialSettings const& settings) -> bool {
    auto const& path{settings.package_path};
    auto const generated{path.find("/Generated/Materials/")};
    auto const leaf_position{path.rfind('/')};
    return !path.empty() && path.front() == '/' && generated != std::string::npos &&
           generated != 0 && path.find('/', 1) == generated && leaf_position != std::string::npos &&
           path.substr(leaf_position + 1) == settings.name &&
           path.find('.', leaf_position) == std::string::npos;
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

    if (!valid_identifier(material.settings.name) || !valid_generated_path(material.settings) ||
        material.settings.domain < MaterialDomain::ui ||
        material.settings.domain > MaterialDomain::surface ||
        material.settings.blend_mode < BlendMode::additive ||
        material.settings.blend_mode > BlendMode::translucent ||
        material.settings.shading_model < ShadingModel::default_lit ||
        material.settings.shading_model > ShadingModel::unlit ||
        (material.settings.domain == MaterialDomain::ui &&
         (material.settings.blend_mode != BlendMode::additive ||
          material.settings.shading_model != ShadingModel::default_lit ||
          material.settings.two_sided || material.settings.disable_depth_test ||
          material.settings.used_with_instanced_static_meshes)) ||
        (material.settings.disable_depth_test &&
         material.settings.blend_mode != BlendMode::translucent)) {
        diagnostics.push_back({{}, 1, 1, "invalid or unsafe material settings"});
    }

    auto validate_named{[&](NamedNode const& named, std::string_view const category) {
        if (!valid_identifier(named.name)) {
            report(diagnostics, named.span, "invalid " + std::string{category} + " name");
        } else if (!names.insert(named.name).second) {
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
        if (!valid_identifier(parameter.name)) {
            report(diagnostics, parameter.span, "invalid parameter name");
        } else if (!names.insert(parameter.name).second) {
            report(
                diagnostics, parameter.span, "duplicate parameter name '" + parameter.name + "'");
        }
        if (parameter.type == ValueType::invalid ||
            (parameter.type == ValueType::texture && parameter.texture_path.empty()) ||
            (is_numeric(parameter.type) &&
             !std::all_of(parameter.default_value.begin(),
                          parameter.default_value.begin() + component_count(parameter.type),
                          [](double const value) { return std::isfinite(value); })) ||
            !valid_handle(material, parameter.node) ||
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
        if (node.kind < NodeKind::constant || node.kind > NodeKind::cosine ||
            node.type == ValueType::invalid) {
            report(diagnostics, node.span, "invalid material node kind or type");
            continue;
        }
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
        } else if (node.kind == NodeKind::per_instance_custom_data) {
            if (material.settings.domain != MaterialDomain::surface ||
                node.type != ValueType::float1 || !node.inputs.empty()) {
                report(diagnostics, node.span, "malformed per-instance custom-data node");
            }
        } else if (node.kind == NodeKind::add || node.kind == NodeKind::multiply) {
            auto type{ValueType::invalid};
            if (node.inputs.size() >= 2 && valid_handle(material, node.inputs[0])) {
                type = material.nodes[node.inputs[0].index].type;
                for (std::size_t input_index{1}; input_index < node.inputs.size(); ++input_index) {
                    if (!valid_handle(material, node.inputs[input_index])) {
                        type = ValueType::invalid;
                        break;
                    }
                    type = promoted(type, material.nodes[node.inputs[input_index].index].type);
                }
            }
            if (type != node.type) {
                report(diagnostics, node.span, "malformed n-ary arithmetic node");
            }
        } else if (node.kind == NodeKind::subtract || node.kind == NodeKind::divide) {
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
                if (!valid_identifier(input.name) || !input_names.insert(input.name).second ||
                    input.type == ValueType::invalid || !valid_handle(material, input.node) ||
                    input.node != node.inputs[input_index] ||
                    (valid_handle(material, input.node) &&
                     material.nodes[input.node.index].type != input.type)) {
                    report(diagnostics, node.span, "malformed custom input '" + input.name + "'");
                }
            }
        } else if (node.kind == NodeKind::vector_constructor) {
            if (node.type == ValueType::float1 || !is_numeric(node.type) ||
                node.inputs.size() != component_count(node.type) ||
                std::ranges::any_of(node.inputs, [&](NodeHandle const input) {
                    return !valid_handle(material, input) ||
                           material.nodes[input.index].type != ValueType::float1;
                })) {
                report(diagnostics, node.span, "malformed vector-constructor node");
            }
        } else if (node.kind == NodeKind::time) {
            if (node.type != ValueType::float1 || !node.inputs.empty()) {
                report(diagnostics, node.span, "malformed time node");
            }
        } else if (node.kind == NodeKind::sine || node.kind == NodeKind::cosine) {
            if (node.type != ValueType::float1 || node.inputs.size() != 1 ||
                !valid_handle(material, node.inputs[0]) ||
                (valid_handle(material, node.inputs[0]) &&
                 material.nodes[node.inputs[0].index].type != ValueType::float1)) {
                report(diagnostics, node.span, "malformed trigonometric node");
            }
        }
    }

    std::set<std::string> dependencies;
    for (auto const& dependency : material.texture_dependencies) {
        if (dependency.empty() || !dependencies.insert(dependency).second) {
            diagnostics.push_back({{}, 1, 1, "invalid or duplicate texture dependency"});
        }
    }

    std::size_t emissive_count{};
    std::size_t opacity_count{};
    for (auto const& output : material.outputs) {
        auto const type{valid_handle(material, output.node) ? material.nodes[output.node.index].type
                                                            : ValueType::invalid};
        if (output.name == "emissive") {
            ++emissive_count;
            if (type != ValueType::float3 && type != ValueType::float4) {
                report(diagnostics, output.span, "emissive output requires float3 or float4");
            }
        } else if (output.name == "opacity" &&
                   material.settings.domain == MaterialDomain::surface &&
                   material.settings.blend_mode == BlendMode::translucent) {
            ++opacity_count;
            if (type != ValueType::float1) {
                report(diagnostics, output.span, "opacity output requires float");
            }
        } else {
            report(diagnostics, output.span, "output is not supported by the material domain");
        }
    }
    if (emissive_count != 1 || opacity_count > 1 ||
        (material.settings.domain == MaterialDomain::ui && material.outputs.size() != 1)) {
        diagnostics.push_back({{}, 1, 1, "material outputs do not match the material domain"});
    }
    return diagnostics;
}

}
