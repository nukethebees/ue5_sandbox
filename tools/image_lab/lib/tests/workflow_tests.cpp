#include <sandbox/image_lab/workflow.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
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

    auto const bytes{read_bytes(*path)};
    constexpr std::array<std::uint8_t, 8> signature{137u, 80u, 78u, 71u, 13u, 10u, 26u, 10u};
    ASSERT_GE(bytes.size(), 33u);
    EXPECT_TRUE(std::equal(signature.begin(), signature.end(), bytes.begin()));
    EXPECT_EQ(read_u32(bytes, 8), 13u);
    auto const chunk_type{std::string{bytes.begin() + 12, bytes.begin() + 16}};
    EXPECT_EQ(chunk_type, "IHDR");
    EXPECT_EQ(read_u32(bytes, 16), 3u);
    EXPECT_EQ(read_u32(bytes, 20), 2u);
    EXPECT_EQ(bytes[24], 8u);
    EXPECT_EQ(bytes[25], 6u);
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

} // namespace sandbox::image_lab::tests
