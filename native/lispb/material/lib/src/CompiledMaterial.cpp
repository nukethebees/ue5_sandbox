#include <material_gen/CompiledMaterial.h>

#include <lispb/indices.h>
#include <lispb/strings.h>
#include <material_gen/BlendMode.h>
#include <material_gen/CustomInputs.h>
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
#include <material_gen/TextureSamplerType.h>
#include <material_gen/ValueType.h>

#include <algorithm>
#include <array>
#include <bit>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace material_synth {

CompiledMaterial::CompiledMaterial()
    : material{strings} {}

CompiledMaterial::CompiledMaterial(CompiledMaterial&& other) noexcept
    : strings{std::move(other.strings)}
    , source_path{other.source_path}
    , source_hash{other.source_hash}
    , material{std::move(other.material)} {
    material.strings = &strings;
}

auto CompiledMaterial::operator=(CompiledMaterial&& other) noexcept -> CompiledMaterial& {
    strings = std::move(other.strings);
    source_path = other.source_path;
    source_hash = other.source_hash;
    material = std::move(other.material);
    material.strings = &strings;
    return *this;
}

namespace {

inline constexpr std::array<std::uint8_t, 8> artifact_magic{'S', 'B', 'X', 'M', 'A', 'T', 'I', 'R'};
inline constexpr std::size_t maximum_string_size{16 * 1024 * 1024};
inline constexpr std::size_t maximum_collection_size{1024 * 1024};

class Writer {
  public:
    explicit Writer(lispb::Strings const& strings)
        : strings_{strings} {}
    void write_u8(std::uint8_t const value) { bytes_.push_back(value); }
    void write_u32(std::uint32_t const value) {
        for (unsigned shift{}; shift < 32; shift += 8) {
            write_u8(static_cast<std::uint8_t>(value >> shift));
        }
    }
    void write_u64(std::uint64_t const value) {
        for (unsigned shift{}; shift < 64; shift += 8) {
            write_u8(static_cast<std::uint8_t>(value >> shift));
        }
    }
    void write_size(std::size_t const value) { write_u64(static_cast<std::uint64_t>(value)); }
    void write_double(double const value) { write_u64(std::bit_cast<std::uint64_t>(value)); }
    void write_string(std::string_view const value) {
        write_size(value.size());
        bytes_.insert(bytes_.end(), value.begin(), value.end());
    }
    void write_string(lispb::StringId id) { write_string(strings_.view(id)); }
    void write_span(SourceSpan const& span) {
        write_size(span.line);
        write_size(span.column);
        write_string(span.path);
        write_string(span.expansion);
    }
    void write_handle(NodeHandle const handle) { write_size(handle.value); }
    auto finish() && -> std::vector<std::uint8_t> { return std::move(bytes_); }
  private:
    lispb::Strings const& strings_;
    std::vector<std::uint8_t> bytes_;
};

class Reader {
  public:
    explicit Reader(std::span<std::uint8_t const> const bytes, MaterialIR& material)
        : bytes_{bytes}
        , material_{material} {}

    auto read_u8() -> std::expected<std::uint8_t, std::string> {
        if (offset_ == bytes_.size()) {
            return std::unexpected{"unexpected end of artifact"};
        }
        return bytes_[offset_++];
    }
    auto read_u32() -> std::expected<std::uint32_t, std::string> {
        std::uint32_t value{};
        for (unsigned shift{}; shift < 32; shift += 8) {
            auto const byte{read_u8()};
            if (!byte) {
                return std::unexpected{byte.error()};
            }
            value |= static_cast<std::uint32_t>(*byte) << shift;
        }
        return value;
    }
    auto read_u64() -> std::expected<std::uint64_t, std::string> {
        std::uint64_t value{};
        for (unsigned shift{}; shift < 64; shift += 8) {
            auto const byte{read_u8()};
            if (!byte) {
                return std::unexpected{byte.error()};
            }
            value |= static_cast<std::uint64_t>(*byte) << shift;
        }
        return value;
    }
    auto read_size(std::size_t const maximum = std::numeric_limits<std::size_t>::max())
        -> std::expected<std::size_t, std::string> {
        auto const value{read_u64()};
        if (!value) {
            return std::unexpected{value.error()};
        }
        if (*value > maximum || *value > std::numeric_limits<std::size_t>::max()) {
            return std::unexpected{"artifact value exceeds its supported size"};
        }
        return static_cast<std::size_t>(*value);
    }
    auto read_double() -> std::expected<double, std::string> {
        auto const value{read_u64()};
        if (!value) {
            return std::unexpected{value.error()};
        }
        return std::bit_cast<double>(*value);
    }
    auto read_string(bool const reusable = true) -> std::expected<lispb::StringId, std::string> {
        auto const size{read_size(maximum_string_size)};
        if (!size) {
            return std::unexpected{size.error()};
        }
        if (*size > bytes_.size() - offset_) {
            return std::unexpected{"unexpected end of artifact string"};
        }
        auto const* const begin{reinterpret_cast<char const*>(bytes_.data() + offset_)};
        offset_ += *size;
        auto const value{std::string_view{begin, *size}};
        return reusable ? material_.intern(value) : material_.strings->add(value);
    }
    auto read_span() -> std::expected<SourceSpan, std::string> {
        auto const line{read_size(std::numeric_limits<std::uint32_t>::max())};
        auto const column{read_size(std::numeric_limits<std::uint32_t>::max())};
        auto path{read_string()};
        auto expansion{read_string()};
        if (!line || !column || !path || !expansion) {
            return std::unexpected{"malformed source span"};
        }
        return SourceSpan{static_cast<std::uint32_t>(*line),
                          static_cast<std::uint32_t>(*column),
                          *path,
                          *expansion};
    }
    auto read_handle() -> std::expected<NodeHandle, std::string> {
        auto const index{read_size(invalid_node_index)};
        if (!index) {
            return std::unexpected{index.error()};
        }
        return NodeHandle{static_cast<std::uint32_t>(*index)};
    }
    auto remaining() const -> std::size_t { return bytes_.size() - offset_; }
  private:
    std::span<std::uint8_t const> bytes_;
    MaterialIR& material_;
    std::size_t offset_{};
};

auto valid_source_hash(std::string_view const hash) -> bool {
    return hash.size() == 64 && std::ranges::all_of(hash, [](unsigned char const value) {
               return std::isxdigit(value);
           });
}

void write_node(Writer& writer, Nodes const& nodes, NodeHandle node) {
    auto const kind{nodes.kind(node)};
    auto const custom_inputs{kind == NodeKind::custom || kind == NodeKind::shader_call
                                 ? nodes.custom_inputs(node)
                                 : std::span<CustomInputId const>{}};
    writer.write_u8(static_cast<std::uint8_t>(kind));
    writer.write_u8(static_cast<std::uint8_t>(nodes.type(node)));
    writer.write_size(nodes.inputs(node).size());
    for (auto const input : nodes.inputs(node)) {
        writer.write_handle(input);
    }
    writer.write_span(nodes.span(node));
    for (std::size_t component{}; component < 4; ++component) {
        writer.write_double(kind == NodeKind::constant ? nodes.constant(node, component) : 0.0);
    }
    writer.write_size((kind == NodeKind::constant ? nodes.component_count(node) : 0));
    writer.write_size((kind == NodeKind::parameter ? nodes.parameter(node).value : 0));
    writer.write_u32((kind == NodeKind::texture_coordinate ? nodes.coordinate_index(node) : 0));
    writer.write_u32(
        (kind == NodeKind::per_instance_custom_data ? nodes.instance_data_index(node) : 0));
    writer.write_u8(static_cast<std::uint8_t>(
        (kind == NodeKind::parameter || kind == NodeKind::sample || kind == NodeKind::texture_object
             ? nodes.texture_sampler_type(node)
             : TextureSamplerType::linear_color)));
    writer.write_string((kind == NodeKind::custom ? nodes.description(node) : lispb::StringId{}));
    writer.write_string((kind == NodeKind::custom ? nodes.code(node) : lispb::StringId{}));
    writer.write_size(custom_inputs.size());
    for (std::size_t index{}; index < custom_inputs.size(); ++index) {
        auto const& input{custom_inputs[index]};
        writer.write_string(nodes.input_declarations.name[input.value]);
        writer.write_u8(static_cast<std::uint8_t>(nodes.input_declarations.type[input.value]));
        writer.write_handle(nodes.inputs(node)[index]);
    }
    writer.write_string(
        (kind == NodeKind::texture_object ? nodes.texture_path(node) : lispb::StringId{}));
    writer.write_string(
        (kind == NodeKind::component_mask ? nodes.component_mask(node) : lispb::StringId{}));
    writer.write_u8(static_cast<std::uint8_t>(
        (kind == NodeKind::transform_position ? nodes.source_space(node) : PositionSpace::world)));
    writer.write_u8(static_cast<std::uint8_t>((kind == NodeKind::transform_position
                                                   ? nodes.destination_space(node)
                                                   : PositionSpace::local)));
    writer.write_u8(static_cast<std::uint8_t>((kind == NodeKind::scene_texture
                                                   ? nodes.scene_texture(node)
                                                   : SceneTexture::post_process_input0)));
    writer.write_string(
        (kind == NodeKind::shader_call ? nodes.shader_path(node) : lispb::StringId{}));
    writer.write_string(
        (kind == NodeKind::shader_call ? nodes.shader_function(node) : lispb::StringId{}));
}

auto read_value_type(Reader& reader) -> std::expected<ValueType, std::string> {
    auto const value{reader.read_u8()};
    if (!value || *value == static_cast<std::uint8_t>(ValueType::invalid) ||
        *value > static_cast<std::uint8_t>(ValueType::texture)) {
        return std::unexpected{"invalid material value type"};
    }
    return static_cast<ValueType>(*value);
}

auto read_texture_sampler_type(Reader& reader) -> std::expected<TextureSamplerType, std::string> {
    auto const value{reader.read_u8()};
    if (!value || *value > static_cast<std::uint8_t>(TextureSamplerType::linear_grayscale)) {
        return std::unexpected{"invalid texture sampler type"};
    }
    return static_cast<TextureSamplerType>(*value);
}

auto read_node(Reader& reader, Nodes& nodes) -> std::expected<void, std::string> {
    auto const kind{reader.read_u8()};
    auto const type{read_value_type(reader)};
    auto const input_count{reader.read_size(maximum_collection_size)};
    if (!kind || *kind > static_cast<std::uint8_t>(NodeKind::vertex_color) || !type ||
        !input_count) {
        return std::unexpected{"invalid material node header"};
    }

    std::vector<NodeHandle> inputs;
    inputs.reserve(*input_count);
    for (std::size_t index{}; index < *input_count; ++index) {
        auto const input{reader.read_handle()};
        if (!input) {
            return std::unexpected{input.error()};
        }
        inputs.push_back(*input);
    }
    auto span{reader.read_span()};
    if (!span) {
        return std::unexpected{span.error()};
    }
    std::array<double, 4> values{};
    for (double& value : values) {
        auto const stored{reader.read_double()};
        if (!stored) {
            return std::unexpected{stored.error()};
        }
        value = *stored;
    }
    auto const components{reader.read_size(4)};
    auto const parameter{reader.read_size(maximum_collection_size)};
    auto const coordinate{reader.read_u32()};
    auto const instance_data{reader.read_u32()};
    auto const texture_sampler{read_texture_sampler_type(reader)};
    auto description{reader.read_string(false)};
    auto code{reader.read_string(false)};
    auto const custom_input_count{reader.read_size(maximum_collection_size)};
    if (!components || !parameter || !coordinate || !instance_data || !texture_sampler ||
        !description || !code || !custom_input_count) {
        return std::unexpected{"malformed material node payload"};
    }
    std::vector<CustomInputId> custom_inputs;
    custom_inputs.reserve(*custom_input_count);
    for (std::size_t index{}; index < *custom_input_count; ++index) {
        auto name{reader.read_string()};
        auto const input_type{read_value_type(reader)};
        auto const input_node{reader.read_handle()};
        if (!name || !input_type || !input_node || index >= inputs.size() ||
            *input_node != inputs[index]) {
            return std::unexpected{"malformed custom input"};
        }
        custom_inputs.push_back(nodes.input_declarations.append(*name, *input_type));
    }
    auto texture_path{reader.read_string()};
    auto component_mask{reader.read_string()};
    auto const source_space{reader.read_u8()};
    auto const destination_space{reader.read_u8()};
    auto const scene_texture{reader.read_u8()};
    auto shader_path{reader.read_string()};
    auto shader_function{reader.read_string()};
    if (!texture_path || !component_mask || !source_space || !destination_space || !scene_texture ||
        !shader_path || !shader_function ||
        *source_space > static_cast<std::uint8_t>(PositionSpace::local) ||
        *destination_space > static_cast<std::uint8_t>(PositionSpace::local) ||
        *scene_texture > static_cast<std::uint8_t>(SceneTexture::scene_depth)) {
        return std::unexpected{"malformed extended material node payload"};
    }
    // Consume the fixed wire payload, then retain only the active kind's columns.
    switch (static_cast<NodeKind>(*kind)) {
        case NodeKind::constant:
            nodes.add_constant(*type, *span, inputs, std::span{values}.first(*components));
            break;
        case NodeKind::parameter:
            nodes.add_parameter(*type,
                                *span,
                                inputs,
                                ParameterId{static_cast<std::uint32_t>(*parameter)},
                                *texture_sampler);
            break;
        case NodeKind::texture_coordinate:
            nodes.add_texture_coordinate(*type, *span, inputs, *coordinate);
            break;
        case NodeKind::per_instance_custom_data:
            nodes.add_per_instance_custom_data(*type, *span, inputs, *instance_data);
            break;
        case NodeKind::sample:
            nodes.add_sample(*type, *span, inputs, *texture_sampler);
            break;
        case NodeKind::custom:
            nodes.add_custom(*type, *span, inputs, *description, *code, custom_inputs);
            break;
        case NodeKind::texture_object:
            nodes.add_texture_object(*type, *span, inputs, *texture_path, *texture_sampler);
            break;
        case NodeKind::component_mask:
            nodes.add_component_mask(*type, *span, inputs, *component_mask);
            break;
        case NodeKind::transform_position:
            nodes.add_transform_position(*type,
                                         *span,
                                         inputs,
                                         static_cast<PositionSpace>(*source_space),
                                         static_cast<PositionSpace>(*destination_space));
            break;
        case NodeKind::scene_texture:
            nodes.add_scene_texture(
                *type, *span, inputs, static_cast<SceneTexture>(*scene_texture));
            break;
        case NodeKind::shader_call:
            nodes.add_shader_call(
                *type, *span, inputs, *shader_path, *shader_function, custom_inputs);
            break;
        default:
            nodes.add(static_cast<NodeKind>(*kind), *type, *span, inputs);
            break;
    }
    return {};
}

void write_named_node(Writer& writer, NamedNodes const& nodes, NamedNodeId named) {
    writer.write_string(nodes.name[named.value]);
    writer.write_handle(nodes.node[named.value]);
    writer.write_span(nodes.span[named.value]);
}

auto read_named_node(Reader& reader, NamedNodes& nodes) -> std::expected<void, std::string> {
    auto name{reader.read_string()};
    auto const node{reader.read_handle()};
    auto span{reader.read_span()};
    if (!name || !node || !span) {
        return std::unexpected{"malformed named node"};
    }
    nodes.append(*name, *node, *span);
    return {};
}

}

auto serialize(MaterialIR const& material,
               std::string_view const source_path,
               std::string_view const source_hash)
    -> std::expected<std::vector<std::uint8_t>, std::string> {
    if (source_path.empty()) {
        return std::unexpected{"compiled material source path is empty"};
    }
    if (!valid_source_hash(source_hash)) {
        return std::unexpected{"compiled material source hash must be a 64-digit SHA-256 hash"};
    }
    Writer writer{*material.strings};
    for (auto const byte : artifact_magic) {
        writer.write_u8(byte);
    }
    writer.write_u32(compiled_material_version);
    writer.write_string(source_path);
    writer.write_string(source_hash);
    writer.write_string(material.settings.name);
    writer.write_string(material.settings.package_path);
    writer.write_u8(static_cast<std::uint8_t>(material.settings.domain));
    writer.write_u8(static_cast<std::uint8_t>(material.settings.blend_mode));
    writer.write_u8(static_cast<std::uint8_t>(material.settings.shading_model));
    writer.write_u8(material.settings.two_sided ? 1 : 0);
    writer.write_u8(material.settings.disable_depth_test ? 1 : 0);
    writer.write_u8(material.settings.used_with_instanced_static_meshes ? 1 : 0);
    writer.write_u8(material.settings.used_with_particle_sprites ? 1 : 0);
    writer.write_u8(material.settings.adopt_existing ? 1 : 0);
    writer.write_double(material.settings.opacity_mask_clip_value);

    writer.write_size(material.parameters.size());
    for (auto const parameter : material.parameters.handles()) {
        writer.write_string(material.parameters.name[parameter.value]);
        writer.write_u8(static_cast<std::uint8_t>(material.parameters.type[parameter.value]));
        writer.write_u8(
            static_cast<std::uint8_t>(material.parameters.texture_sampler_type[parameter.value]));
        for (std::size_t component{}; component < 4; ++component) {
            writer.write_double(material.parameters.default_values[component][parameter.value]);
        }
        writer.write_string(material.parameters.texture_path[parameter.value]);
        writer.write_handle(material.parameters.node[parameter.value]);
        writer.write_span(material.parameters.span[parameter.value]);
    }
    writer.write_size(material.nodes.size());
    for (auto const node : material.nodes.handles()) {
        write_node(writer, material.nodes, node);
    }
    writer.write_size(material.bindings.size());
    for (auto const binding : material.bindings.handles()) {
        write_named_node(writer, material.bindings, binding);
    }
    writer.write_size(material.outputs.size());
    for (auto const output : material.outputs.handles()) {
        write_named_node(writer, material.outputs, output);
    }
    writer.write_size(material.texture_dependencies.size());
    for (auto const& dependency : material.texture_dependencies) {
        writer.write_string(dependency);
    }
    return std::move(writer).finish();
}

auto deserialize(std::span<std::uint8_t const> const bytes)
    -> std::expected<CompiledMaterial, std::string> {
    CompiledMaterial compiled;
    Reader reader{bytes, compiled.material};
    for (auto const expected : artifact_magic) {
        auto const actual{reader.read_u8()};
        if (!actual || *actual != expected) {
            return std::unexpected{"not a MaterialGen compiled artifact"};
        }
    }
    auto const version{reader.read_u32()};
    if (!version || *version != compiled_material_version) {
        return std::unexpected{"unsupported MaterialGen artifact version"};
    }

    auto source_path{reader.read_string()};
    auto source_hash{reader.read_string()};
    auto name{reader.read_string()};
    auto package_path{reader.read_string()};
    auto const domain{reader.read_u8()};
    auto const blend_mode{reader.read_u8()};
    auto const shading_model{reader.read_u8()};
    auto const two_sided{reader.read_u8()};
    auto const disable_depth_test{reader.read_u8()};
    auto const used_with_instances{reader.read_u8()};
    auto const used_with_particle_sprites{reader.read_u8()};
    auto const adopt_existing{reader.read_u8()};
    auto const opacity_mask_clip_value{reader.read_double()};
    if (!source_path || !source_hash || !name || !package_path || !domain || !blend_mode ||
        !shading_model || !two_sided || !disable_depth_test || !used_with_instances ||
        !used_with_particle_sprites || !adopt_existing || !opacity_mask_clip_value ||
        *domain > static_cast<std::uint8_t>(MaterialDomain::post_process) ||
        *blend_mode > static_cast<std::uint8_t>(BlendMode::alpha_composite) ||
        *shading_model > static_cast<std::uint8_t>(ShadingModel::unlit) || *two_sided > 1 ||
        *disable_depth_test > 1 || *used_with_instances > 1 || *used_with_particle_sprites > 1 ||
        *adopt_existing > 1) {
        return std::unexpected{"malformed compiled material settings"};
    }
    compiled.source_path = *source_path;
    compiled.source_hash = *source_hash;
    compiled.material.settings = {.name = *name,
                                  .package_path = *package_path,
                                  .domain = static_cast<MaterialDomain>(*domain),
                                  .blend_mode = static_cast<BlendMode>(*blend_mode),
                                  .shading_model = static_cast<ShadingModel>(*shading_model),
                                  .two_sided = *two_sided != 0,
                                  .disable_depth_test = *disable_depth_test != 0,
                                  .used_with_instanced_static_meshes = *used_with_instances != 0,
                                  .used_with_particle_sprites = *used_with_particle_sprites != 0,
                                  .adopt_existing = *adopt_existing != 0,
                                  .opacity_mask_clip_value = *opacity_mask_clip_value};
    if (compiled.material.text(compiled.source_path).empty() ||
        !valid_source_hash(compiled.material.text(compiled.source_hash))) {
        return std::unexpected{"invalid compiled material source metadata"};
    }

    auto const parameter_count{reader.read_size(maximum_collection_size)};
    if (!parameter_count) {
        return std::unexpected{parameter_count.error()};
    }
    for (std::size_t index{}; index < *parameter_count; ++index) {
        auto parameter_name{reader.read_string()};
        auto const type{read_value_type(reader)};
        auto const texture_sampler{read_texture_sampler_type(reader)};
        if (!parameter_name || !type || !texture_sampler) {
            return std::unexpected{"malformed material parameter"};
        }
        std::array<double, 4> parameter_defaults{};
        for (double& value : parameter_defaults) {
            auto const stored{reader.read_double()};
            if (!stored) {
                return std::unexpected{stored.error()};
            }
            value = *stored;
        }
        auto texture_path{reader.read_string()};
        auto const node{reader.read_handle()};
        auto span{reader.read_span()};
        if (!texture_path || !node || !span) {
            return std::unexpected{"malformed material parameter payload"};
        }
        compiled.material.parameters.append(*parameter_name,
                                            *type,
                                            *texture_path,
                                            *texture_sampler,
                                            *node,
                                            *span,
                                            parameter_defaults);
    }

    auto const node_count{reader.read_size(maximum_collection_size)};
    if (!node_count) {
        return std::unexpected{node_count.error()};
    }
    for (std::size_t index{}; index < *node_count; ++index) {
        auto node{read_node(reader, compiled.material.nodes)};
        if (!node) {
            return std::unexpected{node.error()};
        }
    }

    auto read_named_nodes{[&](NamedNodes& nodes) -> std::expected<void, std::string> {
        auto const count{reader.read_size(maximum_collection_size)};
        if (!count) {
            return std::unexpected{count.error()};
        }
        for (std::size_t index{}; index < *count; ++index) {
            auto node{read_named_node(reader, nodes)};
            if (!node) {
                return std::unexpected{node.error()};
            }
        }
        return {};
    }};
    if (auto result{read_named_nodes(compiled.material.bindings)}; !result) {
        return std::unexpected{result.error()};
    }
    if (auto result{read_named_nodes(compiled.material.outputs)}; !result) {
        return std::unexpected{result.error()};
    }

    auto const dependency_count{reader.read_size(maximum_collection_size)};
    if (!dependency_count) {
        return std::unexpected{dependency_count.error()};
    }
    compiled.material.texture_dependencies.reserve(*dependency_count);
    for (std::size_t index{}; index < *dependency_count; ++index) {
        auto dependency{reader.read_string()};
        if (!dependency) {
            return std::unexpected{dependency.error()};
        }
        compiled.material.texture_dependencies.push_back(*dependency);
    }
    if (reader.remaining() != 0) {
        return std::unexpected{"compiled material artifact has trailing data"};
    }
    auto const diagnostics{validate(compiled.material)};
    if (!diagnostics.empty()) {
        return std::unexpected{"compiled material contains invalid IR: " +
                               diagnostics.front().message};
    }
    return compiled;
}

}
