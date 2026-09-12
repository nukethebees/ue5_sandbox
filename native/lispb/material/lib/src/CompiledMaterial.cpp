#include <material_gen/CompiledMaterial.h>

#include <algorithm>
#include <array>
#include <bit>
#include <cctype>
#include <cstdint>
#include <limits>
#include <string_view>
#include <utility>

namespace material_synth {
namespace {

inline constexpr std::array<std::uint8_t, 8> artifact_magic{'S', 'B', 'X', 'M', 'A', 'T', 'I', 'R'};
inline constexpr std::size_t maximum_string_size{16 * 1024 * 1024};
inline constexpr std::size_t maximum_collection_size{1024 * 1024};

class Writer {
  public:
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
    void write_span(SourceSpan const& span) {
        write_size(span.line);
        write_size(span.column);
        write_string(span.path);
        write_string(span.expansion);
    }
    void write_handle(NodeHandle const handle) { write_size(handle.index); }
    auto finish() && -> std::vector<std::uint8_t> { return std::move(bytes_); }
  private:
    std::vector<std::uint8_t> bytes_;
};

class Reader {
  public:
    explicit Reader(std::span<std::uint8_t const> const bytes)
        : bytes_{bytes} {}

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
    auto read_string() -> std::expected<std::string, std::string> {
        auto const size{read_size(maximum_string_size)};
        if (!size) {
            return std::unexpected{size.error()};
        }
        if (*size > bytes_.size() - offset_) {
            return std::unexpected{"unexpected end of artifact string"};
        }
        auto const* const begin{reinterpret_cast<char const*>(bytes_.data() + offset_)};
        offset_ += *size;
        return std::string{begin, *size};
    }
    auto read_span() -> std::expected<SourceSpan, std::string> {
        auto const line{read_size()};
        auto const column{read_size()};
        auto path{read_string()};
        auto expansion{read_string()};
        if (!line || !column || !path || !expansion) {
            return std::unexpected{"malformed source span"};
        }
        return SourceSpan{*line, *column, std::move(*path), std::move(*expansion)};
    }
    auto read_handle() -> std::expected<NodeHandle, std::string> {
        auto const index{read_size()};
        if (!index) {
            return std::unexpected{index.error()};
        }
        return NodeHandle{*index};
    }
    auto remaining() const -> std::size_t { return bytes_.size() - offset_; }
  private:
    std::span<std::uint8_t const> bytes_;
    std::size_t offset_{};
};

auto valid_source_hash(std::string_view const hash) -> bool {
    return hash.size() == 64 && std::ranges::all_of(hash, [](unsigned char const value) {
               return std::isxdigit(value);
           });
}

void write_node(Writer& writer, Node const& node) {
    writer.write_u8(static_cast<std::uint8_t>(node.kind));
    writer.write_u8(static_cast<std::uint8_t>(node.type));
    writer.write_size(node.inputs.size());
    for (auto const input : node.inputs) {
        writer.write_handle(input);
    }
    writer.write_span(node.span);
    for (double const value : node.constant) {
        writer.write_double(value);
    }
    writer.write_size(node.component_count);
    writer.write_size(node.parameter_index);
    writer.write_u32(node.coordinate_index);
    writer.write_u32(node.instance_data_index);
    writer.write_u8(static_cast<std::uint8_t>(node.texture_sampler_type));
    writer.write_string(node.description);
    writer.write_string(node.code);
    writer.write_size(node.custom_inputs.size());
    for (auto const& input : node.custom_inputs) {
        writer.write_string(input.name);
        writer.write_u8(static_cast<std::uint8_t>(input.type));
        writer.write_handle(input.node);
    }
    writer.write_string(node.texture_path);
    writer.write_string(node.component_mask);
    writer.write_u8(static_cast<std::uint8_t>(node.source_space));
    writer.write_u8(static_cast<std::uint8_t>(node.destination_space));
    writer.write_u8(static_cast<std::uint8_t>(node.scene_texture));
    writer.write_string(node.shader_path);
    writer.write_string(node.shader_function);
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

auto read_node(Reader& reader) -> std::expected<Node, std::string> {
    auto const kind{reader.read_u8()};
    auto const type{read_value_type(reader)};
    auto const input_count{reader.read_size(maximum_collection_size)};
    if (!kind || *kind > static_cast<std::uint8_t>(NodeKind::camera_position) || !type ||
        !input_count) {
        return std::unexpected{"invalid material node header"};
    }

    Node node{.kind = static_cast<NodeKind>(*kind), .type = *type};
    node.inputs.reserve(*input_count);
    for (std::size_t index{}; index < *input_count; ++index) {
        auto const input{reader.read_handle()};
        if (!input) {
            return std::unexpected{input.error()};
        }
        node.inputs.push_back(*input);
    }
    auto span{reader.read_span()};
    if (!span) {
        return std::unexpected{span.error()};
    }
    node.span = std::move(*span);
    for (double& value : node.constant) {
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
    auto description{reader.read_string()};
    auto code{reader.read_string()};
    auto const custom_input_count{reader.read_size(maximum_collection_size)};
    if (!components || !parameter || !coordinate || !instance_data || !texture_sampler ||
        !description || !code || !custom_input_count) {
        return std::unexpected{"malformed material node payload"};
    }
    node.component_count = *components;
    node.parameter_index = *parameter;
    node.coordinate_index = *coordinate;
    node.instance_data_index = *instance_data;
    node.texture_sampler_type = *texture_sampler;
    node.description = std::move(*description);
    node.code = std::move(*code);
    node.custom_inputs.reserve(*custom_input_count);
    for (std::size_t index{}; index < *custom_input_count; ++index) {
        auto name{reader.read_string()};
        auto const input_type{read_value_type(reader)};
        auto const input_node{reader.read_handle()};
        if (!name || !input_type || !input_node) {
            return std::unexpected{"malformed custom input"};
        }
        node.custom_inputs.push_back({std::move(*name), *input_type, *input_node});
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
    node.texture_path = std::move(*texture_path);
    node.component_mask = std::move(*component_mask);
    node.source_space = static_cast<PositionSpace>(*source_space);
    node.destination_space = static_cast<PositionSpace>(*destination_space);
    node.scene_texture = static_cast<SceneTexture>(*scene_texture);
    node.shader_path = std::move(*shader_path);
    node.shader_function = std::move(*shader_function);
    return node;
}

void write_named_node(Writer& writer, NamedNode const& named) {
    writer.write_string(named.name);
    writer.write_handle(named.node);
    writer.write_span(named.span);
}

auto read_named_node(Reader& reader) -> std::expected<NamedNode, std::string> {
    auto name{reader.read_string()};
    auto const node{reader.read_handle()};
    auto span{reader.read_span()};
    if (!name || !node || !span) {
        return std::unexpected{"malformed named node"};
    }
    return NamedNode{std::move(*name), *node, std::move(*span)};
}

}

auto serialize(CompiledMaterial const& compiled)
    -> std::expected<std::vector<std::uint8_t>, std::string> {
    if (compiled.source_path.empty()) {
        return std::unexpected{"compiled material source path is empty"};
    }
    if (!valid_source_hash(compiled.source_hash)) {
        return std::unexpected{"compiled material source hash must be a 64-digit SHA-256 hash"};
    }
    auto const diagnostics{validate(compiled.material)};
    if (!diagnostics.empty()) {
        return std::unexpected{"cannot serialize invalid material IR: " +
                               diagnostics.front().message};
    }

    Writer writer;
    for (auto const byte : artifact_magic) {
        writer.write_u8(byte);
    }
    writer.write_u32(compiled_material_version);
    writer.write_string(compiled.source_path);
    writer.write_string(compiled.source_hash);
    writer.write_string(compiled.material.settings.name);
    writer.write_string(compiled.material.settings.package_path);
    writer.write_u8(static_cast<std::uint8_t>(compiled.material.settings.domain));
    writer.write_u8(static_cast<std::uint8_t>(compiled.material.settings.blend_mode));
    writer.write_u8(static_cast<std::uint8_t>(compiled.material.settings.shading_model));
    writer.write_u8(compiled.material.settings.two_sided ? 1 : 0);
    writer.write_u8(compiled.material.settings.disable_depth_test ? 1 : 0);
    writer.write_u8(compiled.material.settings.used_with_instanced_static_meshes ? 1 : 0);
    writer.write_u8(compiled.material.settings.adopt_existing ? 1 : 0);
    writer.write_double(compiled.material.settings.opacity_mask_clip_value);

    writer.write_size(compiled.material.parameters.size());
    for (auto const& parameter : compiled.material.parameters) {
        writer.write_string(parameter.name);
        writer.write_u8(static_cast<std::uint8_t>(parameter.type));
        writer.write_u8(static_cast<std::uint8_t>(parameter.texture_sampler_type));
        for (double const value : parameter.default_value) {
            writer.write_double(value);
        }
        writer.write_string(parameter.texture_path);
        writer.write_handle(parameter.node);
        writer.write_span(parameter.span);
    }
    writer.write_size(compiled.material.nodes.size());
    for (auto const& node : compiled.material.nodes) {
        write_node(writer, node);
    }
    writer.write_size(compiled.material.bindings.size());
    for (auto const& binding : compiled.material.bindings) {
        write_named_node(writer, binding);
    }
    writer.write_size(compiled.material.outputs.size());
    for (auto const& output : compiled.material.outputs) {
        write_named_node(writer, output);
    }
    writer.write_size(compiled.material.texture_dependencies.size());
    for (auto const& dependency : compiled.material.texture_dependencies) {
        writer.write_string(dependency);
    }
    return std::move(writer).finish();
}

auto deserialize(std::span<std::uint8_t const> const bytes)
    -> std::expected<CompiledMaterial, std::string> {
    Reader reader{bytes};
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

    CompiledMaterial compiled;
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
    auto const adopt_existing{reader.read_u8()};
    auto const opacity_mask_clip_value{reader.read_double()};
    if (!source_path || !source_hash || !name || !package_path || !domain || !blend_mode ||
        !shading_model || !two_sided || !disable_depth_test || !used_with_instances ||
        !adopt_existing || !opacity_mask_clip_value ||
        *domain > static_cast<std::uint8_t>(MaterialDomain::post_process) ||
        *blend_mode > static_cast<std::uint8_t>(BlendMode::alpha_composite) ||
        *shading_model > static_cast<std::uint8_t>(ShadingModel::unlit) || *two_sided > 1 ||
        *disable_depth_test > 1 || *used_with_instances > 1 || *adopt_existing > 1) {
        return std::unexpected{"malformed compiled material settings"};
    }
    compiled.source_path = std::move(*source_path);
    compiled.source_hash = std::move(*source_hash);
    compiled.material.settings = {.name = std::move(*name),
                                  .package_path = std::move(*package_path),
                                  .domain = static_cast<MaterialDomain>(*domain),
                                  .blend_mode = static_cast<BlendMode>(*blend_mode),
                                  .shading_model = static_cast<ShadingModel>(*shading_model),
                                  .two_sided = *two_sided != 0,
                                  .disable_depth_test = *disable_depth_test != 0,
                                  .used_with_instanced_static_meshes = *used_with_instances != 0,
                                  .adopt_existing = *adopt_existing != 0,
                                  .opacity_mask_clip_value = *opacity_mask_clip_value};
    if (compiled.source_path.empty() || !valid_source_hash(compiled.source_hash)) {
        return std::unexpected{"invalid compiled material source metadata"};
    }

    auto const parameter_count{reader.read_size(maximum_collection_size)};
    if (!parameter_count) {
        return std::unexpected{parameter_count.error()};
    }
    compiled.material.parameters.reserve(*parameter_count);
    for (std::size_t index{}; index < *parameter_count; ++index) {
        auto parameter_name{reader.read_string()};
        auto const type{read_value_type(reader)};
        auto const texture_sampler{read_texture_sampler_type(reader)};
        if (!parameter_name || !type || !texture_sampler) {
            return std::unexpected{"malformed material parameter"};
        }
        Parameter parameter{.name = std::move(*parameter_name),
                            .type = *type,
                            .texture_sampler_type = *texture_sampler};
        for (double& value : parameter.default_value) {
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
        parameter.texture_path = std::move(*texture_path);
        parameter.node = *node;
        parameter.span = std::move(*span);
        compiled.material.parameters.push_back(std::move(parameter));
    }

    auto const node_count{reader.read_size(maximum_collection_size)};
    if (!node_count) {
        return std::unexpected{node_count.error()};
    }
    compiled.material.nodes.reserve(*node_count);
    for (std::size_t index{}; index < *node_count; ++index) {
        auto node{read_node(reader)};
        if (!node) {
            return std::unexpected{node.error()};
        }
        compiled.material.nodes.push_back(std::move(*node));
    }

    auto read_named_nodes{[&](std::vector<NamedNode>& nodes) -> std::expected<void, std::string> {
        auto const count{reader.read_size(maximum_collection_size)};
        if (!count) {
            return std::unexpected{count.error()};
        }
        nodes.reserve(*count);
        for (std::size_t index{}; index < *count; ++index) {
            auto node{read_named_node(reader)};
            if (!node) {
                return std::unexpected{node.error()};
            }
            nodes.push_back(std::move(*node));
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
        compiled.material.texture_dependencies.push_back(std::move(*dependency));
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
