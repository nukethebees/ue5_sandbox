#pragma once

#include <sandbox/image/image_generation.h>

#include <expected>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace sandbox::image_lab {

template <typename T>
using WorkflowResult = std::expected<T, std::string>;

[[nodiscard]] auto default_preset_names() -> std::vector<std::string>;
[[nodiscard]] auto find_default_request(std::string_view output_name)
    -> std::optional<image::GenerationRequest>;
[[nodiscard]] auto describe_default_preset(std::string_view output_name)
    -> WorkflowResult<std::string>;

[[nodiscard]] auto serialize_request_json(image::GenerationRequest const& request) -> std::string;
[[nodiscard]] auto deserialize_request_json(std::string_view json_text)
    -> WorkflowResult<image::GenerationRequest>;
[[nodiscard]] auto write_request_json(image::GenerationRequest const& request,
                                      std::filesystem::path const& path) -> WorkflowResult<void>;
[[nodiscard]] auto load_request_json(std::filesystem::path const& path)
    -> WorkflowResult<image::GenerationRequest>;

[[nodiscard]] auto generate_to_png(image::GenerationRequest const& request,
                                   std::filesystem::path const& output_directory)
    -> WorkflowResult<std::filesystem::path>;
[[nodiscard]] auto generate_all_defaults_to_png(std::filesystem::path const& output_directory)
    -> WorkflowResult<std::vector<std::filesystem::path>>;

} // namespace sandbox::image_lab
