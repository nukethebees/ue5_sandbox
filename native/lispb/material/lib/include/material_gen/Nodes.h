#pragma once

#include <lispb/indices.h>
#include <material_gen/Constants.h>
#include <material_gen/CustomInputs.h>
#include <material_gen/Customs.h>
#include <material_gen/Indices.h>
#include <material_gen/NodeKind.h>
#include <material_gen/NodeRange.h>
#include <material_gen/ParameterReferences.h>
#include <material_gen/PositionSpace.h>
#include <material_gen/SceneTexture.h>
#include <material_gen/ShaderCalls.h>
#include <material_gen/SourceSpan.h>
#include <material_gen/Textures.h>
#include <material_gen/TextureSamplerType.h>
#include <material_gen/Transforms.h>
#include <material_gen/ValueType.h>

#include <cstddef>
#include <cstdint>
#include <ranges>
#include <span>
#include <vector>
namespace material_synth {

class Nodes {
  public:
    CustomInputs input_declarations;
    auto
        add(NodeKind kind, ValueType type, SourceSpan span, std::span<NodeHandle const> inputs = {})
            -> NodeHandle;
    auto size() const -> std::size_t { return kinds_.size(); }
    auto kind(NodeHandle node) const -> NodeKind { return kinds_[node.value]; }
    auto type(NodeHandle node) const -> ValueType { return types_[node.value]; }
    auto span(NodeHandle node) const -> SourceSpan { return spans_[node.value]; }
    auto inputs(NodeHandle node) const -> std::span<NodeHandle const>;
    auto constant(NodeHandle node, std::size_t component) const -> double;
    auto texture_sampler_type(NodeHandle node) const -> TextureSamplerType;
    auto custom_inputs(NodeHandle node) const -> std::span<CustomInputId const>;
    auto handles() const {
        return std::views::iota(std::uint32_t{}, static_cast<std::uint32_t>(size())) |
               std::views::transform([](std::uint32_t index) { return NodeHandle{index}; });
    }
    auto component_count(NodeHandle node) const -> std::uint8_t;
    auto parameter(NodeHandle node) const -> ParameterId;
    auto coordinate_index(NodeHandle node) const -> unsigned;
    auto instance_data_index(NodeHandle node) const -> unsigned;
    auto description(NodeHandle node) const -> lispb::StringId;
    auto code(NodeHandle node) const -> lispb::StringId;
    auto texture_path(NodeHandle node) const -> lispb::StringId;
    auto component_mask(NodeHandle node) const -> lispb::StringId;
    auto source_space(NodeHandle node) const -> PositionSpace;
    auto destination_space(NodeHandle node) const -> PositionSpace;
    auto scene_texture(NodeHandle node) const -> SceneTexture;
    auto shader_path(NodeHandle node) const -> lispb::StringId;
    auto shader_function(NodeHandle node) const -> lispb::StringId;
    auto add_constant(ValueType type,
                      SourceSpan span,
                      std::span<NodeHandle const> inputs,
                      std::span<double const> values) -> NodeHandle;
    auto add_parameter(ValueType type,
                       SourceSpan span,
                       std::span<NodeHandle const> inputs,
                       ParameterId index,
                       TextureSamplerType sampler) -> NodeHandle;
    auto add_texture_coordinate(ValueType type,
                                SourceSpan span,
                                std::span<NodeHandle const> inputs,
                                unsigned index) -> NodeHandle;
    auto add_per_instance_custom_data(ValueType type,
                                      SourceSpan span,
                                      std::span<NodeHandle const> inputs,
                                      unsigned index) -> NodeHandle;
    auto add_sample(ValueType type,
                    SourceSpan span,
                    std::span<NodeHandle const> inputs,
                    TextureSamplerType sampler) -> NodeHandle;
    auto add_custom(ValueType type,
                    SourceSpan span,
                    std::span<NodeHandle const> inputs,
                    lispb::StringId description,
                    lispb::StringId code,
                    std::span<CustomInputId const> custom_inputs) -> NodeHandle;
    auto add_texture_object(ValueType type,
                            SourceSpan span,
                            std::span<NodeHandle const> inputs,
                            lispb::StringId path,
                            TextureSamplerType sampler) -> NodeHandle;
    auto add_component_mask(ValueType type,
                            SourceSpan span,
                            std::span<NodeHandle const> inputs,
                            lispb::StringId mask) -> NodeHandle;
    auto add_transform_position(ValueType type,
                                SourceSpan span,
                                std::span<NodeHandle const> inputs,
                                PositionSpace source,
                                PositionSpace destination) -> NodeHandle;
    auto add_scene_texture(ValueType type,
                           SourceSpan span,
                           std::span<NodeHandle const> inputs,
                           SceneTexture texture) -> NodeHandle;
    auto add_shader_call(ValueType type,
                         SourceSpan span,
                         std::span<NodeHandle const> inputs,
                         lispb::StringId path,
                         lispb::StringId function,
                         std::span<CustomInputId const> custom_inputs) -> NodeHandle;
  private:
    auto append(NodeKind kind,
                ValueType type,
                SourceSpan span,
                std::span<NodeHandle const> inputs,
                std::uint32_t payload) -> NodeHandle;
    auto save_custom_inputs(std::span<CustomInputId const> values) -> NodeRange;
    std::vector<NodeKind> kinds_;
    std::vector<ValueType> types_;
    std::vector<SourceSpan> spans_;
    std::vector<NodeRange> input_ranges_;
    std::vector<std::uint32_t> payloads_;
    std::vector<NodeHandle> inputs_;
    std::vector<CustomInputId> custom_inputs_;

    Constants constants_;
    ParameterReferences parameters_;
    std::vector<unsigned> coordinate_indices_;
    std::vector<unsigned> instance_data_indices_;
    std::vector<TextureSamplerType> sample_samplers_;
    Customs customs_;
    Textures textures_;
    std::vector<lispb::StringId> component_masks_;
    Transforms transforms_;
    std::vector<SceneTexture> scene_textures_;
    ShaderCalls shader_calls_;
};

}
