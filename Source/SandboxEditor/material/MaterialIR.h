#pragma once

#include <codegen/sexpr/syntax.h>

#if __has_include("CoreTypes.h")
#include "CoreTypes.h"
#endif

#include <array>
#include <cstddef>
#include <limits>
#include <string>
#include <vector>

#ifndef SANDBOXEDITOR_API
#define SANDBOXEDITOR_API
#endif

namespace material_synth {

enum class ValueType { invalid, float1, float2, float3, float4, texture };
enum class MaterialDomain { ui };
enum class BlendMode { additive };
enum class NodeKind {
    constant,
    parameter,
    texture_coordinate,
    add,
    subtract,
    multiply,
    divide,
    lerp,
    saturate,
    sample,
    custom,
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

struct CustomInput {
    std::string name;
    ValueType type{ValueType::invalid};
    NodeHandle node;
};

struct Node {
    NodeKind kind{NodeKind::constant};
    ValueType type{ValueType::invalid};
    std::vector<NodeHandle> inputs;
    codegen::sexpr::SourceSpan span;
    std::array<double, 4> constant{};
    std::size_t component_count{};
    std::size_t parameter_index{};
    unsigned coordinate_index{};
    std::string description;
    std::string code;
    std::vector<CustomInput> custom_inputs;
};

struct Parameter {
    std::string name;
    ValueType type{ValueType::invalid};
    std::array<double, 4> default_value{};
    std::string texture_path;
    NodeHandle node;
    codegen::sexpr::SourceSpan span;
};

struct NamedNode {
    std::string name;
    NodeHandle node;
    codegen::sexpr::SourceSpan span;
};

struct MaterialSettings {
    std::string name;
    std::string package_path;
    MaterialDomain domain{MaterialDomain::ui};
    BlendMode blend_mode{BlendMode::additive};
};

struct MaterialIR {
    MaterialSettings settings;
    std::vector<Parameter> parameters;
    std::vector<Node> nodes;
    std::vector<NamedNode> bindings;
    std::vector<NamedNode> outputs;
    std::vector<std::string> texture_dependencies;
};

SANDBOXEDITOR_API auto is_numeric(ValueType type) -> bool;
SANDBOXEDITOR_API auto component_count(ValueType type) -> std::size_t;
SANDBOXEDITOR_API auto validate(MaterialIR const& material) -> std::vector<Diagnostic>;

}
