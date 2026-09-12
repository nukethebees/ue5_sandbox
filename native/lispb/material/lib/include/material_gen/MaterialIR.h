#pragma once

#include <array>
#include <cstddef>
#include <limits>
#include <string>
#include <vector>

namespace material_synth {

enum class ValueType { invalid, float1, float2, float3, float4, texture };
enum class TextureSamplerType { linear_color, linear_grayscale };
enum class MaterialDomain { ui, surface, post_process };
enum class BlendMode { additive, translucent, opaque, masked };
enum class ShadingModel { default_lit, unlit };
enum class PositionSpace { world, local };
enum class SceneTexture { post_process_input0, scene_depth };
enum class NodeKind {
    constant,
    parameter,
    texture_coordinate,
    per_instance_custom_data,
    add,
    subtract,
    multiply,
    divide,
    lerp,
    saturate,
    sample,
    custom,
    vector_constructor,
    time,
    sine,
    cosine,
    texture_object,
    component_mask,
    world_position,
    object_position,
    pixel_normal,
    vertex_normal,
    camera_vector,
    transform_position,
    scene_texture,
    shader_call,
};

struct NodeHandle {
    std::size_t index{std::numeric_limits<std::size_t>::max()};

    auto operator==(NodeHandle const&) const -> bool = default;
};

struct Diagnostic {
    std::string path;
    std::size_t line{1};
    std::size_t column{1};
    std::string message;
};

struct SourceSpan {
    std::size_t line{1};
    std::size_t column{1};
    std::string path;
    std::string expansion;
};

struct CustomInput {
    std::string name;
    ValueType type{ValueType::invalid};
    NodeHandle node;
};

struct Node {
    NodeKind kind{NodeKind::constant};
    ValueType type{ValueType::invalid};
    std::vector<NodeHandle> inputs;
    SourceSpan span;
    std::array<double, 4> constant{};
    std::size_t component_count{};
    std::size_t parameter_index{};
    unsigned coordinate_index{};
    unsigned instance_data_index{};
    TextureSamplerType texture_sampler_type{TextureSamplerType::linear_color};
    std::string description;
    std::string code;
    std::vector<CustomInput> custom_inputs;
    std::string texture_path;
    std::string component_mask;
    PositionSpace source_space{PositionSpace::world};
    PositionSpace destination_space{PositionSpace::local};
    SceneTexture scene_texture{SceneTexture::post_process_input0};
    std::string shader_path;
    std::string shader_function;
};

struct Parameter {
    std::string name;
    ValueType type{ValueType::invalid};
    std::array<double, 4> default_value{};
    std::string texture_path;
    TextureSamplerType texture_sampler_type{TextureSamplerType::linear_color};
    NodeHandle node;
    SourceSpan span;
};

struct NamedNode {
    std::string name;
    NodeHandle node;
    SourceSpan span;
};

struct MaterialSettings {
    std::string name;
    std::string package_path;
    MaterialDomain domain{MaterialDomain::ui};
    BlendMode blend_mode{BlendMode::additive};
    ShadingModel shading_model{ShadingModel::default_lit};
    bool two_sided{};
    bool disable_depth_test{};
    bool used_with_instanced_static_meshes{};
    bool adopt_existing{};
    double opacity_mask_clip_value{0.3333};
};

struct MaterialIR {
    MaterialSettings settings;
    std::vector<Parameter> parameters;
    std::vector<Node> nodes;
    std::vector<NamedNode> bindings;
    std::vector<NamedNode> outputs;
    std::vector<std::string> texture_dependencies;
};

auto is_numeric(ValueType type) -> bool;
auto component_count(ValueType type) -> std::size_t;
auto validate(MaterialIR const& material) -> std::vector<Diagnostic>;

}
