#include <material_gen/MaterialFrontend.h>

#include "material_type_rules.h"

#include <codegen/sexpr/indices.h>
#include <codegen/sexpr/reader.h>
#include <codegen/sexpr/source_error.h>
#include <codegen/sexpr/source_span.h>
#include <codegen/sexpr/sources.h>
#include <codegen/sexpr/syntax.h>
#include <codegen/sexpr/token_kind.h>
#include <lispb/indices.h>
#include <lispb/project_loader.h>
#include <lispb/strings.h>
#include <material_gen/BlendMode.h>
#include <material_gen/CustomInputs.h>
#include <material_gen/Diagnostic.h>
#include <material_gen/Indices.h>
#include <material_gen/MaterialDomain.h>
#include <material_gen/MaterialIR.h>
#include <material_gen/NamedNodes.h>
#include <material_gen/NodeKind.h>
#include <material_gen/Nodes.h>
#include <material_gen/Parameters.h>
#include <material_gen/PositionSpace.h>
#include <material_gen/SceneTexture.h>
#include <material_gen/ShadingModel.h>
#include <material_gen/SourceSpan.h>
#include <material_gen/TextureResolver.h>
#include <material_gen/TextureSamplerType.h>
#include <material_gen/ValueType.h>

#include <algorithm>
#include <array>
#include <cassert>
#include <charconv>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <limits>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <unordered_map>
#include <utility>
#include <vector>

namespace material_synth {
namespace {

using lispb::codegen::sexpr::FormId;
using ParserSourceSpan = lispb::codegen::sexpr::SourceSpan;
using lispb::codegen::sexpr::TokenKind;

auto parse_number(lispb::codegen::sexpr::Syntax const& syntax, FormId const form)
    -> std::optional<double> {
    if (syntax.is_list(form) || syntax.kind(form) != TokenKind::atom) {
        return std::nullopt;
    }
    double value{};
    auto const* const begin{syntax.text(form).data()};
    auto const* const end{begin + syntax.text(form).size()};
    auto const [position, error]{std::from_chars(begin, end, value)};
    if (error != std::errc{} || position != end || !std::isfinite(value)) {
        return std::nullopt;
    }
    return value;
}

auto parse_type(std::string_view const name) -> ValueType {
    if (name == "float") {
        return ValueType::float1;
    }
    if (name == "float2") {
        return ValueType::float2;
    }
    if (name == "float3") {
        return ValueType::float3;
    }
    if (name == "float4") {
        return ValueType::float4;
    }
    if (name == "texture") {
        return ValueType::texture;
    }
    return ValueType::invalid;
}

class Analyzer {
  public:
    Analyzer(lispb::codegen::sexpr::Syntax& syntax,
             lispb::codegen::sexpr::SourceFileId const source,
             TextureResolver const resolver,
             MaterialIR& material)
        : syntax_{syntax}
        , source_{source}
        , resolver_{resolver}
        , material_{material} {
        assert(material.strings == &syntax.strings());
    }

    auto run() -> std::vector<Diagnostic> {
        lispb::codegen::sexpr::HandleRange forms;
        try {
            forms = lispb::codegen::sexpr::read_forms(syntax_, source_);
        } catch (lispb::codegen::sexpr::SourceError const& error) {
            diagnostics_.push_back({std::string{error.path()},
                                    error.location().line,
                                    error.location().column,
                                    std::string{error.message()}});
            return std::move(diagnostics_);
        } catch (std::exception const& error) {
            diagnostics_.push_back(
                {std::string{syntax_.sources().path(source_)}, 1, 1, error.what()});
            return std::move(diagnostics_);
        }

        if (forms.count != 1 || syntax_.head(syntax_.forms(forms).front()) != "material") {
            fail((forms.count == 0) ? ParserSourceSpan{.file = source_}
                                    : syntax_.token_span(syntax_.forms(forms).front()),
                 "source must contain exactly one material definition");
            return std::move(diagnostics_);
        }
        parse_material(syntax_.forms(forms).front());

        if (diagnostics_.empty()) {
            auto validation{validate(material_)};
            diagnostics_.insert(diagnostics_.end(), validation.begin(), validation.end());
        }
        if (!diagnostics_.empty()) {
            return std::move(diagnostics_);
        }
        return {};
    }
  private:
    auto material_span(ParserSourceSpan const& span) -> SourceSpan {
        auto const location{syntax_.sources().location(span)};
        return {location.line,
                location.column,
                syntax_.sources().path_id(span.file),
                material_.intern(syntax_.sources().expansion_text(span))};
    }

    void fail(ParserSourceSpan const& span, std::string message) {
        auto const location{syntax_.sources().location(span)};
        diagnostics_.push_back(
            {std::string{location.path}, location.line, location.column, std::move(message)});
    }

    auto atom(FormId const form, std::string_view const purpose)
        -> std::optional<std::string_view> {
        if (syntax_.is_list(form) || syntax_.kind(form) != TokenKind::atom) {
            fail(syntax_.token_span(form), std::string{purpose} + " must be a symbol");
            return std::nullopt;
        }
        return syntax_.text(form);
    }

    void parse_material(FormId const form) {
        if (syntax_.children(form).size() < 2) {
            fail(syntax_.token_span(form), "material requires a name");
            return;
        }
        auto const name{atom(syntax_.children(form)[1], "material name")};
        if (!name || !valid_identifier(*name)) {
            if (name) {
                fail(syntax_.token_span(syntax_.children(form)[1]), "invalid material identifier");
            }
            return;
        }
        material_.settings.name = lispb::string_id(syntax_.identifier(syntax_.children(form)[1]));

        std::set<lispb::IdentifierId> clauses;
        auto const count{syntax_.children(form).size()};
        for (std::size_t index{2}; index < count; ++index) {
            auto const& clause{syntax_.children(form)[index]};
            auto const head{syntax_.head(clause)};
            if (head.empty()) {
                fail(syntax_.token_span(clause),
                     "material clause must be a list beginning with a symbol");
                continue;
            }
            if (head == "asset" || head == "domain" || head == "blend" || head == "shading" ||
                head == "two-sided" || head == "disable-depth-test" || head == "usage" ||
                head == "adopt-existing" || head == "opacity-mask-clip") {
                if (!clauses.insert(syntax_.identifier(syntax_.children(clause).front())).second) {
                    fail(syntax_.token_span(clause),
                         "duplicate material setting '" + std::string{head} + "'");
                    continue;
                }
            }
            if (head == "asset") {
                parse_asset(clause);
            } else if (head == "domain") {
                parse_domain(clause);
            } else if (head == "blend") {
                parse_blend_mode(clause);
            } else if (head == "shading") {
                parse_shading_model(clause);
            } else if (head == "two-sided") {
                parse_boolean_setting(clause, "two-sided", material_.settings.two_sided);
            } else if (head == "disable-depth-test") {
                parse_boolean_setting(
                    clause, "disable-depth-test", material_.settings.disable_depth_test);
            } else if (head == "usage") {
                parse_usage(clause);
            } else if (head == "adopt-existing") {
                parse_boolean_setting(clause, "adopt-existing", material_.settings.adopt_existing);
            } else if (head == "opacity-mask-clip") {
                parse_opacity_mask_clip(clause);
            } else if (head == "parameter") {
                parse_parameter(clause);
            } else if (head == "let") {
                parse_binding(clause);
            } else if (head == "emissive") {
                parse_output(clause, "emissive");
            } else if (head == "opacity") {
                parse_output(clause, "opacity");
            } else if (head == "base-color") {
                parse_output(clause, "base-color");
            } else if (head == "opacity-mask") {
                parse_output(clause, "opacity-mask");
            } else if (head == "world-position-offset") {
                parse_output(clause, "world-position-offset");
            } else {
                fail(syntax_.token_span(clause),
                     "unknown material clause '" + std::string{head} + "'");
            }
        }
        for (auto const required : {"asset", "domain", "blend"}) {
            if (!clauses.contains(syntax_.strings().find(required))) {
                fail(syntax_.token_span(form),
                     "missing material setting '" + std::string{required} + "'");
            }
        }
    }

    void parse_asset(FormId const form) {
        if (syntax_.children(form).size() != 2 ||
            syntax_.kind(syntax_.children(form)[1]) != TokenKind::string) {
            fail(syntax_.token_span(form), "asset requires one quoted package path");
            return;
        }
        auto const& path{syntax_.text(syntax_.children(form)[1])};
        auto const leaf_position{path.rfind('/')};
        if (path.empty() || path.front() != '/' || path.find('/', 1) == std::string::npos ||
            leaf_position == std::string::npos || leaf_position == path.size() - 1 ||
            path.substr(leaf_position + 1) != material_.text(material_.settings.name) ||
            path.find('.', leaf_position) != std::string::npos) {
            fail(syntax_.token_span(syntax_.children(form)[1]),
                 "asset path must be /<mount>/<path>/<material-name>");
            return;
        }
        material_.settings.package_path = syntax_.text_id(syntax_.children(form)[1]);
    }

    auto setting_value(FormId const form, std::string_view const description)
        -> std::optional<std::string_view> {
        if (syntax_.children(form).size() != 2) {
            fail(syntax_.token_span(form), std::string{description} + " requires one value");
            return std::nullopt;
        }
        return atom(syntax_.children(form)[1], description);
    }

    void parse_domain(FormId const form) {
        auto const value{setting_value(form, "domain")};
        if (!value) {
            return;
        }
        if (*value == "ui") {
            material_.settings.domain = MaterialDomain::ui;
        } else if (*value == "surface") {
            material_.settings.domain = MaterialDomain::surface;
        } else if (*value == "post-process") {
            material_.settings.domain = MaterialDomain::post_process;
        } else {
            fail(syntax_.token_span(syntax_.children(form)[1]),
                 "unknown material domain '" + std::string{*value} + "'");
        }
    }

    void parse_blend_mode(FormId const form) {
        auto const value{setting_value(form, "blend mode")};
        if (!value) {
            return;
        }
        if (*value == "additive") {
            material_.settings.blend_mode = BlendMode::additive;
        } else if (*value == "translucent") {
            material_.settings.blend_mode = BlendMode::translucent;
        } else if (*value == "opaque") {
            material_.settings.blend_mode = BlendMode::opaque;
        } else if (*value == "masked") {
            material_.settings.blend_mode = BlendMode::masked;
        } else if (*value == "alpha-composite") {
            material_.settings.blend_mode = BlendMode::alpha_composite;
        } else {
            fail(syntax_.token_span(syntax_.children(form)[1]),
                 "unknown blend mode '" + std::string{*value} + "'");
        }
    }

    void parse_shading_model(FormId const form) {
        auto const value{setting_value(form, "shading model")};
        if (!value) {
            return;
        }
        if (*value == "default-lit") {
            material_.settings.shading_model = ShadingModel::default_lit;
        } else if (*value == "unlit") {
            material_.settings.shading_model = ShadingModel::unlit;
        } else {
            fail(syntax_.token_span(syntax_.children(form)[1]),
                 "unknown shading model '" + std::string{*value} + "'");
        }
    }

    void parse_boolean_setting(FormId const form, std::string_view const name, bool& setting) {
        auto const value{setting_value(form, name)};
        if (!value) {
            return;
        }
        if (*value == "true") {
            setting = true;
        } else if (*value == "false") {
            setting = false;
        } else {
            fail(syntax_.token_span(syntax_.children(form)[1]),
                 std::string{name} + " requires 'true' or 'false'");
        }
    }

    void parse_usage(FormId const form) {
        auto const value{setting_value(form, "usage")};
        if (!value) {
            return;
        }
        if (*value == "instanced-static-meshes") {
            material_.settings.used_with_instanced_static_meshes = true;
        } else if (*value == "particle-sprites") {
            material_.settings.used_with_particle_sprites = true;
        } else {
            fail(syntax_.token_span(syntax_.children(form)[1]),
                 "unknown material usage '" + std::string{*value} + "'");
        }
    }

    void parse_opacity_mask_clip(FormId const form) {
        if (syntax_.children(form).size() != 2) {
            fail(syntax_.token_span(form), "opacity-mask-clip requires one numeric value");
            return;
        }
        auto const value{parse_number(syntax_, syntax_.children(form)[1])};
        if (!value || *value < 0.0 || *value > 1.0) {
            fail(syntax_.token_span(syntax_.children(form)[1]),
                 "opacity-mask-clip must be a finite value from 0 to 1");
            return;
        }
        material_.settings.opacity_mask_clip_value = *value;
    }

    void parse_parameter(FormId const form) {
        if (syntax_.children(form).size() < 4) {
            fail(syntax_.token_span(form), "parameter requires a type, name, and default");
            return;
        }
        auto const type_name{atom(syntax_.children(form)[1], "parameter type")};
        auto const name{atom(syntax_.children(form)[2], "parameter name")};
        if (!type_name || !name || !valid_identifier(*name)) {
            if (name && !valid_identifier(*name)) {
                fail(syntax_.token_span(syntax_.children(form)[2]), "invalid parameter identifier");
            }
            return;
        }
        ValueType const type{*type_name == "scalar"    ? ValueType::float1
                             : *type_name == "color"   ? ValueType::float4
                             : *type_name == "texture" ? ValueType::texture
                                                       : ValueType::invalid};
        if (type == ValueType::invalid) {
            fail(syntax_.token_span(syntax_.children(form)[1]),
                 "unknown parameter type '" + std::string{*type_name} + "'");
            return;
        }
        if (symbols_.contains(syntax_.identifier(syntax_.children(form)[2]))) {
            fail(syntax_.token_span(syntax_.children(form)[2]),
                 "duplicate parameter or binding '" + std::string{*name} + "'");
            return;
        }
        auto const valid_size{type == ValueType::float4    ? syntax_.children(form).size() == 7
                              : type == ValueType::texture ? syntax_.children(form).size() == 4 ||
                                                                 syntax_.children(form).size() == 6
                                                           : syntax_.children(form).size() == 4};
        if (!valid_size) {
            fail(syntax_.token_span(form), "parameter default has the wrong arity");
            return;
        }

        std::array<double, 4> parameter_defaults{};
        lispb::StringId parameter_texture;
        auto parameter_sampler{TextureSamplerType::linear_color};
        if (type == ValueType::texture) {
            if (syntax_.kind(syntax_.children(form)[3]) != TokenKind::string) {
                fail(syntax_.token_span(syntax_.children(form)[3]),
                     "texture default must be a quoted asset path");
                return;
            }
            auto const resolved{resolve_texture(syntax_.children(form)[3])};
            if (!resolved) {
                return;
            }
            parameter_texture = *resolved;
            if (syntax_.children(form).size() == 6) {
                if (syntax_.kind(syntax_.children(form)[4]) != TokenKind::keyword ||
                    syntax_.text(syntax_.children(form)[4]) != "sampler") {
                    fail(syntax_.token_span(syntax_.children(form)[4]),
                         "texture parameter supports only the :sampler option");
                    return;
                }
                auto const sampler{atom(syntax_.children(form)[5], "texture sampler")};
                if (!sampler) {
                    return;
                }
                if (*sampler == "linear-color") {
                    parameter_sampler = TextureSamplerType::linear_color;
                } else if (*sampler == "linear-grayscale") {
                    parameter_sampler = TextureSamplerType::linear_grayscale;
                } else {
                    fail(syntax_.token_span(syntax_.children(form)[5]),
                         "unknown texture sampler '" + std::string{*sampler} + "'");
                    return;
                }
            }
        } else {
            auto const components{component_count(type)};
            for (std::size_t index{}; index < components; ++index) {
                auto const value{parse_number(syntax_, syntax_.children(form)[index + 3])};
                if (!value) {
                    fail(syntax_.token_span(syntax_.children(form)[index + 3]),
                         "parameter default must be finite numeric literals");
                    return;
                }
                parameter_defaults[index] = *value;
            }
        }

        auto const parameter{material_.parameters.append(
            lispb::string_id(syntax_.identifier(syntax_.children(form)[2])),
            type,
            parameter_texture,
            parameter_sampler,
            {},
            material_span(syntax_.token_span(form)),
            parameter_defaults)};
        auto const node{material_.nodes.add_parameter(
            type, material_span(syntax_.token_span(form)), {}, parameter, parameter_sampler)};
        material_.parameters.node[parameter.value] = node;
        symbols_.emplace(syntax_.identifier(syntax_.children(form)[2]), node);
    }

    void parse_binding(FormId const form) {
        if (syntax_.children(form).size() != 3) {
            fail(syntax_.token_span(form), "let requires a name and expression");
            return;
        }
        auto const name{atom(syntax_.children(form)[1], "binding name")};
        if (!name || !valid_identifier(*name)) {
            if (name) {
                fail(syntax_.token_span(syntax_.children(form)[1]), "invalid binding identifier");
            }
            return;
        }
        if (symbols_.contains(syntax_.identifier(syntax_.children(form)[1]))) {
            fail(syntax_.token_span(syntax_.children(form)[1]),
                 "binding shadowing is not allowed for '" + std::string{*name} + "'");
            return;
        }
        auto const node{expression(syntax_.children(form)[2])};
        if (!node) {
            return;
        }
        symbols_.emplace(syntax_.identifier(syntax_.children(form)[1]), *node);
        material_.bindings.append(lispb::string_id(syntax_.identifier(syntax_.children(form)[1])),
                                  *node,
                                  material_span(syntax_.token_span(form)));
    }

    void parse_output(FormId const form, std::string_view const name) {
        if (syntax_.children(form).size() != 2) {
            fail(syntax_.token_span(form), std::string{name} + " requires one expression");
            return;
        }
        if (std::ranges::any_of(material_.outputs.handles(), [&](NamedNodeId output) {
                return material_.text(material_.outputs.name[output.value]) == name;
            })) {
            fail(syntax_.token_span(form), "duplicate " + std::string{name} + " output");
            return;
        }
        auto const node{expression(syntax_.children(form)[1])};
        if (node) {
            material_.outputs.append(
                material_.intern(name), *node, material_span(syntax_.token_span(form)));
        }
    }

    auto expression(FormId const form) -> std::optional<NodeHandle> {
        if (!syntax_.is_list(form)) {
            if (auto const value{parse_number(syntax_, form)}) {
                return material_.nodes.add_constant(ValueType::float1,
                                                    material_span(syntax_.token_span(form)),
                                                    {},
                                                    std::array{*value});
            }
            if (syntax_.kind(form) == TokenKind::atom) {
                auto const found{symbols_.find(syntax_.identifier(form))};
                if (found != symbols_.end()) {
                    return found->second;
                }
                fail(syntax_.token_span(form),
                     "unknown symbol '" + std::string{syntax_.text(form)} + "'");
            } else {
                fail(syntax_.token_span(form), "expected a numeric literal, symbol, or expression");
            }
            return std::nullopt;
        }
        auto const head{syntax_.head(form)};
        if (head.empty()) {
            fail(syntax_.token_span(form), "expression must begin with an operation name");
            return std::nullopt;
        }
        if (head == "float2" || head == "float3" || head == "float4") {
            return vector(form, parse_type(head));
        }
        if (head == "+" || head == "-" || head == "*" || head == "/") {
            return arithmetic(form, head);
        }
        if (head == "lerp") {
            return lerp(form);
        }
        if (head == "saturate") {
            return saturate(form);
        }
        if (head == "step") {
            return step(form);
        }
        if (head == "time") {
            return time(form);
        }
        if (head == "sin" || head == "cos") {
            return trigonometric(form, head);
        }
        if (head == "texcoord") {
            return texcoord(form);
        }
        if (head == "per-instance-custom-data") {
            return per_instance_custom_data(form);
        }
        if (head == "sample") {
            return sample(form);
        }
        if (head == "texture") {
            return texture(form);
        }
        if (head == "swizzle") {
            return swizzle(form);
        }
        if (head == "world-position") {
            return standard_value(form, NodeKind::world_position, ValueType::float3);
        }
        if (head == "object-position") {
            return standard_value(form, NodeKind::object_position, ValueType::float3);
        }
        if (head == "pixel-normal") {
            return standard_value(form, NodeKind::pixel_normal, ValueType::float3);
        }
        if (head == "vertex-normal") {
            return standard_value(form, NodeKind::vertex_normal, ValueType::float3);
        }
        if (head == "camera-vector") {
            return standard_value(form, NodeKind::camera_vector, ValueType::float3);
        }
        if (head == "camera-position") {
            return standard_value(form, NodeKind::camera_position, ValueType::float3);
        }
        if (head == "vertex-color") {
            return standard_value(form, NodeKind::vertex_color, ValueType::float4);
        }
        if (head == "transform-position") {
            return transform_position(form);
        }
        if (head == "scene-texture") {
            return scene_texture(form);
        }
        if (head == "shader-call") {
            return shader_call(form);
        }
        if (head == "custom") {
            return custom(form);
        }
        fail(syntax_.token_span(form), "unknown expression form '" + std::string{head} + "'");
        return std::nullopt;
    }

    auto vector(FormId const form, ValueType const type) -> std::optional<NodeHandle> {
        auto const components{component_count(type)};
        if (syntax_.children(form).size() != components + 1) {
            fail(syntax_.token_span(form), "vector constructor has the wrong arity");
            return std::nullopt;
        }

        std::array<double, 4> values{};
        bool is_constant{true};
        for (std::size_t index{}; index < components; ++index) {
            auto const value{parse_number(syntax_, syntax_.children(form)[index + 1])};
            if (!value) {
                is_constant = false;
                break;
            }
            values[index] = *value;
        }
        if (is_constant) {
            return material_.nodes.add_constant(type,
                                                material_span(syntax_.token_span(form)),
                                                {},
                                                std::span{values}.first(components));
        }

        std::array<NodeHandle, 4> inputs;
        std::size_t input_count{};
        for (std::size_t index{}; index < components; ++index) {
            auto const component{expression(syntax_.children(form)[index + 1])};
            if (!component) {
                continue;
            }
            if (material_.nodes.type(*component) != ValueType::float1) {
                fail(syntax_.token_span(syntax_.children(form)[index + 1]),
                     "vector constructor components must be scalar values");
                continue;
            }
            inputs[input_count++] = *component;
        }
        if (input_count != components) {
            return std::nullopt;
        }
        return material_.nodes.add(NodeKind::vector_constructor,
                                   type,
                                   material_span(syntax_.token_span(form)),
                                   std::span{inputs}.first(components));
    }

    auto arithmetic(FormId const form, std::string_view const operation)
        -> std::optional<NodeHandle> {
        NodeKind const kind{operation == "+"   ? NodeKind::add
                            : operation == "-" ? NodeKind::subtract
                            : operation == "*" ? NodeKind::multiply
                                               : NodeKind::divide};
        auto const is_nary{operation == "+" || operation == "*"};
        if (syntax_.children(form).size() < 3 || (!is_nary && syntax_.children(form).size() != 3)) {
            fail(syntax_.token_span(form),
                 is_nary ? "addition and multiplication require at least two operands"
                         : "subtraction and division require two operands");
            return std::nullopt;
        }

        std::vector<NodeHandle> inputs;
        inputs.reserve(syntax_.children(form).size() - 1);
        for (std::size_t index{1}; index < syntax_.children(form).size(); ++index) {
            auto const input{expression(syntax_.children(form)[index])};
            if (input) {
                inputs.push_back(*input);
            }
        }
        if (inputs.size() != syntax_.children(form).size() - 1) {
            return std::nullopt;
        }

        auto type{material_.nodes.type(inputs.front())};
        for (std::size_t index{1}; index < inputs.size(); ++index) {
            type = promote_value_types(type, material_.nodes.type(inputs[index]));
        }
        if (type == ValueType::invalid) {
            fail(syntax_.token_span(form),
                 "arithmetic operands must be compatible scalar/vector values");
            return std::nullopt;
        }
        return material_.nodes.add(kind, type, material_span(syntax_.token_span(form)), inputs);
    }

    auto time(FormId const form) -> std::optional<NodeHandle> {
        if (syntax_.children(form).size() != 1) {
            fail(syntax_.token_span(form), "time requires no operands");
            return std::nullopt;
        }
        return material_.nodes.add(
            NodeKind::time, ValueType::float1, material_span(syntax_.token_span(form)), {});
    }

    auto trigonometric(FormId const form, std::string_view const operation)
        -> std::optional<NodeHandle> {
        auto const kind{operation == "sin" ? NodeKind::sine : NodeKind::cosine};
        if (syntax_.children(form).size() != 2) {
            fail(syntax_.token_span(form), std::string{operation} + " requires one operand");
            return std::nullopt;
        }
        auto const input{expression(syntax_.children(form)[1])};
        if (!input) {
            return std::nullopt;
        }
        if (material_.nodes.type(*input) != ValueType::float1) {
            fail(syntax_.token_span(form),
                 std::string{syntax_.head(form)} + " requires a scalar operand");
            return std::nullopt;
        }
        return material_.nodes.add(
            kind, ValueType::float1, material_span(syntax_.token_span(form)), std::array{*input});
    }

    auto lerp(FormId const form) -> std::optional<NodeHandle> {
        if (syntax_.children(form).size() != 4) {
            fail(syntax_.token_span(form), "lerp requires three operands");
            return std::nullopt;
        }
        auto const first{expression(syntax_.children(form)[1])};
        auto const second{expression(syntax_.children(form)[2])};
        auto const alpha{expression(syntax_.children(form)[3])};
        if (!first || !second || !alpha) {
            return std::nullopt;
        }
        auto const type{
            promote_value_types(material_.nodes.type(*first), material_.nodes.type(*second))};
        auto const alpha_type{material_.nodes.type(*alpha)};
        if (type == ValueType::invalid || (alpha_type != ValueType::float1 && alpha_type != type)) {
            fail(syntax_.token_span(form), "lerp operands are not compatible");
            return std::nullopt;
        }
        return material_.nodes.add(NodeKind::lerp,
                                   type,
                                   material_span(syntax_.token_span(form)),
                                   std::array{*first, *second, *alpha});
    }

    auto saturate(FormId const form) -> std::optional<NodeHandle> {
        if (syntax_.children(form).size() != 2) {
            fail(syntax_.token_span(form), "saturate requires one operand");
            return std::nullopt;
        }
        auto const input{expression(syntax_.children(form)[1])};
        if (!input) {
            return std::nullopt;
        }
        auto const type{material_.nodes.type(*input)};
        if (!is_numeric(type)) {
            fail(syntax_.token_span(form), "saturate requires a numeric operand");
            return std::nullopt;
        }
        return material_.nodes.add(
            NodeKind::saturate, type, material_span(syntax_.token_span(form)), std::array{*input});
    }

    auto step(FormId const form) -> std::optional<NodeHandle> {
        if (syntax_.children(form).size() != 3) {
            fail(syntax_.token_span(form), "step requires an edge and value");
            return std::nullopt;
        }
        auto const edge{expression(syntax_.children(form)[1])};
        auto const value{expression(syntax_.children(form)[2])};
        if (!edge || !value) {
            return std::nullopt;
        }
        if (material_.nodes.type(*edge) != ValueType::float1 ||
            material_.nodes.type(*value) != ValueType::float1) {
            fail(syntax_.token_span(form), "step requires scalar operands");
            return std::nullopt;
        }
        return material_.nodes.add(NodeKind::step,
                                   ValueType::float1,
                                   material_span(syntax_.token_span(form)),
                                   std::array{*edge, *value});
    }

    auto texcoord(FormId const form) -> std::optional<NodeHandle> {
        if (syntax_.children(form).size() != 2) {
            fail(syntax_.token_span(form), "texcoord requires one index");
            return std::nullopt;
        }
        auto const value{parse_number(syntax_, syntax_.children(form)[1])};
        if (!value || *value < 0.0 || *value > 7.0 || std::floor(*value) != *value) {
            fail(syntax_.token_span(syntax_.children(form)[1]),
                 "texture-coordinate index must be an integer from 0 to 7");
            return std::nullopt;
        }
        return material_.nodes.add_texture_coordinate(ValueType::float2,
                                                      material_span(syntax_.token_span(form)),
                                                      {},
                                                      static_cast<unsigned>(*value));
    }

    auto sample(FormId const form) -> std::optional<NodeHandle> {
        if (syntax_.children(form).size() != 3) {
            fail(syntax_.token_span(form), "sample requires a texture and float2 coordinates");
            return std::nullopt;
        }
        auto const texture{expression(syntax_.children(form)[1])};
        auto const coordinates{expression(syntax_.children(form)[2])};
        if (!texture || !coordinates) {
            return std::nullopt;
        }
        if (material_.nodes.type(*texture) != ValueType::texture ||
            material_.nodes.type(*coordinates) != ValueType::float2) {
            fail(syntax_.token_span(form), "sample requires arguments (texture, float2)");
            return std::nullopt;
        }
        return material_.nodes.add_sample(ValueType::float4,
                                          material_span(syntax_.token_span(form)),
                                          std::array{*texture, *coordinates},
                                          material_.nodes.texture_sampler_type(*texture));
    }

    auto resolve_texture(FormId const path_form) -> std::optional<lispb::StringId> {
        if (syntax_.kind(path_form) != TokenKind::string) {
            fail(syntax_.token_span(path_form), "texture path must be quoted");
            return std::nullopt;
        }
        auto resolved{resolver_.resolve == nullptr
                          ? std::nullopt
                          : resolver_.resolve(resolver_.context, syntax_.text(path_form))};
        if (!resolved) {
            fail(syntax_.token_span(path_form),
                 "unresolved texture asset '" + std::string{syntax_.text(path_form)} + "'");
            return std::nullopt;
        }
        auto const path{material_.intern(*resolved)};
        if (std::ranges::find(material_.texture_dependencies, path) ==
            material_.texture_dependencies.end()) {
            material_.texture_dependencies.push_back(path);
        }
        return path;
    }

    auto texture(FormId const form) -> std::optional<NodeHandle> {
        if (syntax_.children(form).size() != 2) {
            fail(syntax_.token_span(form), "texture requires one quoted asset path");
            return std::nullopt;
        }
        auto texture_path{resolve_texture(syntax_.children(form)[1])};
        if (!texture_path) {
            return std::nullopt;
        }
        return material_.nodes.add_texture_object(ValueType::texture,
                                                  material_span(syntax_.token_span(form)),
                                                  {},
                                                  *texture_path,
                                                  TextureSamplerType::linear_color);
    }

    auto swizzle(FormId const form) -> std::optional<NodeHandle> {
        if (syntax_.children(form).size() != 3) {
            fail(syntax_.token_span(form), "swizzle requires an expression and component mask");
            return std::nullopt;
        }
        auto const input{expression(syntax_.children(form)[1])};
        auto const mask{atom(syntax_.children(form)[2], "component mask")};
        if (!input || !mask) {
            return std::nullopt;
        }
        auto const input_components{component_count(material_.nodes.type(*input))};
        auto const valid_mask{!mask->empty() && mask->size() <= 4 &&
                              std::ranges::all_of(*mask, [](char const component) {
                                  return component == 'r' || component == 'g' || component == 'b' ||
                                         component == 'a';
                              })};
        std::size_t previous_component{};
        bool ordered{true};
        for (std::size_t index{}; valid_mask && index < mask->size(); ++index) {
            auto const component{std::string_view{"rgba"}.find((*mask)[index])};
            if (component >= input_components || (index != 0 && component <= previous_component)) {
                ordered = false;
                break;
            }
            previous_component = component;
        }
        if (!valid_mask || !ordered) {
            fail(syntax_.token_span(syntax_.children(form)[2]),
                 "component mask must select ordered, unique components present in the input");
            return std::nullopt;
        }
        auto const result_type{static_cast<ValueType>(static_cast<unsigned>(ValueType::float1) +
                                                      static_cast<unsigned>(mask->size() - 1))};
        return material_.nodes.add_component_mask(
            result_type,
            material_span(syntax_.token_span(form)),
            std::array{*input},
            lispb::string_id(syntax_.identifier(syntax_.children(form)[2])));
    }

    auto standard_value(FormId const form, NodeKind const kind, ValueType const type)
        -> std::optional<NodeHandle> {
        if (syntax_.children(form).size() != 1) {
            fail(syntax_.token_span(form), "standard material value requires no operands");
            return std::nullopt;
        }
        return material_.nodes.add(kind, type, material_span(syntax_.token_span(form)), {});
    }

    auto transform_position(FormId const form) -> std::optional<NodeHandle> {
        if (syntax_.children(form).size() != 4) {
            fail(syntax_.token_span(form),
                 "transform-position requires source space, destination space, and value");
            return std::nullopt;
        }
        auto const source{atom(syntax_.children(form)[1], "source position space")};
        auto const destination{atom(syntax_.children(form)[2], "destination position space")};
        if (!source || !destination) {
            return std::nullopt;
        }
        auto parse_space{[](std::string_view const space) -> std::optional<PositionSpace> {
            if (space == "world") {
                return PositionSpace::world;
            }
            if (space == "local") {
                return PositionSpace::local;
            }
            return std::nullopt;
        }};
        auto const source_space{parse_space(*source)};
        auto const destination_space{parse_space(*destination)};
        if (!source_space || !destination_space || *source_space == *destination_space) {
            fail(syntax_.token_span(form),
                 "transform-position requires two distinct supported spaces");
            return std::nullopt;
        }
        auto const input{expression(syntax_.children(form)[3])};
        if (!input) {
            return std::nullopt;
        }
        if (material_.nodes.type(*input) != ValueType::float3) {
            fail(syntax_.token_span(syntax_.children(form)[3]),
                 "transform-position value must be float3");
            return std::nullopt;
        }
        return material_.nodes.add_transform_position(ValueType::float3,
                                                      material_span(syntax_.token_span(form)),
                                                      std::array{*input},
                                                      *source_space,
                                                      *destination_space);
    }

    auto scene_texture(FormId const form) -> std::optional<NodeHandle> {
        auto const name{setting_value(form, "scene-texture")};
        if (!name) {
            return std::nullopt;
        }
        if (*name == "post-process-input0") {
            return material_.nodes.add_scene_texture(ValueType::float4,
                                                     material_span(syntax_.token_span(form)),
                                                     {},
                                                     SceneTexture::post_process_input0);
        }
        if (*name == "scene-depth") {
            return material_.nodes.add_scene_texture(ValueType::float1,
                                                     material_span(syntax_.token_span(form)),
                                                     {},
                                                     SceneTexture::scene_depth);
        }
        fail(syntax_.token_span(syntax_.children(form)[1]),
             "unsupported scene texture '" + std::string{*name} + "'");
        return std::nullopt;
    }

    auto shader_call(FormId const form) -> std::optional<NodeHandle> {
        if (syntax_.children(form).size() != 5) {
            fail(syntax_.token_span(form),
                 "shader-call requires result type, include path, function, and input list");
            return std::nullopt;
        }
        auto const result_name{atom(syntax_.children(form)[1], "shader-call result type")};
        auto const result_type{result_name ? parse_type(*result_name) : ValueType::invalid};
        auto const function{atom(syntax_.children(form)[3], "shader function")};
        auto const& path_form{syntax_.children(form)[2]};
        auto const& declarations{syntax_.children(form)[4]};
        if (!is_numeric(result_type)) {
            fail(syntax_.token_span(syntax_.children(form)[1]),
                 "shader-call result type must be float, float2, float3, or float4");
            return std::nullopt;
        }
        if (syntax_.kind(path_form) != TokenKind::string || syntax_.text(path_form).empty() ||
            syntax_.text(path_form).front() != '/' ||
            !(syntax_.text(path_form).ends_with(".ush") ||
              syntax_.text(path_form).ends_with(".usf"))) {
            fail(syntax_.token_span(path_form),
                 "shader-call requires an absolute virtual .ush or .usf path");
            return std::nullopt;
        }
        if (!function || !valid_identifier(*function)) {
            fail(syntax_.token_span(syntax_.children(form)[3]), "invalid shader function name");
            return std::nullopt;
        }
        if (!syntax_.is_list(declarations)) {
            fail(syntax_.token_span(declarations), "shader-call inputs must be a list");
            return std::nullopt;
        }

        std::vector<NodeHandle> inputs;
        std::vector<CustomInputId> custom_inputs;
        inputs.reserve(syntax_.children(declarations).size());
        custom_inputs.reserve(syntax_.children(declarations).size());
        std::set<lispb::IdentifierId> input_names;
        for (auto const& declaration : syntax_.children(declarations)) {
            if (!syntax_.is_list(declaration) || syntax_.children(declaration).size() != 3) {
                fail(syntax_.token_span(declaration),
                     "shader-call input must be (name type expression)");
                continue;
            }
            auto const name{atom(syntax_.children(declaration)[0], "shader-call input name")};
            auto const type_name{atom(syntax_.children(declaration)[1], "shader-call input type")};
            auto const type{type_name ? parse_type(*type_name) : ValueType::invalid};
            if (!name || !valid_identifier(*name) || type == ValueType::invalid ||
                !input_names.insert(syntax_.identifier(syntax_.children(declaration)[0])).second) {
                fail(syntax_.token_span(declaration),
                     "invalid or duplicate shader-call input declaration");
                continue;
            }
            auto const input{expression(syntax_.children(declaration)[2])};
            if (!input) {
                continue;
            }
            if (material_.nodes.type(*input) != type) {
                fail(syntax_.token_span(declaration),
                     "shader-call input expression does not match its declared type");
                continue;
            }
            inputs.push_back(*input);
            custom_inputs.push_back(material_.nodes.input_declarations.append(
                lispb::string_id(syntax_.identifier(syntax_.children(declaration)[0])), type));
        }
        if (custom_inputs.size() != syntax_.children(declarations).size()) {
            return std::nullopt;
        }
        return material_.nodes.add_shader_call(
            result_type,
            material_span(syntax_.token_span(form)),
            inputs,
            syntax_.text_id(path_form),
            lispb::string_id(syntax_.identifier(syntax_.children(form)[3])),
            custom_inputs);
    }

    auto per_instance_custom_data(FormId const form) -> std::optional<NodeHandle> {
        if (syntax_.children(form).size() != 2) {
            fail(syntax_.token_span(form), "per-instance-custom-data requires one index");
            return std::nullopt;
        }
        auto const value{parse_number(syntax_, syntax_.children(form)[1])};
        if (!value || *value < 0.0 ||
            *value > static_cast<double>(std::numeric_limits<std::int32_t>::max()) ||
            std::floor(*value) != *value) {
            fail(syntax_.token_span(syntax_.children(form)[1]),
                 "per-instance custom-data index must be a non-negative integer");
            return std::nullopt;
        }
        return material_.nodes.add_per_instance_custom_data(ValueType::float1,
                                                            material_span(syntax_.token_span(form)),
                                                            {},
                                                            static_cast<unsigned>(*value));
    }

    auto custom(FormId const form) -> std::optional<NodeHandle> {
        if (syntax_.children(form).size() < 4) {
            fail(syntax_.token_span(form), "custom requires a result type, input list, and HLSL");
            return std::nullopt;
        }
        auto const result_name{atom(syntax_.children(form)[1], "custom result type")};
        auto const result_type{result_name ? parse_type(*result_name) : ValueType::invalid};
        if (!is_numeric(result_type)) {
            fail(syntax_.token_span(syntax_.children(form)[1]),
                 "custom result type must be float, float2, float3, or float4");
            return std::nullopt;
        }
        auto const& declarations{syntax_.children(form)[2]};
        if (!syntax_.is_list(declarations)) {
            fail(syntax_.token_span(declarations), "custom inputs must be a list");
            return std::nullopt;
        }
        std::vector<NodeHandle> inputs;
        std::vector<CustomInputId> custom_inputs;
        inputs.reserve(syntax_.children(declarations).size());
        custom_inputs.reserve(syntax_.children(declarations).size());
        lispb::StringId description;
        std::set<lispb::IdentifierId> input_names;
        for (auto const& declaration : syntax_.children(declarations)) {
            if (!syntax_.is_list(declaration) || syntax_.children(declaration).size() != 3) {
                fail(syntax_.token_span(declaration),
                     "custom input must be (name type expression)");
                continue;
            }
            auto const name{atom(syntax_.children(declaration)[0], "custom input name")};
            auto const type_name{atom(syntax_.children(declaration)[1], "custom input type")};
            auto const type{type_name ? parse_type(*type_name) : ValueType::invalid};
            if (!name || !valid_identifier(*name) || !type_name || type == ValueType::invalid) {
                fail(syntax_.token_span(declaration), "invalid custom input declaration");
                continue;
            }
            if (!input_names.insert(syntax_.identifier(syntax_.children(declaration)[0])).second) {
                fail(syntax_.token_span(syntax_.children(declaration)[0]),
                     "duplicate custom input '" + std::string{*name} + "'");
                continue;
            }
            auto const input{expression(syntax_.children(declaration)[2])};
            if (!input) {
                continue;
            }
            if (material_.nodes.type(*input) != type) {
                fail(syntax_.token_span(declaration),
                     "custom input expression does not match its declared type");
                continue;
            }
            inputs.push_back(*input);
            custom_inputs.push_back(material_.nodes.input_declarations.append(
                lispb::string_id(syntax_.identifier(syntax_.children(declaration)[0])), type));
        }

        std::size_t index{3};
        if (index < syntax_.children(form).size() &&
            syntax_.kind(syntax_.children(form)[index]) == TokenKind::keyword) {
            if (syntax_.text(syntax_.children(form)[index]) != "description" ||
                index + 1 >= syntax_.children(form).size() ||
                syntax_.kind(syntax_.children(form)[index + 1]) != TokenKind::string) {
                fail(syntax_.token_span(syntax_.children(form)[index]),
                     "custom supports only :description followed by a string");
                return std::nullopt;
            }
            description = syntax_.text_id(syntax_.children(form)[index + 1]);
            index += 2;
        }
        std::string code;
        for (; index < syntax_.children(form).size(); ++index) {
            if (syntax_.kind(syntax_.children(form)[index]) != TokenKind::string) {
                fail(syntax_.token_span(syntax_.children(form)[index]),
                     "custom HLSL must be one or more strings");
                return std::nullopt;
            }
            code += syntax_.text(syntax_.children(form)[index]);
        }
        if (code.empty()) {
            fail(syntax_.token_span(form), "custom HLSL must not be empty");
            return std::nullopt;
        }
        return material_.nodes.add_custom(result_type,
                                          material_span(syntax_.token_span(form)),
                                          inputs,
                                          description,
                                          material_.strings->add(code),
                                          custom_inputs);
    }

    lispb::codegen::sexpr::Syntax& syntax_;
    lispb::codegen::sexpr::SourceFileId source_;
    TextureResolver resolver_;
    MaterialIR& material_;
    std::unordered_map<lispb::IdentifierId, NodeHandle> symbols_;
    std::vector<Diagnostic> diagnostics_;
};

}

auto analyze(lispb::codegen::sexpr::Syntax& syntax,
             lispb::codegen::sexpr::SourceFileId const source,
             TextureResolver const texture_resolver,
             MaterialIR& material) -> std::vector<Diagnostic> {
    return Analyzer{syntax, source, texture_resolver, material}.run();
}

}
