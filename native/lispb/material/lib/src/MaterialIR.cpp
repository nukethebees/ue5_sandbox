#include <material_gen/MaterialIR.h>

#include "material_type_rules.h"

#include <lispb/indices.h>
#include <lispb/strings.h>
#include <material_gen/BlendMode.h>
#include <material_gen/Diagnostic.h>
#include <material_gen/Indices.h>
#include <material_gen/MaterialDomain.h>
#include <material_gen/NamedNodes.h>
#include <material_gen/NodeKind.h>
#include <material_gen/Nodes.h>
#include <material_gen/PositionSpace.h>
#include <material_gen/SceneTexture.h>
#include <material_gen/ShadingModel.h>
#include <material_gen/SourceSpan.h>
#include <material_gen/TextureSamplerType.h>
#include <material_gen/ValueType.h>

#include <algorithm>
#include <cassert>
#include <cctype>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <format>
#include <ranges>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace material_synth {

auto MaterialIR::text(lispb::StringId id) const -> std::string_view {
    return strings->view(id);
}

auto MaterialIR::intern(std::string_view value) -> lispb::StringId {
    return strings->intern(value);
}

namespace {

void report(MaterialIR const& material,
            std::vector<Diagnostic>& diagnostics,
            SourceSpan const& span,
            std::string message) {
    diagnostics.push_back(
        {std::string{material.text(span.path)}, span.line, span.column, std::move(message)});
}

auto valid_handle(MaterialIR const& material, NodeHandle const handle) -> bool {
    return handle.value < material.nodes.size();
}

auto valid_asset_path(MaterialIR const& material) -> bool {
    auto const& settings{material.settings};
    auto const path{material.text(settings.package_path)};
    auto const generated{path.find("/Generated/Materials/")};
    auto const leaf_position{path.rfind('/')};
    auto const structurally_valid{!path.empty() && path.front() == '/' &&
                                  path.find('/', 1) != std::string::npos &&
                                  leaf_position != std::string::npos &&
                                  path.substr(leaf_position + 1) == material.text(settings.name) &&
                                  path.find('.', leaf_position) == std::string::npos};
    auto const generated_path{generated != std::string::npos && generated != 0 &&
                              path.find('/', 1) == generated};
    return structurally_valid && (generated_path || settings.adopt_existing);
}

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

auto validate(MaterialIR const& material) -> std::vector<Diagnostic> {
    std::vector<Diagnostic> diagnostics;
    std::set<std::string_view> names;

    if (!valid_identifier(material.text(material.settings.name)) || !valid_asset_path(material) ||
        material.settings.domain < MaterialDomain::ui ||
        material.settings.domain > MaterialDomain::post_process ||
        material.settings.blend_mode < BlendMode::additive ||
        material.settings.blend_mode > BlendMode::alpha_composite ||
        material.settings.shading_model < ShadingModel::default_lit ||
        material.settings.shading_model > ShadingModel::unlit ||
        !std::isfinite(material.settings.opacity_mask_clip_value) ||
        material.settings.opacity_mask_clip_value < 0.0 ||
        material.settings.opacity_mask_clip_value > 1.0 ||
        (material.settings.domain == MaterialDomain::ui &&
         (material.settings.blend_mode != BlendMode::additive ||
          material.settings.shading_model != ShadingModel::default_lit ||
          material.settings.two_sided || material.settings.disable_depth_test ||
          material.settings.used_with_instanced_static_meshes ||
          material.settings.used_with_particle_sprites)) ||
        (material.settings.domain == MaterialDomain::post_process &&
         (material.settings.blend_mode != BlendMode::opaque ||
          material.settings.shading_model != ShadingModel::unlit || material.settings.two_sided ||
          material.settings.disable_depth_test ||
          material.settings.used_with_instanced_static_meshes ||
          material.settings.used_with_particle_sprites)) ||
        (material.settings.disable_depth_test &&
         material.settings.blend_mode != BlendMode::translucent &&
         material.settings.blend_mode != BlendMode::alpha_composite)) {
        diagnostics.push_back({{}, 1, 1, "invalid or unsafe material settings"});
    }

    auto validate_named{
        [&](NamedNodes const& nodes, NamedNodeId named, std::string_view const category) {
            if (!valid_identifier(material.text(nodes.name[named.value]))) {
                report(material,
                       diagnostics,
                       nodes.span[named.value],
                       std::format("invalid {} name", category));
            } else if (!names.insert(material.text(nodes.name[named.value])).second) {
                report(material,
                       diagnostics,
                       nodes.span[named.value],
                       std::format("duplicate {} name '{}'",
                                   category,
                                   material.text(nodes.name[named.value])));
            }
            if (!valid_handle(material, nodes.node[named.value])) {
                report(material,
                       diagnostics,
                       nodes.span[named.value],
                       std::format("invalid node handle for '{}'",
                                   material.text(nodes.name[named.value])));
            }
        }};

    for (auto const parameter : material.parameters.handles()) {
        if (!valid_identifier(material.text(material.parameters.name[parameter.value]))) {
            report(material,
                   diagnostics,
                   material.parameters.span[parameter.value],
                   "invalid parameter name");
        } else if (!names.insert(material.text(material.parameters.name[parameter.value])).second) {
            report(material,
                   diagnostics,
                   material.parameters.span[parameter.value],
                   std::format("duplicate parameter name '{}'",
                               material.text(material.parameters.name[parameter.value])));
        }
        if (material.parameters.type[parameter.value] == ValueType::invalid ||
            (material.parameters.type[parameter.value] == ValueType::texture &&
             material.text(material.parameters.texture_path[parameter.value]).empty()) ||
            (is_numeric(material.parameters.type[parameter.value]) &&
             !std::ranges::all_of(
                 std::views::iota(std::size_t{},
                                  component_count(material.parameters.type[parameter.value])),
                 [&](std::size_t component) {
                     return std::isfinite(
                         material.parameters.default_values[component][parameter.value]);
                 })) ||
            !valid_handle(material, material.parameters.node[parameter.value]) ||
            material.nodes.kind(material.parameters.node[parameter.value]) != NodeKind::parameter ||
            material.nodes.parameter(material.parameters.node[parameter.value]) != parameter ||
            material.nodes.type(material.parameters.node[parameter.value]) !=
                material.parameters.type[parameter.value] ||
            material.parameters.texture_sampler_type[parameter.value] <
                TextureSamplerType::linear_color ||
            material.parameters.texture_sampler_type[parameter.value] >
                TextureSamplerType::linear_grayscale ||
            material.nodes.texture_sampler_type(material.parameters.node[parameter.value]) !=
                material.parameters.texture_sampler_type[parameter.value]) {
            report(material,
                   diagnostics,
                   material.parameters.span[parameter.value],
                   std::format("malformed parameter node for '{}'",
                               material.text(material.parameters.name[parameter.value])));
        }
    }
    for (auto const binding : material.bindings.handles()) {
        validate_named(material.bindings, binding, "binding");
    }

    names.clear();
    for (auto const output : material.outputs.handles()) {
        if (!valid_handle(material, material.outputs.node[output.value])) {
            report(material,
                   diagnostics,
                   material.outputs.span[output.value],
                   std::format("invalid node handle for output '{}'",
                               material.text(material.outputs.name[output.value])));
        }
    }

    auto const node_count{material.nodes.size()};
    for (std::size_t index{}; index < node_count; ++index) {
        auto const node{NodeHandle{static_cast<std::uint32_t>(index)}};
        if (material.nodes.kind(node) < NodeKind::constant ||
            material.nodes.kind(node) > NodeKind::vertex_color ||
            material.nodes.type(node) == ValueType::invalid) {
            report(material,
                   diagnostics,
                   material.nodes.span(node),
                   "invalid material node kind or type");
            continue;
        }
        for (auto const input : material.nodes.inputs(node)) {
            if (!valid_handle(material, input) || input.value >= index) {
                report(material,
                       diagnostics,
                       material.nodes.span(node),
                       "node input must reference an earlier valid node");
            }
        }
        if (material.nodes.kind(node) == NodeKind::constant) {
            if (!is_numeric(material.nodes.type(node)) ||
                material.nodes.component_count(node) !=
                    component_count(material.nodes.type(node)) ||
                !material.nodes.inputs(node).empty() ||
                !std::ranges::all_of(std::views::iota(std::size_t{},
                                                      static_cast<std::size_t>(
                                                          material.nodes.component_count(node))),
                                     [&](std::size_t component) {
                                         return std::isfinite(
                                             material.nodes.constant(node, component));
                                     })) {
                report(material, diagnostics, material.nodes.span(node), "malformed constant node");
            }
        } else if (material.nodes.kind(node) == NodeKind::parameter) {
            if (material.nodes.parameter(node).value >= material.parameters.size() ||
                !material.nodes.inputs(node).empty() ||
                (material.nodes.parameter(node).value < material.parameters.size() &&
                 material.parameters.type[material.nodes.parameter(node).value] !=
                     material.nodes.type(node))) {
                report(
                    material, diagnostics, material.nodes.span(node), "malformed parameter node");
            }
        } else if (material.nodes.kind(node) == NodeKind::texture_coordinate) {
            if (material.nodes.type(node) != ValueType::float2 ||
                material.nodes.coordinate_index(node) > 7 || !material.nodes.inputs(node).empty()) {
                report(material,
                       diagnostics,
                       material.nodes.span(node),
                       "malformed texture-coordinate node");
            }
        } else if (material.nodes.kind(node) == NodeKind::per_instance_custom_data) {
            if (material.settings.domain != MaterialDomain::surface ||
                material.nodes.type(node) != ValueType::float1 ||
                !material.nodes.inputs(node).empty()) {
                report(material,
                       diagnostics,
                       material.nodes.span(node),
                       "malformed per-instance custom-data node");
            }
        } else if (material.nodes.kind(node) == NodeKind::add ||
                   material.nodes.kind(node) == NodeKind::multiply) {
            auto type{ValueType::invalid};
            if (material.nodes.inputs(node).size() >= 2 &&
                valid_handle(material, material.nodes.inputs(node)[0])) {
                type = material.nodes.type(material.nodes.inputs(node)[0]);
                for (std::size_t input_index{1}; input_index < material.nodes.inputs(node).size();
                     ++input_index) {
                    if (!valid_handle(material, material.nodes.inputs(node)[input_index])) {
                        type = ValueType::invalid;
                        break;
                    }
                    type = promote_value_types(
                        type, material.nodes.type(material.nodes.inputs(node)[input_index]));
                }
            }
            if (type != material.nodes.type(node)) {
                report(material,
                       diagnostics,
                       material.nodes.span(node),
                       "malformed n-ary arithmetic node");
            }
        } else if (material.nodes.kind(node) == NodeKind::subtract ||
                   material.nodes.kind(node) == NodeKind::divide) {
            if (material.nodes.inputs(node).size() != 2 ||
                !valid_handle(material, material.nodes.inputs(node)[0]) ||
                !valid_handle(material, material.nodes.inputs(node)[1]) ||
                promote_value_types(material.nodes.type(material.nodes.inputs(node)[0]),
                                    material.nodes.type(material.nodes.inputs(node)[1])) !=
                    material.nodes.type(node)) {
                report(
                    material, diagnostics, material.nodes.span(node), "malformed arithmetic node");
            }
        } else if (material.nodes.kind(node) == NodeKind::lerp) {
            if (material.nodes.inputs(node).size() != 3 ||
                !valid_handle(material, material.nodes.inputs(node)[0]) ||
                !valid_handle(material, material.nodes.inputs(node)[1]) ||
                !valid_handle(material, material.nodes.inputs(node)[2]) ||
                (valid_handle(material, material.nodes.inputs(node)[0]) &&
                 valid_handle(material, material.nodes.inputs(node)[1]) &&
                 promote_value_types(material.nodes.type(material.nodes.inputs(node)[0]),
                                     material.nodes.type(material.nodes.inputs(node)[1])) !=
                     material.nodes.type(node)) ||
                (valid_handle(material, material.nodes.inputs(node)[2]) &&
                 material.nodes.type(material.nodes.inputs(node)[2]) != ValueType::float1 &&
                 material.nodes.type(material.nodes.inputs(node)[2]) !=
                     material.nodes.type(node))) {
                report(material, diagnostics, material.nodes.span(node), "malformed lerp node");
            }
        } else if (material.nodes.kind(node) == NodeKind::saturate) {
            if (material.nodes.inputs(node).size() != 1 ||
                !valid_handle(material, material.nodes.inputs(node)[0]) ||
                (valid_handle(material, material.nodes.inputs(node)[0]) &&
                 material.nodes.type(material.nodes.inputs(node)[0]) !=
                     material.nodes.type(node)) ||
                !is_numeric(material.nodes.type(node))) {
                report(material, diagnostics, material.nodes.span(node), "malformed saturate node");
            }
        } else if (material.nodes.kind(node) == NodeKind::step) {
            if (material.nodes.type(node) != ValueType::float1 ||
                material.nodes.inputs(node).size() != 2 ||
                !valid_handle(material, material.nodes.inputs(node)[0]) ||
                !valid_handle(material, material.nodes.inputs(node)[1]) ||
                (valid_handle(material, material.nodes.inputs(node)[0]) &&
                 material.nodes.type(material.nodes.inputs(node)[0]) != ValueType::float1) ||
                (valid_handle(material, material.nodes.inputs(node)[1]) &&
                 material.nodes.type(material.nodes.inputs(node)[1]) != ValueType::float1)) {
                report(material, diagnostics, material.nodes.span(node), "malformed step node");
            }
        } else if (material.nodes.kind(node) == NodeKind::sample) {
            if (material.nodes.inputs(node).size() != 2 ||
                !valid_handle(material, material.nodes.inputs(node)[0]) ||
                !valid_handle(material, material.nodes.inputs(node)[1]) ||
                material.nodes.type(material.nodes.inputs(node)[0]) != ValueType::texture ||
                material.nodes.type(material.nodes.inputs(node)[1]) != ValueType::float2 ||
                material.nodes.type(node) != ValueType::float4 ||
                material.nodes.texture_sampler_type(node) !=
                    material.nodes.texture_sampler_type(material.nodes.inputs(node)[0])) {
                report(material,
                       diagnostics,
                       material.nodes.span(node),
                       "malformed texture sample node");
            }
        } else if (material.nodes.kind(node) == NodeKind::custom) {
            std::set<std::string_view> input_names;
            if (!is_numeric(material.nodes.type(node)) ||
                material.text(material.nodes.code(node)).empty() ||
                material.nodes.custom_inputs(node).size() != material.nodes.inputs(node).size()) {
                report(material, diagnostics, material.nodes.span(node), "malformed custom node");
            }
            for (std::size_t input_index{}; input_index < material.nodes.custom_inputs(node).size();
                 ++input_index) {
                auto const& input{material.nodes.custom_inputs(node)[input_index]};
                if (!valid_identifier(
                        material.text(material.nodes.input_declarations.name[input.value])) ||
                    !input_names
                         .insert(material.text(material.nodes.input_declarations.name[input.value]))
                         .second ||
                    material.nodes.input_declarations.type[input.value] == ValueType::invalid ||
                    input_index >= material.nodes.inputs(node).size() ||
                    !valid_handle(material, material.nodes.inputs(node)[input_index]) ||
                    material.nodes.type(material.nodes.inputs(node)[input_index]) !=
                        material.nodes.input_declarations.type[input.value]) {
                    report(material,
                           diagnostics,
                           material.nodes.span(node),
                           std::format(
                               "malformed custom input '{}'",
                               material.text(material.nodes.input_declarations.name[input.value])));
                }
            }
        } else if (material.nodes.kind(node) == NodeKind::vector_constructor) {
            if (material.nodes.type(node) == ValueType::float1 ||
                !is_numeric(material.nodes.type(node)) ||
                material.nodes.inputs(node).size() != component_count(material.nodes.type(node)) ||
                std::ranges::any_of(material.nodes.inputs(node), [&](NodeHandle const input) {
                    return !valid_handle(material, input) ||
                           material.nodes.type(input) != ValueType::float1;
                })) {
                report(material,
                       diagnostics,
                       material.nodes.span(node),
                       "malformed vector-constructor node");
            }
        } else if (material.nodes.kind(node) == NodeKind::time) {
            if (material.nodes.type(node) != ValueType::float1 ||
                !material.nodes.inputs(node).empty()) {
                report(material, diagnostics, material.nodes.span(node), "malformed time node");
            }
        } else if (material.nodes.kind(node) == NodeKind::sine ||
                   material.nodes.kind(node) == NodeKind::cosine) {
            if (material.nodes.type(node) != ValueType::float1 ||
                material.nodes.inputs(node).size() != 1 ||
                !valid_handle(material, material.nodes.inputs(node)[0]) ||
                (valid_handle(material, material.nodes.inputs(node)[0]) &&
                 material.nodes.type(material.nodes.inputs(node)[0]) != ValueType::float1)) {
                report(material,
                       diagnostics,
                       material.nodes.span(node),
                       "malformed trigonometric node");
            }
        } else if (material.nodes.kind(node) == NodeKind::texture_object) {
            if (material.nodes.type(node) != ValueType::texture ||
                material.text(material.nodes.texture_path(node)).empty() ||
                !material.nodes.inputs(node).empty()) {
                report(material,
                       diagnostics,
                       material.nodes.span(node),
                       "malformed texture-object node");
            }
        } else if (material.nodes.kind(node) == NodeKind::component_mask) {
            auto const input_valid{material.nodes.inputs(node).size() == 1 &&
                                   valid_handle(material, material.nodes.inputs(node)[0])};
            auto const input_components{
                input_valid ? component_count(material.nodes.type(material.nodes.inputs(node)[0]))
                            : 0};
            std::size_t previous_component{};
            bool mask_valid{!material.text(material.nodes.component_mask(node)).empty() &&
                            material.text(material.nodes.component_mask(node)).size() <= 4};
            for (std::size_t component_index{};
                 mask_valid &&
                 component_index < material.text(material.nodes.component_mask(node)).size();
                 ++component_index) {
                auto const component{std::string_view{"rgba"}.find(
                    material.text(material.nodes.component_mask(node))[component_index])};
                mask_valid = component < input_components &&
                             (component_index == 0 || component > previous_component);
                previous_component = component;
            }
            if (!input_valid || !mask_valid ||
                component_count(material.nodes.type(node)) !=
                    material.text(material.nodes.component_mask(node)).size()) {
                report(material,
                       diagnostics,
                       material.nodes.span(node),
                       "malformed component-mask node");
            }
        } else if (material.nodes.kind(node) == NodeKind::world_position ||
                   material.nodes.kind(node) == NodeKind::object_position ||
                   material.nodes.kind(node) == NodeKind::pixel_normal ||
                   material.nodes.kind(node) == NodeKind::vertex_normal ||
                   material.nodes.kind(node) == NodeKind::camera_vector ||
                   material.nodes.kind(node) == NodeKind::camera_position) {
            if (material.nodes.type(node) != ValueType::float3 ||
                !material.nodes.inputs(node).empty()) {
                report(material,
                       diagnostics,
                       material.nodes.span(node),
                       "malformed standard material-value node");
            }
        } else if (material.nodes.kind(node) == NodeKind::vertex_color) {
            if (material.nodes.type(node) != ValueType::float4 ||
                !material.nodes.inputs(node).empty()) {
                report(material,
                       diagnostics,
                       material.nodes.span(node),
                       "malformed vertex-color material-value node");
            }
        } else if (material.nodes.kind(node) == NodeKind::transform_position) {
            if (material.nodes.type(node) != ValueType::float3 ||
                material.nodes.inputs(node).size() != 1 ||
                !valid_handle(material, material.nodes.inputs(node)[0]) ||
                (valid_handle(material, material.nodes.inputs(node)[0]) &&
                 material.nodes.type(material.nodes.inputs(node)[0]) != ValueType::float3) ||
                material.nodes.source_space(node) > PositionSpace::local ||
                material.nodes.destination_space(node) > PositionSpace::local ||
                material.nodes.source_space(node) == material.nodes.destination_space(node)) {
                report(material,
                       diagnostics,
                       material.nodes.span(node),
                       "malformed position-transform node");
            }
        } else if (material.nodes.kind(node) == NodeKind::scene_texture) {
            auto const expected_type{material.nodes.scene_texture(node) == SceneTexture::scene_depth
                                         ? ValueType::float1
                                         : ValueType::float4};
            if (material.settings.domain != MaterialDomain::post_process ||
                material.nodes.scene_texture(node) > SceneTexture::scene_depth ||
                material.nodes.type(node) != expected_type ||
                !material.nodes.inputs(node).empty()) {
                report(material,
                       diagnostics,
                       material.nodes.span(node),
                       "malformed scene-texture node");
            }
        } else if (material.nodes.kind(node) == NodeKind::shader_call) {
            std::set<std::string_view> input_names;
            auto const valid_path{
                !material.text(material.nodes.shader_path(node)).empty() &&
                material.text(material.nodes.shader_path(node)).front() == '/' &&
                (material.text(material.nodes.shader_path(node)).ends_with(".ush") ||
                 material.text(material.nodes.shader_path(node)).ends_with(".usf"))};
            if (!is_numeric(material.nodes.type(node)) || !valid_path ||
                !valid_identifier(material.text(material.nodes.shader_function(node))) ||
                material.nodes.custom_inputs(node).size() != material.nodes.inputs(node).size()) {
                report(
                    material, diagnostics, material.nodes.span(node), "malformed shader-call node");
            }
            for (std::size_t input_index{}; input_index < material.nodes.custom_inputs(node).size();
                 ++input_index) {
                auto const& input{material.nodes.custom_inputs(node)[input_index]};
                if (!valid_identifier(
                        material.text(material.nodes.input_declarations.name[input.value])) ||
                    !input_names
                         .insert(material.text(material.nodes.input_declarations.name[input.value]))
                         .second ||
                    material.nodes.input_declarations.type[input.value] == ValueType::invalid ||
                    input_index >= material.nodes.inputs(node).size() ||
                    !valid_handle(material, material.nodes.inputs(node)[input_index]) ||
                    material.nodes.type(material.nodes.inputs(node)[input_index]) !=
                        material.nodes.input_declarations.type[input.value]) {
                    report(material,
                           diagnostics,
                           material.nodes.span(node),
                           std::format(
                               "malformed shader-call input '{}'",
                               material.text(material.nodes.input_declarations.name[input.value])));
                }
            }
        }
    }

    std::set<std::string_view> dependencies;
    for (auto const& dependency : material.texture_dependencies) {
        if (material.text(dependency).empty() ||
            !dependencies.insert(material.text(dependency)).second) {
            diagnostics.push_back({{}, 1, 1, "invalid or duplicate texture dependency"});
        }
    }

    std::size_t emissive_count{};
    std::set<std::string_view> output_names;
    for (auto const output : material.outputs.handles()) {
        auto const type{valid_handle(material, material.outputs.node[output.value])
                            ? material.nodes.type(material.outputs.node[output.value])
                            : ValueType::invalid};
        if (material.text(material.outputs.name[output.value]) == "emissive") {
            ++emissive_count;
            if (type != ValueType::float3 && type != ValueType::float4) {
                report(material,
                       diagnostics,
                       material.outputs.span[output.value],
                       "emissive output requires float3 or float4");
            }
        } else if (material.text(material.outputs.name[output.value]) == "opacity" &&
                   material.settings.domain == MaterialDomain::surface &&
                   (material.settings.blend_mode == BlendMode::translucent ||
                    material.settings.blend_mode == BlendMode::additive ||
                    material.settings.blend_mode == BlendMode::alpha_composite)) {
            if (type != ValueType::float1) {
                report(material,
                       diagnostics,
                       material.outputs.span[output.value],
                       "opacity output requires float");
            }
        } else if (material.text(material.outputs.name[output.value]) == "base-color" &&
                   material.settings.domain == MaterialDomain::surface) {
            if (type != ValueType::float3 && type != ValueType::float4) {
                report(material,
                       diagnostics,
                       material.outputs.span[output.value],
                       "base-color output requires float3 or float4");
            }
        } else if (material.text(material.outputs.name[output.value]) == "opacity-mask" &&
                   material.settings.domain == MaterialDomain::surface &&
                   material.settings.blend_mode == BlendMode::masked) {
            if (type != ValueType::float1) {
                report(material,
                       diagnostics,
                       material.outputs.span[output.value],
                       "opacity-mask output requires float");
            }
        } else if (material.text(material.outputs.name[output.value]) == "world-position-offset" &&
                   material.settings.domain == MaterialDomain::surface) {
            if (type != ValueType::float3) {
                report(material,
                       diagnostics,
                       material.outputs.span[output.value],
                       "world-position-offset output requires float3");
            }
        } else {
            report(material,
                   diagnostics,
                   material.outputs.span[output.value],
                   "output is not supported by the material domain");
        }
        if (!output_names.insert(material.text(material.outputs.name[output.value])).second) {
            report(material,
                   diagnostics,
                   material.outputs.span[output.value],
                   "duplicate material output");
        }
    }
    if (emissive_count != 1 || ((material.settings.domain == MaterialDomain::ui ||
                                 material.settings.domain == MaterialDomain::post_process) &&
                                material.outputs.size() != 1)) {
        diagnostics.push_back({{}, 1, 1, "material outputs do not match the material domain"});
    }
    return diagnostics;
}

}
