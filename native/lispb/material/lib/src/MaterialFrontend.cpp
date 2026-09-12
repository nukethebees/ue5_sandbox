#include <material_gen/MaterialFrontend.h>

#include <codegen/sexpr/reader.h>

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <limits>
#include <map>
#include <set>
#include <utility>

namespace material_synth {
namespace {

using codegen::sexpr::Form;
using ParserSourceSpan = codegen::sexpr::SourceSpan;
using codegen::sexpr::TokenKind;

auto material_span(ParserSourceSpan const& span) -> SourceSpan {
    return {span.line, span.column, span.path, span.expansion};
}

auto is_identifier(std::string_view const value) -> bool {
    if (value.empty() ||
        !(std::isalpha(static_cast<unsigned char>(value.front())) != 0 || value.front() == '_')) {
        return false;
    }
    return std::ranges::all_of(value.substr(1), [](unsigned char const character) {
        return std::isalnum(character) != 0 || character == '_';
    });
}

auto parse_number(Form const& form) -> std::optional<double> {
    if (form.is_list() || form.token.kind != TokenKind::atom) {
        return std::nullopt;
    }
    double value{};
    auto const* const begin{form.token.text.data()};
    auto const* const end{begin + form.token.text.size()};
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

auto promote(ValueType const left, ValueType const right) -> ValueType {
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

class Analyzer {
  public:
    Analyzer(std::string_view const path, TextureResolver const resolver)
        : path_{path}
        , resolver_{resolver} {}

    auto run(std::string_view const source) -> AnalysisResult {
        std::vector<Form> forms;
        try {
            forms = codegen::sexpr::read_forms(path_, source);
        } catch (codegen::sexpr::SourceError const& error) {
            diagnostics_.push_back({std::string{error.path()},
                                    error.span().line,
                                    error.span().column,
                                    std::string{error.message()}});
            return {{}, std::move(diagnostics_)};
        } catch (std::exception const& error) {
            diagnostics_.push_back({std::string{path_}, 1, 1, error.what()});
            return {{}, std::move(diagnostics_)};
        }

        if (forms.size() != 1 || forms.front().head() != "material") {
            fail(forms.empty() ? ParserSourceSpan{1, 1, std::string{path_}, {}}
                               : forms.front().token.span,
                 "source must contain exactly one material definition");
            return {{}, std::move(diagnostics_)};
        }
        parse_material(forms.front());

        if (diagnostics_.empty()) {
            auto validation{validate(material_)};
            diagnostics_.insert(diagnostics_.end(), validation.begin(), validation.end());
        }
        if (!diagnostics_.empty()) {
            return {{}, std::move(diagnostics_)};
        }
        return {std::move(material_), {}};
    }
  private:
    void fail(ParserSourceSpan const& span, std::string message) {
        diagnostics_.push_back({span.path.empty() ? std::string{path_} : span.path,
                                span.line,
                                span.column,
                                std::move(message)});
    }

    auto atom(Form const& form, std::string_view const purpose) -> std::optional<std::string_view> {
        if (form.is_list() || form.token.kind != TokenKind::atom) {
            fail(form.token.span, std::string{purpose} + " must be a symbol");
            return std::nullopt;
        }
        return form.token.text;
    }

    void parse_material(Form const& form) {
        if (form.children.size() < 2) {
            fail(form.token.span, "material requires a name");
            return;
        }
        auto const name{atom(form.children[1], "material name")};
        if (!name || !is_identifier(*name)) {
            if (name) {
                fail(form.children[1].token.span, "invalid material identifier");
            }
            return;
        }
        material_.settings.name = *name;

        std::set<std::string> clauses;
        auto const count{form.children.size()};
        for (std::size_t index{2}; index < count; ++index) {
            auto const& clause{form.children[index]};
            auto const head{clause.head()};
            if (head.empty()) {
                fail(clause.token.span, "material clause must be a list beginning with a symbol");
                continue;
            }
            if (head == "asset" || head == "domain" || head == "blend" || head == "shading" ||
                head == "two-sided" || head == "disable-depth-test" || head == "usage" ||
                head == "adopt-existing" || head == "opacity-mask-clip") {
                if (!clauses.insert(std::string{head}).second) {
                    fail(clause.token.span,
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
                fail(clause.token.span, "unknown material clause '" + std::string{head} + "'");
            }
        }
        for (auto const required : {"asset", "domain", "blend"}) {
            if (!clauses.contains(required)) {
                fail(form.token.span, "missing material setting '" + std::string{required} + "'");
            }
        }
    }

    void parse_asset(Form const& form) {
        if (form.children.size() != 2 || form.children[1].token.kind != TokenKind::string) {
            fail(form.token.span, "asset requires one quoted package path");
            return;
        }
        auto const& path{form.children[1].token.text};
        auto const leaf_position{path.rfind('/')};
        if (path.empty() || path.front() != '/' || path.find('/', 1) == std::string::npos ||
            leaf_position == std::string::npos || leaf_position == path.size() - 1 ||
            path.substr(leaf_position + 1) != material_.settings.name ||
            path.find('.', leaf_position) != std::string::npos) {
            fail(form.children[1].token.span, "asset path must be /<mount>/<path>/<material-name>");
            return;
        }
        material_.settings.package_path = path;
    }

    auto setting_value(Form const& form, std::string_view const description)
        -> std::optional<std::string_view> {
        if (form.children.size() != 2) {
            fail(form.token.span, std::string{description} + " requires one value");
            return std::nullopt;
        }
        return atom(form.children[1], description);
    }

    void parse_domain(Form const& form) {
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
            fail(form.children[1].token.span,
                 "unknown material domain '" + std::string{*value} + "'");
        }
    }

    void parse_blend_mode(Form const& form) {
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
        } else {
            fail(form.children[1].token.span, "unknown blend mode '" + std::string{*value} + "'");
        }
    }

    void parse_shading_model(Form const& form) {
        auto const value{setting_value(form, "shading model")};
        if (!value) {
            return;
        }
        if (*value == "default-lit") {
            material_.settings.shading_model = ShadingModel::default_lit;
        } else if (*value == "unlit") {
            material_.settings.shading_model = ShadingModel::unlit;
        } else {
            fail(form.children[1].token.span,
                 "unknown shading model '" + std::string{*value} + "'");
        }
    }

    void parse_boolean_setting(Form const& form, std::string_view const name, bool& setting) {
        auto const value{setting_value(form, name)};
        if (!value) {
            return;
        }
        if (*value == "true") {
            setting = true;
        } else if (*value == "false") {
            setting = false;
        } else {
            fail(form.children[1].token.span, std::string{name} + " requires 'true' or 'false'");
        }
    }

    void parse_usage(Form const& form) {
        auto const value{setting_value(form, "usage")};
        if (!value) {
            return;
        }
        if (*value == "instanced-static-meshes") {
            material_.settings.used_with_instanced_static_meshes = true;
        } else {
            fail(form.children[1].token.span,
                 "unknown material usage '" + std::string{*value} + "'");
        }
    }

    void parse_opacity_mask_clip(Form const& form) {
        if (form.children.size() != 2) {
            fail(form.token.span, "opacity-mask-clip requires one numeric value");
            return;
        }
        auto const value{parse_number(form.children[1])};
        if (!value || *value < 0.0 || *value > 1.0) {
            fail(form.children[1].token.span,
                 "opacity-mask-clip must be a finite value from 0 to 1");
            return;
        }
        material_.settings.opacity_mask_clip_value = *value;
    }

    void parse_parameter(Form const& form) {
        if (form.children.size() < 4) {
            fail(form.token.span, "parameter requires a type, name, and default");
            return;
        }
        auto const type_name{atom(form.children[1], "parameter type")};
        auto const name{atom(form.children[2], "parameter name")};
        if (!type_name || !name || !is_identifier(*name)) {
            if (name && !is_identifier(*name)) {
                fail(form.children[2].token.span, "invalid parameter identifier");
            }
            return;
        }
        ValueType const type{*type_name == "scalar"    ? ValueType::float1
                             : *type_name == "color"   ? ValueType::float4
                             : *type_name == "texture" ? ValueType::texture
                                                       : ValueType::invalid};
        if (type == ValueType::invalid) {
            fail(form.children[1].token.span,
                 "unknown parameter type '" + std::string{*type_name} + "'");
            return;
        }
        if (symbols_.contains(std::string{*name})) {
            fail(form.children[2].token.span,
                 "duplicate parameter or binding '" + std::string{*name} + "'");
            return;
        }
        auto const expected_size{type == ValueType::float4 ? 7U : 4U};
        if (form.children.size() != expected_size) {
            fail(form.token.span, "parameter default has the wrong arity");
            return;
        }

        Parameter parameter{
            .name = std::string{*name}, .type = type, .span = material_span(form.token.span)};
        if (type == ValueType::texture) {
            if (form.children[3].token.kind != TokenKind::string) {
                fail(form.children[3].token.span, "texture default must be a quoted asset path");
                return;
            }
            auto const resolved{
                resolver_.resolve == nullptr
                    ? std::nullopt
                    : resolver_.resolve(resolver_.context, form.children[3].token.text)};
            if (!resolved) {
                fail(form.children[3].token.span,
                     "unresolved texture asset '" + form.children[3].token.text + "'");
                return;
            }
            parameter.texture_path = *resolved;
            if (std::ranges::find(material_.texture_dependencies, *resolved) ==
                material_.texture_dependencies.end()) {
                material_.texture_dependencies.push_back(*resolved);
            }
        } else {
            auto const components{component_count(type)};
            for (std::size_t index{}; index < components; ++index) {
                auto const value{parse_number(form.children[index + 3])};
                if (!value) {
                    fail(form.children[index + 3].token.span,
                         "parameter default must be finite numeric literals");
                    return;
                }
                parameter.default_value[index] = *value;
            }
        }

        parameter.node = add_node(Node{.kind = NodeKind::parameter,
                                       .type = type,
                                       .span = material_span(form.token.span),
                                       .parameter_index = material_.parameters.size()});
        symbols_.emplace(parameter.name, parameter.node);
        material_.parameters.push_back(std::move(parameter));
    }

    void parse_binding(Form const& form) {
        if (form.children.size() != 3) {
            fail(form.token.span, "let requires a name and expression");
            return;
        }
        auto const name{atom(form.children[1], "binding name")};
        if (!name || !is_identifier(*name)) {
            if (name) {
                fail(form.children[1].token.span, "invalid binding identifier");
            }
            return;
        }
        if (symbols_.contains(std::string{*name})) {
            fail(form.children[1].token.span,
                 "binding shadowing is not allowed for '" + std::string{*name} + "'");
            return;
        }
        auto const node{expression(form.children[2])};
        if (!node) {
            return;
        }
        symbols_.emplace(std::string{*name}, *node);
        material_.bindings.push_back({std::string{*name}, *node, material_span(form.token.span)});
    }

    void parse_output(Form const& form, std::string_view const name) {
        if (form.children.size() != 2) {
            fail(form.token.span, std::string{name} + " requires one expression");
            return;
        }
        if (std::ranges::any_of(material_.outputs,
                                [name](NamedNode const& output) { return output.name == name; })) {
            fail(form.token.span, "duplicate " + std::string{name} + " output");
            return;
        }
        auto const node{expression(form.children[1])};
        if (node) {
            material_.outputs.push_back({std::string{name}, *node, material_span(form.token.span)});
        }
    }

    auto expression(Form const& form) -> std::optional<NodeHandle> {
        if (!form.is_list()) {
            if (auto const value{parse_number(form)}) {
                Node node{.kind = NodeKind::constant,
                          .type = ValueType::float1,
                          .span = material_span(form.token.span),
                          .component_count = 1};
                node.constant[0] = *value;
                return add_node(std::move(node));
            }
            if (form.token.kind == TokenKind::atom) {
                auto const found{symbols_.find(form.token.text)};
                if (found != symbols_.end()) {
                    return found->second;
                }
                fail(form.token.span, "unknown symbol '" + form.token.text + "'");
            } else {
                fail(form.token.span, "expected a numeric literal, symbol, or expression");
            }
            return std::nullopt;
        }
        auto const head{form.head()};
        if (head.empty()) {
            fail(form.token.span, "expression must begin with an operation name");
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
        fail(form.token.span, "unknown expression form '" + std::string{head} + "'");
        return std::nullopt;
    }

    auto vector(Form const& form, ValueType const type) -> std::optional<NodeHandle> {
        auto const components{component_count(type)};
        if (form.children.size() != components + 1) {
            fail(form.token.span, "vector constructor has the wrong arity");
            return std::nullopt;
        }

        Node node{.kind = NodeKind::constant,
                  .type = type,
                  .span = material_span(form.token.span),
                  .component_count = components};
        bool is_constant{true};
        for (std::size_t index{}; index < components; ++index) {
            auto const value{parse_number(form.children[index + 1])};
            if (!value) {
                is_constant = false;
                break;
            }
            node.constant[index] = *value;
        }
        if (is_constant) {
            return add_node(std::move(node));
        }

        node = Node{.kind = NodeKind::vector_constructor,
                    .type = type,
                    .span = material_span(form.token.span)};
        for (std::size_t index{}; index < components; ++index) {
            auto const component{expression(form.children[index + 1])};
            if (!component) {
                continue;
            }
            if (material_.nodes[component->index].type != ValueType::float1) {
                fail(form.children[index + 1].token.span,
                     "vector constructor components must be scalar values");
                continue;
            }
            node.inputs.push_back(*component);
        }
        if (node.inputs.size() != components) {
            return std::nullopt;
        }
        return add_node(std::move(node));
    }

    auto arithmetic(Form const& form, std::string_view const operation)
        -> std::optional<NodeHandle> {
        auto const is_nary{operation == "+" || operation == "*"};
        if (form.children.size() < 3 || (!is_nary && form.children.size() != 3)) {
            fail(form.token.span,
                 is_nary ? "addition and multiplication require at least two operands"
                         : "subtraction and division require two operands");
            return std::nullopt;
        }

        std::vector<NodeHandle> inputs;
        inputs.reserve(form.children.size() - 1);
        for (std::size_t index{1}; index < form.children.size(); ++index) {
            auto const input{expression(form.children[index])};
            if (input) {
                inputs.push_back(*input);
            }
        }
        if (inputs.size() != form.children.size() - 1) {
            return std::nullopt;
        }

        auto type{material_.nodes[inputs.front().index].type};
        for (std::size_t index{1}; index < inputs.size(); ++index) {
            type = promote(type, material_.nodes[inputs[index].index].type);
        }
        if (type == ValueType::invalid) {
            fail(form.token.span, "arithmetic operands must be compatible scalar/vector values");
            return std::nullopt;
        }
        NodeKind const kind{operation == "+"   ? NodeKind::add
                            : operation == "-" ? NodeKind::subtract
                            : operation == "*" ? NodeKind::multiply
                                               : NodeKind::divide};
        return add_node(Node{.kind = kind,
                             .type = type,
                             .inputs = std::move(inputs),
                             .span = material_span(form.token.span)});
    }

    auto time(Form const& form) -> std::optional<NodeHandle> {
        if (form.children.size() != 1) {
            fail(form.token.span, "time requires no operands");
            return std::nullopt;
        }
        return add_node(Node{.kind = NodeKind::time,
                             .type = ValueType::float1,
                             .span = material_span(form.token.span)});
    }

    auto trigonometric(Form const& form, std::string_view const operation)
        -> std::optional<NodeHandle> {
        if (form.children.size() != 2) {
            fail(form.token.span, std::string{operation} + " requires one operand");
            return std::nullopt;
        }
        auto const input{expression(form.children[1])};
        if (!input) {
            return std::nullopt;
        }
        if (material_.nodes[input->index].type != ValueType::float1) {
            fail(form.token.span, std::string{operation} + " requires a scalar operand");
            return std::nullopt;
        }
        return add_node(Node{.kind = operation == "sin" ? NodeKind::sine : NodeKind::cosine,
                             .type = ValueType::float1,
                             .inputs = {*input},
                             .span = material_span(form.token.span)});
    }

    auto lerp(Form const& form) -> std::optional<NodeHandle> {
        if (form.children.size() != 4) {
            fail(form.token.span, "lerp requires three operands");
            return std::nullopt;
        }
        auto const first{expression(form.children[1])};
        auto const second{expression(form.children[2])};
        auto const alpha{expression(form.children[3])};
        if (!first || !second || !alpha) {
            return std::nullopt;
        }
        auto const type{
            promote(material_.nodes[first->index].type, material_.nodes[second->index].type)};
        auto const alpha_type{material_.nodes[alpha->index].type};
        if (type == ValueType::invalid || (alpha_type != ValueType::float1 && alpha_type != type)) {
            fail(form.token.span, "lerp operands are not compatible");
            return std::nullopt;
        }
        return add_node(Node{.kind = NodeKind::lerp,
                             .type = type,
                             .inputs = {*first, *second, *alpha},
                             .span = material_span(form.token.span)});
    }

    auto saturate(Form const& form) -> std::optional<NodeHandle> {
        if (form.children.size() != 2) {
            fail(form.token.span, "saturate requires one operand");
            return std::nullopt;
        }
        auto const input{expression(form.children[1])};
        if (!input) {
            return std::nullopt;
        }
        auto const type{material_.nodes[input->index].type};
        if (!is_numeric(type)) {
            fail(form.token.span, "saturate requires a numeric operand");
            return std::nullopt;
        }
        return add_node(Node{.kind = NodeKind::saturate,
                             .type = type,
                             .inputs = {*input},
                             .span = material_span(form.token.span)});
    }

    auto texcoord(Form const& form) -> std::optional<NodeHandle> {
        if (form.children.size() != 2) {
            fail(form.token.span, "texcoord requires one index");
            return std::nullopt;
        }
        auto const value{parse_number(form.children[1])};
        if (!value || *value < 0.0 || *value > 7.0 || std::floor(*value) != *value) {
            fail(form.children[1].token.span,
                 "texture-coordinate index must be an integer from 0 to 7");
            return std::nullopt;
        }
        return add_node(Node{.kind = NodeKind::texture_coordinate,
                             .type = ValueType::float2,
                             .span = material_span(form.token.span),
                             .coordinate_index = static_cast<unsigned>(*value)});
    }

    auto sample(Form const& form) -> std::optional<NodeHandle> {
        if (form.children.size() != 3) {
            fail(form.token.span, "sample requires a texture and float2 coordinates");
            return std::nullopt;
        }
        auto const texture{expression(form.children[1])};
        auto const coordinates{expression(form.children[2])};
        if (!texture || !coordinates) {
            return std::nullopt;
        }
        if (material_.nodes[texture->index].type != ValueType::texture ||
            material_.nodes[coordinates->index].type != ValueType::float2) {
            fail(form.token.span, "sample requires arguments (texture, float2)");
            return std::nullopt;
        }
        return add_node(Node{.kind = NodeKind::sample,
                             .type = ValueType::float4,
                             .inputs = {*texture, *coordinates},
                             .span = material_span(form.token.span)});
    }

    auto resolve_texture(Form const& path_form) -> std::optional<std::string> {
        if (path_form.token.kind != TokenKind::string) {
            fail(path_form.token.span, "texture path must be quoted");
            return std::nullopt;
        }
        auto const resolved{resolver_.resolve == nullptr
                                ? std::nullopt
                                : resolver_.resolve(resolver_.context, path_form.token.text)};
        if (!resolved) {
            fail(path_form.token.span, "unresolved texture asset '" + path_form.token.text + "'");
            return std::nullopt;
        }
        if (std::ranges::find(material_.texture_dependencies, *resolved) ==
            material_.texture_dependencies.end()) {
            material_.texture_dependencies.push_back(*resolved);
        }
        return resolved;
    }

    auto texture(Form const& form) -> std::optional<NodeHandle> {
        if (form.children.size() != 2) {
            fail(form.token.span, "texture requires one quoted asset path");
            return std::nullopt;
        }
        auto texture_path{resolve_texture(form.children[1])};
        if (!texture_path) {
            return std::nullopt;
        }
        return add_node(Node{.kind = NodeKind::texture_object,
                             .type = ValueType::texture,
                             .span = material_span(form.token.span),
                             .texture_path = std::move(*texture_path)});
    }

    auto swizzle(Form const& form) -> std::optional<NodeHandle> {
        if (form.children.size() != 3) {
            fail(form.token.span, "swizzle requires an expression and component mask");
            return std::nullopt;
        }
        auto const input{expression(form.children[1])};
        auto const mask{atom(form.children[2], "component mask")};
        if (!input || !mask) {
            return std::nullopt;
        }
        auto const input_components{component_count(material_.nodes[input->index].type)};
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
            fail(form.children[2].token.span,
                 "component mask must select ordered, unique components present in the input");
            return std::nullopt;
        }
        auto const result_type{static_cast<ValueType>(static_cast<unsigned>(ValueType::float1) +
                                                      static_cast<unsigned>(mask->size() - 1))};
        return add_node(Node{.kind = NodeKind::component_mask,
                             .type = result_type,
                             .inputs = {*input},
                             .span = material_span(form.token.span),
                             .component_mask = std::string{*mask}});
    }

    auto standard_value(Form const& form, NodeKind const kind, ValueType const type)
        -> std::optional<NodeHandle> {
        if (form.children.size() != 1) {
            fail(form.token.span, "standard material value requires no operands");
            return std::nullopt;
        }
        return add_node(Node{.kind = kind, .type = type, .span = material_span(form.token.span)});
    }

    auto transform_position(Form const& form) -> std::optional<NodeHandle> {
        if (form.children.size() != 4) {
            fail(form.token.span,
                 "transform-position requires source space, destination space, and value");
            return std::nullopt;
        }
        auto const source{atom(form.children[1], "source position space")};
        auto const destination{atom(form.children[2], "destination position space")};
        auto const input{expression(form.children[3])};
        if (!source || !destination || !input) {
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
            fail(form.token.span, "transform-position requires two distinct supported spaces");
            return std::nullopt;
        }
        if (material_.nodes[input->index].type != ValueType::float3) {
            fail(form.children[3].token.span, "transform-position value must be float3");
            return std::nullopt;
        }
        return add_node(Node{.kind = NodeKind::transform_position,
                             .type = ValueType::float3,
                             .inputs = {*input},
                             .span = material_span(form.token.span),
                             .source_space = *source_space,
                             .destination_space = *destination_space});
    }

    auto scene_texture(Form const& form) -> std::optional<NodeHandle> {
        auto const name{setting_value(form, "scene-texture")};
        if (!name) {
            return std::nullopt;
        }
        if (*name == "post-process-input0") {
            return add_node(Node{.kind = NodeKind::scene_texture,
                                 .type = ValueType::float4,
                                 .span = material_span(form.token.span),
                                 .scene_texture = SceneTexture::post_process_input0});
        }
        if (*name == "scene-depth") {
            return add_node(Node{.kind = NodeKind::scene_texture,
                                 .type = ValueType::float1,
                                 .span = material_span(form.token.span),
                                 .scene_texture = SceneTexture::scene_depth});
        }
        fail(form.children[1].token.span, "unsupported scene texture '" + std::string{*name} + "'");
        return std::nullopt;
    }

    auto shader_call(Form const& form) -> std::optional<NodeHandle> {
        if (form.children.size() != 5) {
            fail(form.token.span,
                 "shader-call requires result type, include path, function, and input list");
            return std::nullopt;
        }
        auto const result_name{atom(form.children[1], "shader-call result type")};
        auto const result_type{result_name ? parse_type(*result_name) : ValueType::invalid};
        auto const function{atom(form.children[3], "shader function")};
        auto const& path_form{form.children[2]};
        auto const& declarations{form.children[4]};
        if (!is_numeric(result_type)) {
            fail(form.children[1].token.span,
                 "shader-call result type must be float, float2, float3, or float4");
            return std::nullopt;
        }
        if (path_form.token.kind != TokenKind::string || path_form.token.text.empty() ||
            path_form.token.text.front() != '/' ||
            !(path_form.token.text.ends_with(".ush") || path_form.token.text.ends_with(".usf"))) {
            fail(path_form.token.span,
                 "shader-call requires an absolute virtual .ush or .usf path");
            return std::nullopt;
        }
        if (!function || !is_identifier(*function)) {
            fail(form.children[3].token.span, "invalid shader function name");
            return std::nullopt;
        }
        if (!declarations.is_list()) {
            fail(declarations.token.span, "shader-call inputs must be a list");
            return std::nullopt;
        }

        Node node{.kind = NodeKind::shader_call,
                  .type = result_type,
                  .span = material_span(form.token.span),
                  .shader_path = path_form.token.text,
                  .shader_function = std::string{*function}};
        std::set<std::string> input_names;
        for (auto const& declaration : declarations.children) {
            if (!declaration.is_list() || declaration.children.size() != 3) {
                fail(declaration.token.span, "shader-call input must be (name type expression)");
                continue;
            }
            auto const name{atom(declaration.children[0], "shader-call input name")};
            auto const type_name{atom(declaration.children[1], "shader-call input type")};
            auto const type{type_name ? parse_type(*type_name) : ValueType::invalid};
            if (!name || !is_identifier(*name) || type == ValueType::invalid ||
                !input_names.insert(std::string{*name}).second) {
                fail(declaration.token.span, "invalid or duplicate shader-call input declaration");
                continue;
            }
            auto const input{expression(declaration.children[2])};
            if (!input) {
                continue;
            }
            if (material_.nodes[input->index].type != type) {
                fail(declaration.token.span,
                     "shader-call input expression does not match its declared type");
                continue;
            }
            node.inputs.push_back(*input);
            node.custom_inputs.push_back({std::string{*name}, type, *input});
        }
        if (node.custom_inputs.size() != declarations.children.size()) {
            return std::nullopt;
        }
        return add_node(std::move(node));
    }

    auto per_instance_custom_data(Form const& form) -> std::optional<NodeHandle> {
        if (form.children.size() != 2) {
            fail(form.token.span, "per-instance-custom-data requires one index");
            return std::nullopt;
        }
        auto const value{parse_number(form.children[1])};
        if (!value || *value < 0.0 ||
            *value > static_cast<double>(std::numeric_limits<std::int32_t>::max()) ||
            std::floor(*value) != *value) {
            fail(form.children[1].token.span,
                 "per-instance custom-data index must be a non-negative integer");
            return std::nullopt;
        }
        return add_node(Node{.kind = NodeKind::per_instance_custom_data,
                             .type = ValueType::float1,
                             .span = material_span(form.token.span),
                             .instance_data_index = static_cast<unsigned>(*value)});
    }

    auto custom(Form const& form) -> std::optional<NodeHandle> {
        if (form.children.size() < 4) {
            fail(form.token.span, "custom requires a result type, input list, and HLSL");
            return std::nullopt;
        }
        auto const result_name{atom(form.children[1], "custom result type")};
        auto const result_type{result_name ? parse_type(*result_name) : ValueType::invalid};
        if (!is_numeric(result_type)) {
            fail(form.children[1].token.span,
                 "custom result type must be float, float2, float3, or float4");
            return std::nullopt;
        }
        auto const& declarations{form.children[2]};
        if (!declarations.is_list()) {
            fail(declarations.token.span, "custom inputs must be a list");
            return std::nullopt;
        }
        Node node{
            .kind = NodeKind::custom, .type = result_type, .span = material_span(form.token.span)};
        std::set<std::string> input_names;
        for (auto const& declaration : declarations.children) {
            if (!declaration.is_list() || declaration.children.size() != 3) {
                fail(declaration.token.span, "custom input must be (name type expression)");
                continue;
            }
            auto const name{atom(declaration.children[0], "custom input name")};
            auto const type_name{atom(declaration.children[1], "custom input type")};
            auto const type{type_name ? parse_type(*type_name) : ValueType::invalid};
            if (!name || !is_identifier(*name) || !type_name || type == ValueType::invalid) {
                fail(declaration.token.span, "invalid custom input declaration");
                continue;
            }
            if (!input_names.insert(std::string{*name}).second) {
                fail(declaration.children[0].token.span,
                     "duplicate custom input '" + std::string{*name} + "'");
                continue;
            }
            auto const input{expression(declaration.children[2])};
            if (!input) {
                continue;
            }
            if (material_.nodes[input->index].type != type) {
                fail(declaration.token.span,
                     "custom input expression does not match its declared type");
                continue;
            }
            node.inputs.push_back(*input);
            node.custom_inputs.push_back({std::string{*name}, type, *input});
        }

        std::size_t index{3};
        if (index < form.children.size() && form.children[index].token.kind == TokenKind::keyword) {
            if (form.children[index].token.text != "description" ||
                index + 1 >= form.children.size() ||
                form.children[index + 1].token.kind != TokenKind::string) {
                fail(form.children[index].token.span,
                     "custom supports only :description followed by a string");
                return std::nullopt;
            }
            node.description = form.children[index + 1].token.text;
            index += 2;
        }
        for (; index < form.children.size(); ++index) {
            if (form.children[index].token.kind != TokenKind::string) {
                fail(form.children[index].token.span, "custom HLSL must be one or more strings");
                return std::nullopt;
            }
            node.code += form.children[index].token.text;
        }
        if (node.code.empty()) {
            fail(form.token.span, "custom HLSL must not be empty");
            return std::nullopt;
        }
        return add_node(std::move(node));
    }

    auto add_node(Node node) -> NodeHandle {
        NodeHandle const handle{material_.nodes.size()};
        material_.nodes.push_back(std::move(node));
        return handle;
    }

    std::string_view path_;
    TextureResolver resolver_;
    MaterialIR material_;
    std::map<std::string, NodeHandle> symbols_;
    std::vector<Diagnostic> diagnostics_;
};

}

auto analyze(std::string_view const path,
             std::string_view const source,
             TextureResolver const texture_resolver) -> AnalysisResult {
    return Analyzer{path, texture_resolver}.run(source);
}

}
