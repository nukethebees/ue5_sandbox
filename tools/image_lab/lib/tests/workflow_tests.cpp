#include <sandbox/image_lab/workflow.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

namespace sandbox::image_lab::tests {
namespace {

class TemporaryDirectory {
  public:
    TemporaryDirectory() {
        auto const timestamp{std::chrono::steady_clock::now().time_since_epoch().count()};
        path_ = std::filesystem::temp_directory_path() /
                ("sandbox-image-lab-" + std::to_string(timestamp));
    }

    ~TemporaryDirectory() {
        std::error_code error;
        std::filesystem::remove_all(path_, error);
    }

    [[nodiscard]] auto path() const -> std::filesystem::path const& { return path_; }
  private:
    std::filesystem::path path_;
};

[[nodiscard]] auto read_bytes(std::filesystem::path const& path) -> std::vector<std::uint8_t> {
    std::ifstream stream{path, std::ios::binary};
    return {std::istreambuf_iterator<char>{stream}, std::istreambuf_iterator<char>{}};
}

[[nodiscard]] auto read_u32(std::vector<std::uint8_t> const& bytes, std::size_t const offset)
    -> std::uint32_t {
    return static_cast<std::uint32_t>(bytes[offset]) << 24 |
           static_cast<std::uint32_t>(bytes[offset + 1]) << 16 |
           static_cast<std::uint32_t>(bytes[offset + 2]) << 8 |
           static_cast<std::uint32_t>(bytes[offset + 3]);
}

[[nodiscard]] auto read_u16_le(std::vector<std::uint8_t> const& bytes, std::size_t const offset)
    -> std::uint16_t {
    auto const value{static_cast<std::uint32_t>(bytes[offset]) |
                     static_cast<std::uint32_t>(bytes[offset + 1]) << 8};
    return static_cast<std::uint16_t>(value);
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

struct DecodedPng {
    std::uint32_t width{};
    std::uint32_t height{};
    std::vector<std::uint8_t> scanlines;
};

[[nodiscard]] auto decode_stored_zlib(std::vector<std::uint8_t> const& compressed)
    -> std::expected<std::vector<std::uint8_t>, std::string> {
    if (compressed.size() < 6u || (compressed[0] & 0x0Fu) != 8u ||
        ((static_cast<std::uint16_t>(compressed[0]) << 8 | compressed[1]) % 31u) != 0u ||
        (compressed[1] & 0x20u) != 0u) {
        return std::unexpected("Invalid zlib header.");
    }

    std::vector<std::uint8_t> result;
    std::size_t offset{2u};
    bool final_block{};
    do {
        if (offset + 5u > compressed.size()) {
            return std::unexpected("Truncated stored DEFLATE block.");
        }
        auto const header{compressed[offset++]};
        if (((header >> 1) & 0x03u) != 0u) {
            return std::unexpected("Expected a stored DEFLATE block.");
        }
        final_block = (header & 0x01u) != 0u;
        auto const length{read_u16_le(compressed, offset)};
        auto const inverse_length{read_u16_le(compressed, offset + 2u)};
        offset += 4u;
        if (static_cast<std::uint16_t>(~length) != inverse_length) {
            return std::unexpected("Stored DEFLATE length checksum failed.");
        }
        if (offset + length > compressed.size()) {
            return std::unexpected("Truncated stored DEFLATE payload.");
        }
        result.insert(result.end(),
                      compressed.begin() + static_cast<std::ptrdiff_t>(offset),
                      compressed.begin() + static_cast<std::ptrdiff_t>(offset + length));
        offset += length;
    } while (!final_block);

    if (offset + 4u != compressed.size() || adler32(result) != read_u32(compressed, offset)) {
        return std::unexpected("Invalid zlib Adler-32 checksum.");
    }
    return result;
}

[[nodiscard]] auto decode_png(std::vector<std::uint8_t> const& bytes)
    -> std::expected<DecodedPng, std::string> {
    constexpr std::array<std::uint8_t, 8> signature{137u, 80u, 78u, 71u, 13u, 10u, 26u, 10u};
    if (bytes.size() < signature.size() ||
        !std::equal(signature.begin(), signature.end(), bytes.begin())) {
        return std::unexpected("Invalid PNG signature.");
    }

    DecodedPng decoded;
    std::vector<std::uint8_t> idat;
    std::size_t offset{signature.size()};
    bool found_ihdr{};
    bool found_iend{};
    while (offset < bytes.size()) {
        if (offset + 12u > bytes.size()) {
            return std::unexpected("Truncated PNG chunk.");
        }
        auto const length{read_u32(bytes, offset)};
        auto const type_offset{offset + 4u};
        auto const data_offset{type_offset + 4u};
        auto const next_offset{data_offset + static_cast<std::size_t>(length)};
        if (next_offset + 4u > bytes.size()) {
            return std::unexpected("Invalid PNG chunk length.");
        }
        if (crc32(bytes.data() + type_offset, 4u + length) != read_u32(bytes, next_offset)) {
            return std::unexpected("PNG chunk CRC failed.");
        }
        auto const type{std::string{bytes.begin() + static_cast<std::ptrdiff_t>(type_offset),
                                    bytes.begin() + static_cast<std::ptrdiff_t>(data_offset)}};
        if (type == "IHDR") {
            if (found_ihdr || length != 13u) {
                return std::unexpected("Invalid IHDR chunk.");
            }
            decoded.width = read_u32(bytes, data_offset);
            decoded.height = read_u32(bytes, data_offset + 4u);
            if (bytes[data_offset + 8u] != 8u || bytes[data_offset + 9u] != 6u ||
                bytes[data_offset + 10u] != 0u || bytes[data_offset + 11u] != 0u ||
                bytes[data_offset + 12u] != 0u) {
                return std::unexpected("Unsupported PNG format.");
            }
            found_ihdr = true;
        } else if (type == "IDAT") {
            idat.insert(idat.end(),
                        bytes.begin() + static_cast<std::ptrdiff_t>(data_offset),
                        bytes.begin() + static_cast<std::ptrdiff_t>(next_offset));
        } else if (type == "IEND") {
            if (length != 0u || found_iend) {
                return std::unexpected("Invalid IEND chunk.");
            }
            found_iend = true;
            offset = next_offset + 4u;
            break;
        }
        offset = next_offset + 4u;
    }
    if (!found_ihdr || !found_iend || offset != bytes.size() || decoded.width == 0u ||
        decoded.height == 0u || idat.empty()) {
        return std::unexpected("Incomplete PNG stream.");
    }
    auto const scanlines{decode_stored_zlib(idat)};
    if (!scanlines) {
        return std::unexpected(scanlines.error());
    }
    decoded.scanlines = *scanlines;
    return decoded;
}

[[nodiscard]] auto expected_scanlines(image::GeneratedImage const& image)
    -> std::vector<std::uint8_t> {
    std::vector<std::uint8_t> scanlines;
    for (std::int32_t y{}; y < image.height; ++y) {
        scanlines.push_back(0u);
        for (std::int32_t x{}; x < image.width; ++x) {
            auto const& pixel{image.pixels[static_cast<std::size_t>(y * image.width + x)]};
            scanlines.insert(scanlines.end(), {pixel.red, pixel.green, pixel.blue, pixel.alpha});
        }
    }
    return scanlines;
}

} // namespace

TEST(ImageLabWorkflow, FindsAndDescribesEveryCanonicalPreset) {
    auto const names{default_preset_names()};
    ASSERT_EQ(names.size(), image::default_generation_requests().size());
    for (auto const& name : names) {
        auto const request{find_default_request(name)};
        ASSERT_TRUE(request.has_value());
        EXPECT_EQ(request->output_name, name);

        auto const description{describe_default_preset(name)};
        ASSERT_TRUE(description.has_value());
        EXPECT_TRUE(description->starts_with("version=6;"));
    }
    EXPECT_FALSE(find_default_request("not-a-preset").has_value());
    EXPECT_FALSE(describe_default_preset("not-a-preset").has_value());
}

TEST(ImageLabWorkflow, GeneratesAPngWithTheRequestedName) {
    TemporaryDirectory const output;
    auto request{image::make_default_request(image::GeneratorType::RingMask)};
    request.output_name = "ring";
    request.ring_mask = {
        .width = 3, .height = 2, .radius = 0.6f, .thickness = 0.1f, .falloff = 0.04f};

    auto const path{generate_to_png(request, output.path())};
    ASSERT_TRUE(path.has_value());
    EXPECT_EQ(path->filename(), "ring.png");

    auto const decoded{decode_png(read_bytes(*path))};
    ASSERT_TRUE(decoded.has_value()) << decoded.error();
    EXPECT_EQ(decoded->width, 3u);
    EXPECT_EQ(decoded->height, 2u);
    EXPECT_EQ(decoded->scanlines, expected_scanlines(image::generate_image(request)));
}

TEST(ImageLabWorkflow, GeneratesEveryDefaultPresetAndRejectsUnsafeNames) {
    TemporaryDirectory const output;
    auto const paths{generate_all_defaults_to_png(output.path())};
    ASSERT_TRUE(paths.has_value());
    EXPECT_EQ(paths->size(), image::default_generation_requests().size());
    for (auto const& path : *paths) {
        EXPECT_TRUE(std::filesystem::is_regular_file(path));
    }

    auto request{image::make_default_request(image::GeneratorType::RingMask)};
    request.output_name = "../unsafe";
    auto const invalid{generate_to_png(request, output.path())};
    ASSERT_FALSE(invalid.has_value());
    EXPECT_EQ(invalid.error(), "Output name must be a single file name.");
}

TEST(ImageLabWorkflow, SerializesEveryGeneratorAndPostProcessOutput) {
    constexpr std::array generators{image::GeneratorType::RadialGradient,
                                    image::GeneratorType::RingMask,
                                    image::GeneratorType::ShockwaveFlipbook,
                                    image::GeneratorType::Starfield,
                                    image::GeneratorType::Noise,
                                    image::GeneratorType::DomainWarpedNoise,
                                    image::GeneratorType::CurlNoiseFlow,
                                    image::GeneratorType::CellularNoise,
                                    image::GeneratorType::HexGrid};
    constexpr std::array outputs{image::ImagePostProcessParameters::Output::Scalar,
                                 image::ImagePostProcessParameters::Output::NormalMap,
                                 image::ImagePostProcessParameters::Output::SignedDistance};
    for (auto const generator : generators) {
        auto request{image::make_default_request(generator)};
        request.cellular_noise.mode = image::CellularMode::Borders;
        for (auto const output : outputs) {
            request.post_process.output = output;
            auto const serialized{serialize_request_json(request)};
            auto const round_trip{deserialize_request_json(serialized)};
            ASSERT_TRUE(round_trip.has_value()) << round_trip.error();
            EXPECT_EQ(serialize_request_json(*round_trip), serialized);
        }
    }
}

TEST(ImageLabWorkflow, RejectsMalformedAndUnknownRequestJson) {
    EXPECT_FALSE(deserialize_request_json("{").has_value());
    EXPECT_FALSE(deserialize_request_json(R"({"generator":"unknown"})").has_value());

    auto request{image::make_default_request(image::GeneratorType::RingMask)};
    auto serialized{serialize_request_json(request)};
    auto const generator_offset{serialized.find("ring_mask\"", serialized.find("\"generator\""))};
    ASSERT_NE(generator_offset, std::string::npos);
    serialized.replace(generator_offset, std::string{"ring_mask"}.size(), "unknown");
    auto const unknown_generator{deserialize_request_json(serialized)};
    ASSERT_FALSE(unknown_generator.has_value());
    EXPECT_EQ(unknown_generator.error(), "Unknown generator: unknown");

    serialized = serialize_request_json(request);
    auto const output_offset{serialized.find("scalar\"", serialized.find("\"post_process\""))};
    ASSERT_NE(output_offset, std::string::npos);
    serialized.replace(output_offset, std::string{"scalar"}.size(), "unknown");
    auto const unknown_output{deserialize_request_json(serialized)};
    ASSERT_FALSE(unknown_output.has_value());
    EXPECT_EQ(unknown_output.error(), "Unknown post-process output: unknown");
}

TEST(ImageLabWorkflow, LoadsCustomRequestAndGeneratesPng) {
    TemporaryDirectory const output;
    auto request{image::make_default_request(image::GeneratorType::HexGrid)};
    request.output_name = "custom_hex";
    request.hex_grid = {
        .width = 4, .height = 3, .cell_radius = 1.5F, .line_thickness = 0.25F, .falloff = 0.4F};
    request.post_process = {.invert = true,
                            .contrast = 1.2F,
                            .threshold_enabled = true,
                            .threshold = 0.4F,
                            .threshold_softness = 0.1F,
                            .output = image::ImagePostProcessParameters::Output::SignedDistance,
                            .normal_strength = 4.0F,
                            .normal_wrap = true,
                            .distance_threshold = 0.3F,
                            .distance_range = 5.0F,
                            .distance_wrap = true};
    auto const request_path{output.path() / "custom.json"};
    ASSERT_TRUE(write_request_json(request, request_path).has_value());
    auto const loaded{load_request_json(request_path)};
    ASSERT_TRUE(loaded.has_value()) << loaded.error();
    EXPECT_EQ(serialize_request_json(*loaded), serialize_request_json(request));

    auto const png{generate_to_png(*loaded, output.path() / "images")};
    ASSERT_TRUE(png.has_value()) << png.error();
    EXPECT_TRUE(std::filesystem::is_regular_file(*png));
}

} // namespace sandbox::image_lab::tests
