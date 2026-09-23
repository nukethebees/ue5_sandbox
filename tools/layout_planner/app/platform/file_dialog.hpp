#pragma once

#include <expected>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

struct SDL_Window;

namespace ioj::layout_planner {

using FileDialogResult = std::expected<std::optional<std::filesystem::path>, std::string>;

class FileDialog {
  public:
    auto initialize(SDL_Window* window) -> bool;
    void shutdown();

    [[nodiscard]] auto open_file(std::filesystem::path const& initial,
                                 std::string_view extension) const -> FileDialogResult;
    [[nodiscard]] auto save_file(std::filesystem::path const& suggested,
                                 std::string_view extension) const -> FileDialogResult;
    [[nodiscard]] auto pick_folder(std::filesystem::path const& initial) const -> FileDialogResult;
    [[nodiscard]] auto error() const -> std::string;
  private:
    SDL_Window* window_{};
    bool initialized_{};
};

} // namespace ioj::layout_planner
