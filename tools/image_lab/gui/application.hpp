#pragma once

#include <filesystem>

namespace sandbox::image_lab::gui {

auto run_application(std::filesystem::path output_directory) -> int;

} // namespace sandbox::image_lab::gui
