#include <material_gen/Nodes.h>

#include <cassert>
#include <utility>

namespace material_synth {

auto Nodes::append(NodeKind kind,
                   ValueType type,
                   SourceSpan span,
                   std::span<NodeHandle const> inputs,
                   std::uint32_t payload) -> NodeHandle {
    auto const handle{NodeHandle{static_cast<std::uint32_t>(size())}};
    kinds_.push_back(kind);
    types_.push_back(type);
    spans_.push_back(span);
    input_ranges_.push_back(
        {static_cast<std::uint32_t>(inputs_.size()), static_cast<std::uint32_t>(inputs.size())});
    inputs_.insert(inputs_.end(), inputs.begin(), inputs.end());
    payloads_.push_back(payload);
    return handle;
}

auto Nodes::add(NodeKind kind, ValueType type, SourceSpan span, std::span<NodeHandle const> inputs)
    -> NodeHandle {
    switch (kind) {
        case NodeKind::constant:
        case NodeKind::parameter:
        case NodeKind::texture_coordinate:
        case NodeKind::per_instance_custom_data:
        case NodeKind::sample:
        case NodeKind::custom:
        case NodeKind::texture_object:
        case NodeKind::component_mask:
        case NodeKind::transform_position:
        case NodeKind::scene_texture:
        case NodeKind::shader_call:
            assert(false && "Use the kind-specific node factory for payloads");
            std::unreachable();
        default:
            return append(kind, type, span, inputs, 0);
    }
}

auto Nodes::save_custom_inputs(std::span<CustomInputId const> values) -> NodeRange {
    NodeRange const range{static_cast<std::uint32_t>(custom_inputs_.size()),
                          static_cast<std::uint32_t>(values.size())};
    custom_inputs_.insert(custom_inputs_.end(), values.begin(), values.end());
    return range;
}

auto Nodes::inputs(NodeHandle node) const -> std::span<NodeHandle const> {
    auto const range{input_ranges_[node.value]};
    return std::span{inputs_}.subspan(range.begin, range.count);
}

auto Nodes::constant(NodeHandle node, std::size_t component) const -> double {
    assert(kind(node) == NodeKind::constant && component < 4);
    return constants_.components[component][payloads_[node.value]];
}

auto Nodes::texture_sampler_type(NodeHandle node) const -> TextureSamplerType {
    auto const payload{payloads_[node.value]};
    switch (kind(node)) {
        case NodeKind::parameter:
            return parameters_.samplers[payload];
        case NodeKind::sample:
            return sample_samplers_[payload];
        case NodeKind::texture_object:
            return textures_.samplers[payload];
        default:
            assert(false);
            std::unreachable();
    }
}

auto Nodes::custom_inputs(NodeHandle node) const -> std::span<CustomInputId const> {
    assert(kind(node) == NodeKind::custom || kind(node) == NodeKind::shader_call);
    auto const payload{payloads_[node.value]};
    auto const range{kind(node) == NodeKind::custom ? customs_.inputs[payload]
                                                    : shader_calls_.inputs[payload]};
    return std::span{custom_inputs_}.subspan(range.begin, range.count);
}

auto Nodes::component_count(NodeHandle node) const -> std::uint8_t {
    assert(kind(node) == NodeKind::constant);
    return constants_.counts[payloads_[node.value]];
}

auto Nodes::parameter(NodeHandle node) const -> ParameterId {
    assert(kind(node) == NodeKind::parameter);
    return parameters_.indices[payloads_[node.value]];
}

auto Nodes::coordinate_index(NodeHandle node) const -> unsigned {
    assert(kind(node) == NodeKind::texture_coordinate);
    return coordinate_indices_[payloads_[node.value]];
}

auto Nodes::instance_data_index(NodeHandle node) const -> unsigned {
    assert(kind(node) == NodeKind::per_instance_custom_data);
    return instance_data_indices_[payloads_[node.value]];
}

auto Nodes::description(NodeHandle node) const -> lispb::StringId {
    assert(kind(node) == NodeKind::custom);
    return customs_.descriptions[payloads_[node.value]];
}

auto Nodes::code(NodeHandle node) const -> lispb::StringId {
    assert(kind(node) == NodeKind::custom);
    return customs_.code[payloads_[node.value]];
}

auto Nodes::texture_path(NodeHandle node) const -> lispb::StringId {
    assert(kind(node) == NodeKind::texture_object);
    return textures_.paths[payloads_[node.value]];
}

auto Nodes::component_mask(NodeHandle node) const -> lispb::StringId {
    assert(kind(node) == NodeKind::component_mask);
    return component_masks_[payloads_[node.value]];
}

auto Nodes::source_space(NodeHandle node) const -> PositionSpace {
    assert(kind(node) == NodeKind::transform_position);
    return transforms_.sources[payloads_[node.value]];
}

auto Nodes::destination_space(NodeHandle node) const -> PositionSpace {
    assert(kind(node) == NodeKind::transform_position);
    return transforms_.destinations[payloads_[node.value]];
}

auto Nodes::scene_texture(NodeHandle node) const -> SceneTexture {
    assert(kind(node) == NodeKind::scene_texture);
    return scene_textures_[payloads_[node.value]];
}

auto Nodes::shader_path(NodeHandle node) const -> lispb::StringId {
    assert(kind(node) == NodeKind::shader_call);
    return shader_calls_.paths[payloads_[node.value]];
}

auto Nodes::shader_function(NodeHandle node) const -> lispb::StringId {
    assert(kind(node) == NodeKind::shader_call);
    return shader_calls_.functions[payloads_[node.value]];
}

auto Nodes::add_constant(ValueType type,
                         SourceSpan span,
                         std::span<NodeHandle const> inputs,
                         std::span<double const> values) -> NodeHandle {
    auto const payload{static_cast<std::uint32_t>(constants_.counts.size())};
    assert(values.size() <= 4);
    for (std::size_t component{}; component < 4; ++component) {
        constants_.components[component].push_back(component < values.size() ? values[component]
                                                                             : 0.0);
    }
    constants_.counts.push_back(static_cast<std::uint8_t>(values.size()));
    return append(NodeKind::constant, type, span, inputs, payload);
}

auto Nodes::add_parameter(ValueType type,
                          SourceSpan span,
                          std::span<NodeHandle const> inputs,
                          ParameterId index,
                          TextureSamplerType sampler) -> NodeHandle {
    auto const payload{static_cast<std::uint32_t>(parameters_.indices.size())};
    parameters_.indices.push_back(index);
    parameters_.samplers.push_back(sampler);
    return append(NodeKind::parameter, type, span, inputs, payload);
}

auto Nodes::add_texture_coordinate(ValueType type,
                                   SourceSpan span,
                                   std::span<NodeHandle const> inputs,
                                   unsigned index) -> NodeHandle {
    auto const payload{static_cast<std::uint32_t>(coordinate_indices_.size())};
    coordinate_indices_.push_back(index);
    return append(NodeKind::texture_coordinate, type, span, inputs, payload);
}

auto Nodes::add_per_instance_custom_data(ValueType type,
                                         SourceSpan span,
                                         std::span<NodeHandle const> inputs,
                                         unsigned index) -> NodeHandle {
    auto const payload{static_cast<std::uint32_t>(instance_data_indices_.size())};
    instance_data_indices_.push_back(index);
    return append(NodeKind::per_instance_custom_data, type, span, inputs, payload);
}

auto Nodes::add_sample(ValueType type,
                       SourceSpan span,
                       std::span<NodeHandle const> inputs,
                       TextureSamplerType sampler) -> NodeHandle {
    auto const payload{static_cast<std::uint32_t>(sample_samplers_.size())};
    sample_samplers_.push_back(sampler);
    return append(NodeKind::sample, type, span, inputs, payload);
}

auto Nodes::add_custom(ValueType type,
                       SourceSpan span,
                       std::span<NodeHandle const> inputs,
                       lispb::StringId description,
                       lispb::StringId code,
                       std::span<CustomInputId const> custom_inputs) -> NodeHandle {
    auto const payload{static_cast<std::uint32_t>(customs_.code.size())};
    customs_.descriptions.push_back(description);
    customs_.code.push_back(code);
    customs_.inputs.push_back(save_custom_inputs(custom_inputs));
    return append(NodeKind::custom, type, span, inputs, payload);
}

auto Nodes::add_texture_object(ValueType type,
                               SourceSpan span,
                               std::span<NodeHandle const> inputs,
                               lispb::StringId path,
                               TextureSamplerType sampler) -> NodeHandle {
    auto const payload{static_cast<std::uint32_t>(textures_.paths.size())};
    textures_.paths.push_back(path);
    textures_.samplers.push_back(sampler);
    return append(NodeKind::texture_object, type, span, inputs, payload);
}

auto Nodes::add_component_mask(ValueType type,
                               SourceSpan span,
                               std::span<NodeHandle const> inputs,
                               lispb::StringId mask) -> NodeHandle {
    auto const payload{static_cast<std::uint32_t>(component_masks_.size())};
    component_masks_.push_back(mask);
    return append(NodeKind::component_mask, type, span, inputs, payload);
}

auto Nodes::add_transform_position(ValueType type,
                                   SourceSpan span,
                                   std::span<NodeHandle const> inputs,
                                   PositionSpace source,
                                   PositionSpace destination) -> NodeHandle {
    auto const payload{static_cast<std::uint32_t>(transforms_.sources.size())};
    transforms_.sources.push_back(source);
    transforms_.destinations.push_back(destination);
    return append(NodeKind::transform_position, type, span, inputs, payload);
}

auto Nodes::add_scene_texture(ValueType type,
                              SourceSpan span,
                              std::span<NodeHandle const> inputs,
                              SceneTexture texture) -> NodeHandle {
    auto const payload{static_cast<std::uint32_t>(scene_textures_.size())};
    scene_textures_.push_back(texture);
    return append(NodeKind::scene_texture, type, span, inputs, payload);
}

auto Nodes::add_shader_call(ValueType type,
                            SourceSpan span,
                            std::span<NodeHandle const> inputs,
                            lispb::StringId path,
                            lispb::StringId function,
                            std::span<CustomInputId const> custom_inputs) -> NodeHandle {
    auto const payload{static_cast<std::uint32_t>(shader_calls_.paths.size())};
    shader_calls_.paths.push_back(path);
    shader_calls_.functions.push_back(function);
    shader_calls_.inputs.push_back(save_custom_inputs(custom_inputs));
    return append(NodeKind::shader_call, type, span, inputs, payload);
}

}
