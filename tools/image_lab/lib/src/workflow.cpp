#include <sandbox/image_lab/workflow.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <limits>
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
