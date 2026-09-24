#include "application.hpp"

#include <filesystem>

auto main() -> int {
    auto const output_directory{std::filesystem::path{IOJ_IMAGE_LAB_PROJECT_DIRECTORY} / "Saved" /
                                "ImageLab"};
    return sandbox::image_lab::gui::run_application(output_directory);
}
