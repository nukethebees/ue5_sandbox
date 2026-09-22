#pragma once

#include <sandbox/image/image_generation.h>

#include <expected>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace sandbox::image_lab {

[[nodiscard]] auto default_preset_names() -> std::vector<std::string>;
[[nodiscard]] auto find_default_request(std::string_view output_name)
    -> std::optional<image::GenerationRequest>;
[[nodiscard]] auto describe_default_preset(std::string_view output_name)
    -> std::expected<std::string, std::string>;

[[nodiscard]] auto generate_to_png(image::GenerationRequest const& request,
                                   std::filesystem::path const& output_directory)
    -> std::expected<std::filesystem::path, std::string>;
[[nodiscard]] auto generate_all_defaults_to_png(std::filesystem::path const& output_directory)
    -> std::expected<std::vector<std::filesystem::path>, std::string>;

} // namespace sandbox::image_lab
