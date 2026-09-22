#include <sandbox/image_lab/workflow.hpp>

#include <nlohmann/json.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <limits>
#include <sstream>
#include <system_error>
#include <utility>

namespace sandbox::image_lab::png {

auto append_u32(std::vector<std::uint8_t>& bytes, std::uint32_t const value) -> void {
    bytes.push_back(static_cast<std::uint8_t>(value >> 24));
    bytes.push_back(static_cast<std::uint8_t>(value >> 16));
    bytes.push_back(static_cast<std::uint8_t>(value >> 8));
    bytes.push_back(static_cast<std::uint8_t>(value));
}

[[nodiscard]] auto crc32(std::uint8_t const* const bytes, std::size_t const size) -> std::uint32_t {
    std::uint32_t value{0xFFFFFFFFu};
    for (std::size_t index{}; index < size; ++index) {
        value ^= bytes[index];
        for (std::int32_t bit{}; bit < 8; ++bit) {
            value = (value >> 1) ^ (0xEDB88320u & -(value & 1u));
        }
    }
    return ~value;
}

[[nodiscard]] auto adler32(std::vector<std::uint8_t> const& bytes) -> std::uint32_t {
    constexpr std::uint32_t modulus{65521u};
    std::uint32_t first{1u};
    std::uint32_t second{};
    for (auto const byte : bytes) {
        first = (first + byte) % modulus;
        second = (second + first) % modulus;
    }
    return (second << 16) | first;
}

auto append_chunk(std::vector<std::uint8_t>& png,
                  std::array<char, 4> const type,
                  std::vector<std::uint8_t> const& data) -> void {
    append_u32(png, static_cast<std::uint32_t>(data.size()));
    auto const type_start{png.size()};
    for (auto const character : type) {
        png.push_back(static_cast<std::uint8_t>(character));
    }
    png.insert(png.end(), data.begin(), data.end());
    append_u32(png, crc32(png.data() + type_start, type.size() + data.size()));
}

[[nodiscard]] auto deflate_stored(std::vector<std::uint8_t> const& bytes)
    -> std::vector<std::uint8_t> {
    std::vector<std::uint8_t> compressed;
    compressed.reserve(bytes.size() + bytes.size() / 65535u * 5u + 6u);
    compressed.push_back(0x78u);
    compressed.push_back(0x01u);

    std::size_t offset{};
    do {
        auto const remaining{bytes.size() - offset};
        auto const length{static_cast<std::uint16_t>(std::min<std::size_t>(remaining, 65535u))};
        auto const final_block{offset + length == bytes.size()};
        compressed.push_back(final_block ? 0x01u : 0x00u);
        compressed.push_back(static_cast<std::uint8_t>(length));
        compressed.push_back(static_cast<std::uint8_t>(length >> 8));
        auto const inverse_length{static_cast<std::uint16_t>(~length)};
        compressed.push_back(static_cast<std::uint8_t>(inverse_length));
        compressed.push_back(static_cast<std::uint8_t>(inverse_length >> 8));
        compressed.insert(compressed.end(),
                          bytes.begin() + static_cast<std::ptrdiff_t>(offset),
                          bytes.begin() + static_cast<std::ptrdiff_t>(offset + length));
        offset += length;
    } while (offset < bytes.size());

    append_u32(compressed, adler32(bytes));
    return compressed;
}

[[nodiscard]] auto encode(image::GeneratedImage const& image)
    -> std::expected<std::vector<std::uint8_t>, std::string> {
    if (!image.is_valid()) {
        return std::unexpected(image.error.empty() ? "Image buffer is invalid." : image.error);
    }

    auto const width{static_cast<std::size_t>(image.width)};
    auto const height{static_cast<std::size_t>(image.height)};
    if (width > (std::numeric_limits<std::size_t>::max() - height) / 4u) {
        return std::unexpected("Image dimensions are too large to encode as PNG.");
    }

    std::vector<std::uint8_t> scanlines;
    scanlines.reserve(height * (width * 4u + 1u));
    for (std::size_t y{}; y < height; ++y) {
        scanlines.push_back(0u);
        for (std::size_t x{}; x < width; ++x) {
            auto const& pixel{image.pixels[y * width + x]};
            scanlines.insert(scanlines.end(), {pixel.red, pixel.green, pixel.blue, pixel.alpha});
        }
    }

    std::vector<std::uint8_t> header;
    header.reserve(13);
    append_u32(header, static_cast<std::uint32_t>(image.width));
    append_u32(header, static_cast<std::uint32_t>(image.height));
    header.insert(header.end(), {8u, 6u, 0u, 0u, 0u});

    std::vector<std::uint8_t> encoded;
    encoded.insert(encoded.end(), {137u, 80u, 78u, 71u, 13u, 10u, 26u, 10u});
    append_chunk(encoded, {'I', 'H', 'D', 'R'}, header);
    append_chunk(encoded, {'I', 'D', 'A', 'T'}, deflate_stored(scanlines));
    append_chunk(encoded, {'I', 'E', 'N', 'D'}, {});
    return encoded;
}

} // namespace sandbox::image_lab::png

namespace sandbox::image_lab {
namespace {

using Json = nlohmann::json;

[[nodiscard]] auto generator_to_string(image::GeneratorType const value) -> char const* {
    switch (value) {
        case image::GeneratorType::RadialGradient:
            return "radial_gradient";
        case image::GeneratorType::RingMask:
            return "ring_mask";
        case image::GeneratorType::ShockwaveFlipbook:
            return "shockwave_flipbook";
        case image::GeneratorType::Starfield:
            return "starfield";
        case image::GeneratorType::Noise:
            return "noise";
        case image::GeneratorType::DomainWarpedNoise:
            return "domain_warped_noise";
        case image::GeneratorType::CurlNoiseFlow:
            return "curl_noise_flow";
        case image::GeneratorType::CellularNoise:
            return "cellular_noise";
        case image::GeneratorType::HexGrid:
            return "hex_grid";
    }
    return "unknown";
}

[[nodiscard]] auto generator_from_string(std::string const& value)
    -> std::expected<image::GeneratorType, std::string> {
    constexpr std::array values{
        std::pair{"radial_gradient", image::GeneratorType::RadialGradient},
        std::pair{"ring_mask", image::GeneratorType::RingMask},
        std::pair{"shockwave_flipbook", image::GeneratorType::ShockwaveFlipbook},
        std::pair{"starfield", image::GeneratorType::Starfield},
        std::pair{"noise", image::GeneratorType::Noise},
        std::pair{"domain_warped_noise", image::GeneratorType::DomainWarpedNoise},
        std::pair{"curl_noise_flow", image::GeneratorType::CurlNoiseFlow},
        std::pair{"cellular_noise", image::GeneratorType::CellularNoise},
        std::pair{"hex_grid", image::GeneratorType::HexGrid},
    };
    for (auto const& [name, generator] : values) {
        if (value == name) {
            return generator;
        }
    }
    return std::unexpected("Unknown generator: " + value);
}

[[nodiscard]] auto cellular_mode_to_string(image::CellularMode const value) -> char const* {
    switch (value) {
        case image::CellularMode::Distance:
            return "distance";
        case image::CellularMode::Borders:
            return "borders";
    }
    return "unknown";
}

[[nodiscard]] auto cellular_mode_from_string(std::string const& value)
    -> std::expected<image::CellularMode, std::string> {
    if (value == "distance") {
        return image::CellularMode::Distance;
    }
    if (value == "borders") {
        return image::CellularMode::Borders;
    }
    return std::unexpected("Unknown cellular mode: " + value);
}

[[nodiscard]] auto output_to_string(image::ImagePostProcessParameters::Output const value)
    -> char const* {
    switch (value) {
        case image::ImagePostProcessParameters::Output::Scalar:
            return "scalar";
        case image::ImagePostProcessParameters::Output::NormalMap:
            return "normal_map";
        case image::ImagePostProcessParameters::Output::SignedDistance:
            return "signed_distance";
    }
    return "unknown";
}

[[nodiscard]] auto output_from_string(std::string const& value)
    -> std::expected<image::ImagePostProcessParameters::Output, std::string> {
    if (value == "scalar") {
        return image::ImagePostProcessParameters::Output::Scalar;
    }
    if (value == "normal_map") {
        return image::ImagePostProcessParameters::Output::NormalMap;
    }
    if (value == "signed_distance") {
        return image::ImagePostProcessParameters::Output::SignedDistance;
    }
    return std::unexpected("Unknown post-process output: " + value);
}

[[nodiscard]] auto to_json(image::GenerationRequest const& request) -> Json {
    return {
        {"generator", generator_to_string(request.generator)},
        {"output_name", request.output_name},
        {"radial_gradient",
         {{"width", request.radial_gradient.width},
          {"height", request.radial_gradient.height},
          {"inner_radius", request.radial_gradient.inner_radius},
          {"outer_radius", request.radial_gradient.outer_radius}}},
        {"ring_mask",
         {{"width", request.ring_mask.width},
          {"height", request.ring_mask.height},
          {"radius", request.ring_mask.radius},
          {"thickness", request.ring_mask.thickness},
          {"falloff", request.ring_mask.falloff}}},
        {"shockwave_flipbook",
         {{"width", request.shockwave_flipbook.width},
          {"height", request.shockwave_flipbook.height},
          {"columns", request.shockwave_flipbook.columns},
          {"rows", request.shockwave_flipbook.rows},
          {"frame_count", request.shockwave_flipbook.frame_count},
          {"start_radius", request.shockwave_flipbook.start_radius},
          {"end_radius", request.shockwave_flipbook.end_radius},
          {"thickness", request.shockwave_flipbook.thickness},
          {"falloff", request.shockwave_flipbook.falloff},
          {"start_intensity", request.shockwave_flipbook.start_intensity},
          {"end_intensity", request.shockwave_flipbook.end_intensity}}},
        {"starfield",
         {{"width", request.starfield.width},
          {"height", request.starfield.height},
          {"seed", request.starfield.seed},
          {"star_count", request.starfield.star_count},
          {"minimum_brightness", request.starfield.minimum_brightness},
          {"minimum_radius", request.starfield.minimum_radius},
          {"maximum_radius", request.starfield.maximum_radius},
          {"transparent_background", request.starfield.transparent_background}}},
        {"noise",
         {{"width", request.noise.width},
          {"height", request.noise.height},
          {"seed", request.noise.seed},
          {"base_scale", request.noise.base_scale},
          {"octave_count", request.noise.octave_count},
          {"persistence", request.noise.persistence},
          {"tileable", request.noise.tileable}}},
        {"domain_warped_noise",
         {{"width", request.domain_warped_noise.width},
          {"height", request.domain_warped_noise.height},
          {"base_seed", request.domain_warped_noise.base_seed},
          {"warp_seed", request.domain_warped_noise.warp_seed},
          {"base_scale", request.domain_warped_noise.base_scale},
          {"warp_scale", request.domain_warped_noise.warp_scale},
          {"warp_strength", request.domain_warped_noise.warp_strength},
          {"base_octave_count", request.domain_warped_noise.base_octave_count},
          {"warp_octave_count", request.domain_warped_noise.warp_octave_count},
          {"persistence", request.domain_warped_noise.persistence},
          {"tileable", request.domain_warped_noise.tileable}}},
        {"curl_noise_flow",
         {{"width", request.curl_noise_flow.width},
          {"height", request.curl_noise_flow.height},
          {"seed", request.curl_noise_flow.seed},
          {"base_scale", request.curl_noise_flow.base_scale},
          {"octave_count", request.curl_noise_flow.octave_count},
          {"persistence", request.curl_noise_flow.persistence},
          {"derivative_step", request.curl_noise_flow.derivative_step},
          {"strength", request.curl_noise_flow.strength},
          {"tileable", request.curl_noise_flow.tileable}}},
        {"cellular_noise",
         {{"width", request.cellular_noise.width},
          {"height", request.cellular_noise.height},
          {"seed", request.cellular_noise.seed},
          {"cell_size", request.cellular_noise.cell_size},
          {"jitter", request.cellular_noise.jitter},
          {"mode", cellular_mode_to_string(request.cellular_noise.mode)},
          {"edge_width", request.cellular_noise.edge_width},
          {"falloff", request.cellular_noise.falloff},
          {"tileable", request.cellular_noise.tileable}}},
        {"hex_grid",
         {{"width", request.hex_grid.width},
          {"height", request.hex_grid.height},
          {"cell_radius", request.hex_grid.cell_radius},
          {"line_thickness", request.hex_grid.line_thickness},
          {"falloff", request.hex_grid.falloff}}},
        {"post_process",
         {{"invert", request.post_process.invert},
          {"contrast", request.post_process.contrast},
          {"threshold_enabled", request.post_process.threshold_enabled},
          {"threshold", request.post_process.threshold},
          {"threshold_softness", request.post_process.threshold_softness},
          {"output", output_to_string(request.post_process.output)},
          {"normal_strength", request.post_process.normal_strength},
          {"normal_wrap", request.post_process.normal_wrap},
          {"distance_threshold", request.post_process.distance_threshold},
          {"distance_range", request.post_process.distance_range},
          {"distance_wrap", request.post_process.distance_wrap}}},
    };
}

template <typename T>
auto read(Json const& object, char const* const key) -> T {
    return object.at(key).template get<T>();
}

[[nodiscard]] auto from_json(Json const& document)
    -> std::expected<image::GenerationRequest, std::string> {
    if (!document.is_object()) {
        return std::unexpected("Request JSON must contain an object.");
    }

    try {
        image::GenerationRequest request{};
        auto const generator{generator_from_string(read<std::string>(document, "generator"))};
        if (!generator) {
            return std::unexpected(generator.error());
        }
        request.generator = *generator;
        request.output_name = read<std::string>(document, "output_name");

        auto const& radial{document.at("radial_gradient")};
        request.radial_gradient = {read<std::int32_t>(radial, "width"),
                                   read<std::int32_t>(radial, "height"),
                                   read<float>(radial, "inner_radius"),
                                   read<float>(radial, "outer_radius")};
        auto const& ring{document.at("ring_mask")};
        request.ring_mask = {read<std::int32_t>(ring, "width"),
                             read<std::int32_t>(ring, "height"),
                             read<float>(ring, "radius"),
                             read<float>(ring, "thickness"),
                             read<float>(ring, "falloff")};
        auto const& shockwave{document.at("shockwave_flipbook")};
        request.shockwave_flipbook = {read<std::int32_t>(shockwave, "width"),
                                      read<std::int32_t>(shockwave, "height"),
                                      read<std::int32_t>(shockwave, "columns"),
                                      read<std::int32_t>(shockwave, "rows"),
                                      read<std::int32_t>(shockwave, "frame_count"),
                                      read<float>(shockwave, "start_radius"),
                                      read<float>(shockwave, "end_radius"),
                                      read<float>(shockwave, "thickness"),
                                      read<float>(shockwave, "falloff"),
                                      read<float>(shockwave, "start_intensity"),
                                      read<float>(shockwave, "end_intensity")};
        auto const& starfield{document.at("starfield")};
        request.starfield = {read<std::int32_t>(starfield, "width"),
                             read<std::int32_t>(starfield, "height"),
                             read<std::uint32_t>(starfield, "seed"),
                             read<std::int32_t>(starfield, "star_count"),
                             read<float>(starfield, "minimum_brightness"),
                             read<float>(starfield, "minimum_radius"),
                             read<float>(starfield, "maximum_radius"),
                             read<bool>(starfield, "transparent_background")};
        auto const& noise{document.at("noise")};
        request.noise = {read<std::int32_t>(noise, "width"),
                         read<std::int32_t>(noise, "height"),
                         read<std::uint32_t>(noise, "seed"),
                         read<float>(noise, "base_scale"),
                         read<std::int32_t>(noise, "octave_count"),
                         read<float>(noise, "persistence"),
                         read<bool>(noise, "tileable")};
        auto const& domain_warped{document.at("domain_warped_noise")};
        request.domain_warped_noise = {read<std::int32_t>(domain_warped, "width"),
                                       read<std::int32_t>(domain_warped, "height"),
                                       read<std::uint32_t>(domain_warped, "base_seed"),
                                       read<std::uint32_t>(domain_warped, "warp_seed"),
                                       read<float>(domain_warped, "base_scale"),
                                       read<float>(domain_warped, "warp_scale"),
                                       read<float>(domain_warped, "warp_strength"),
                                       read<std::int32_t>(domain_warped, "base_octave_count"),
                                       read<std::int32_t>(domain_warped, "warp_octave_count"),
                                       read<float>(domain_warped, "persistence"),
                                       read<bool>(domain_warped, "tileable")};
        auto const& curl{document.at("curl_noise_flow")};
        request.curl_noise_flow = {read<std::int32_t>(curl, "width"),
                                   read<std::int32_t>(curl, "height"),
                                   read<std::uint32_t>(curl, "seed"),
                                   read<float>(curl, "base_scale"),
                                   read<std::int32_t>(curl, "octave_count"),
                                   read<float>(curl, "persistence"),
                                   read<float>(curl, "derivative_step"),
                                   read<float>(curl, "strength"),
                                   read<bool>(curl, "tileable")};
        auto const& cellular{document.at("cellular_noise")};
        auto const mode{cellular_mode_from_string(read<std::string>(cellular, "mode"))};
        if (!mode) {
            return std::unexpected(mode.error());
        }
        request.cellular_noise = {read<std::int32_t>(cellular, "width"),
                                  read<std::int32_t>(cellular, "height"),
                                  read<std::uint32_t>(cellular, "seed"),
                                  read<float>(cellular, "cell_size"),
                                  read<float>(cellular, "jitter"),
                                  *mode,
                                  read<float>(cellular, "edge_width"),
                                  read<float>(cellular, "falloff"),
                                  read<bool>(cellular, "tileable")};
        auto const& hex{document.at("hex_grid")};
        request.hex_grid = {read<std::int32_t>(hex, "width"),
                            read<std::int32_t>(hex, "height"),
                            read<float>(hex, "cell_radius"),
                            read<float>(hex, "line_thickness"),
                            read<float>(hex, "falloff")};
        auto const& post_process{document.at("post_process")};
        auto const output{output_from_string(read<std::string>(post_process, "output"))};
        if (!output) {
            return std::unexpected(output.error());
        }
        request.post_process = {read<bool>(post_process, "invert"),
                                read<float>(post_process, "contrast"),
                                read<bool>(post_process, "threshold_enabled"),
                                read<float>(post_process, "threshold"),
                                read<float>(post_process, "threshold_softness"),
                                *output,
                                read<float>(post_process, "normal_strength"),
                                read<bool>(post_process, "normal_wrap"),
                                read<float>(post_process, "distance_threshold"),
                                read<float>(post_process, "distance_range"),
                                read<bool>(post_process, "distance_wrap")};
        return request;
    } catch (Json::exception const& error) {
        return std::unexpected("Invalid request JSON structure: " + std::string{error.what()});
    }
}

[[nodiscard]] auto output_path_for(image::GenerationRequest const& request,
                                   std::filesystem::path const& output_directory)
    -> std::expected<std::filesystem::path, std::string> {
    if (request.output_name.empty()) {
        return std::unexpected("Output name must not be empty.");
    }

    auto const filename{std::filesystem::path{request.output_name}};
    if (filename.has_parent_path() || filename.filename() != filename ||
        request.output_name == "." || request.output_name == "..") {
        return std::unexpected("Output name must be a single file name.");
    }
    return output_directory / (request.output_name + ".png");
}

[[nodiscard]] auto ensure_output_directory(std::filesystem::path const& output_directory)
    -> std::expected<void, std::string> {
    if (output_directory.empty()) {
        return std::unexpected("Output directory must not be empty.");
    }

    std::error_code error;
    std::filesystem::create_directories(output_directory, error);
    if (error || !std::filesystem::is_directory(output_directory, error)) {
        return std::unexpected("Could not create output directory: " + output_directory.string());
    }
    return {};
}

} // namespace

auto default_preset_names() -> std::vector<std::string> {
    auto const requests{image::default_generation_requests()};
    std::vector<std::string> names;
    names.reserve(requests.size());
    for (auto const& request : requests) {
        names.push_back(request.output_name);
    }
    return names;
}

auto find_default_request(std::string_view const output_name)
    -> std::optional<image::GenerationRequest> {
    for (auto const& request : image::default_generation_requests()) {
        if (request.output_name == output_name) {
            return request;
        }
    }
    return std::nullopt;
}

auto describe_default_preset(std::string_view const output_name)
    -> std::expected<std::string, std::string> {
    auto const request{find_default_request(output_name)};
    if (!request) {
        return std::unexpected("Unknown Image Lab preset: " + std::string{output_name});
    }
    return image::describe_request(*request);
}

auto serialize_request_json(image::GenerationRequest const& request) -> std::string {
    return to_json(request).dump(2) + '\n';
}

auto deserialize_request_json(std::string_view const json_text)
    -> std::expected<image::GenerationRequest, std::string> {
    try {
        return from_json(Json::parse(json_text.begin(), json_text.end()));
    } catch (Json::exception const& error) {
        return std::unexpected("Malformed request JSON: " + std::string{error.what()});
    }
}

auto write_request_json(image::GenerationRequest const& request, std::filesystem::path const& path)
    -> std::expected<void, std::string> {
    if (path.empty()) {
        return std::unexpected("Request JSON path must not be empty.");
    }
    std::error_code error;
    if (auto const parent{path.parent_path()}; !parent.empty()) {
        std::filesystem::create_directories(parent, error);
        if (error) {
            return std::unexpected("Could not create request directory: " + parent.string());
        }
    }
    std::ofstream stream{path, std::ios::binary | std::ios::trunc};
    if (!stream) {
        return std::unexpected("Could not open request JSON: " + path.string());
    }
    stream << serialize_request_json(request);
    if (!stream) {
        return std::unexpected("Could not write request JSON: " + path.string());
    }
    return {};
}

auto load_request_json(std::filesystem::path const& path)
    -> std::expected<image::GenerationRequest, std::string> {
    std::ifstream stream{path, std::ios::binary};
    if (!stream) {
        return std::unexpected("Could not open request JSON: " + path.string());
    }
    std::ostringstream contents;
    contents << stream.rdbuf();
    if (!stream.good() && !stream.eof()) {
        return std::unexpected("Could not read request JSON: " + path.string());
    }
    return deserialize_request_json(contents.str());
}

auto generate_to_png(image::GenerationRequest const& request,
                     std::filesystem::path const& output_directory)
    -> std::expected<std::filesystem::path, std::string> {
    auto const output_path{output_path_for(request, output_directory)};
    if (!output_path) {
        return std::unexpected(output_path.error());
    }
    auto const ensured{ensure_output_directory(output_directory)};
    if (!ensured) {
        return std::unexpected(ensured.error());
    }

    auto const encoded{png::encode(image::generate_image(request))};
    if (!encoded) {
        return std::unexpected("Could not generate " + request.output_name + ": " +
                               encoded.error());
    }

    std::ofstream stream{*output_path, std::ios::binary | std::ios::trunc};
    if (!stream) {
        return std::unexpected("Could not open output file: " + output_path->string());
    }
    stream.write(reinterpret_cast<char const*>(encoded->data()),
                 static_cast<std::streamsize>(encoded->size()));
    if (!stream) {
        return std::unexpected("Could not write PNG: " + output_path->string());
    }
    return *output_path;
}

auto generate_all_defaults_to_png(std::filesystem::path const& output_directory)
    -> std::expected<std::vector<std::filesystem::path>, std::string> {
    std::vector<std::filesystem::path> paths;
    auto const requests{image::default_generation_requests()};
    paths.reserve(requests.size());
    for (auto const& request : requests) {
        auto const path{generate_to_png(request, output_directory)};
        if (!path) {
            return std::unexpected(path.error());
        }
        paths.push_back(*path);
    }
    return paths;
}

} // namespace sandbox::image_lab
