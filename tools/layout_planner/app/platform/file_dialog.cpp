#include "file_dialog.hpp"

#include "sdl_headers.hpp"

#include <nfd.h>

#include <string>

namespace ioj::layout_planner {
namespace {

auto utf8(std::filesystem::path const& path) -> std::string {
    auto const value{path.u8string()};
    return {reinterpret_cast<char const*>(value.data()), value.size()};
}

auto from_utf8(char const* value) -> std::filesystem::path {
    return std::filesystem::path{std::u8string{reinterpret_cast<char8_t const*>(value)}};
}

auto parent_window(SDL_Window* const window) -> nfdwindowhandle_t {
    nfdwindowhandle_t parent{};
#ifdef _WIN32
    if (window != nullptr) {
        parent.type = NFD_WINDOW_HANDLE_TYPE_WINDOWS;
        parent.handle = SDL_GetPointerProperty(
            SDL_GetWindowProperties(window), SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr);
    }
#else
    static_cast<void>(window);
#endif
    return parent;
}

auto result_path(nfdresult_t const result, nfdu8char_t* const path) -> FileDialogResult {
    if (result == NFD_CANCEL) {
        return std::nullopt;
    }
    if (result != NFD_OKAY) {
        return std::unexpected{std::string{NFD_GetError()}};
    }
    auto selected{from_utf8(path)};
    NFD_FreePathU8(path);
    return selected;
}

} // namespace

auto FileDialog::initialize(SDL_Window* const window) -> bool {
    if (NFD_Init() != NFD_OKAY) {
        return false;
    }
    window_ = window;
    initialized_ = true;
    return true;
}

void FileDialog::shutdown() {
    if (initialized_) {
        NFD_Quit();
        initialized_ = false;
    }
}

auto FileDialog::open_file(std::filesystem::path const& initial,
                           std::string_view const extension) const -> FileDialogResult {
    auto const folder{utf8(initial.has_filename() ? initial.parent_path() : initial)};
    auto const filter_name{std::string{extension} + " files"};
    auto const filter_extension{std::string{extension}};
    nfdu8filteritem_t const filters[]{{filter_name.c_str(), filter_extension.c_str()}};
    nfdopendialogu8args_t args{};
    args.filterList = extension.empty() ? nullptr : filters;
    args.filterCount = extension.empty() ? 0 : 1;
    args.defaultPath = folder.empty() ? nullptr : folder.c_str();
    args.parentWindow = parent_window(window_);
    nfdu8char_t* path{};
    auto const result{NFD_OpenDialogU8_With(&path, &args)};
    return result_path(result, path);
}

auto FileDialog::save_file(std::filesystem::path const& suggested,
                           std::string_view const extension) const -> FileDialogResult {
    auto const folder{utf8(suggested.parent_path())};
    auto const filename{utf8(suggested.filename())};
    auto const filter_name{std::string{extension} + " files"};
    auto const filter_extension{std::string{extension}};
    nfdu8filteritem_t const filters[]{{filter_name.c_str(), filter_extension.c_str()}};
    nfdsavedialogu8args_t args{};
    args.filterList = extension.empty() ? nullptr : filters;
    args.filterCount = extension.empty() ? 0 : 1;
    args.defaultPath = folder.empty() ? nullptr : folder.c_str();
    args.defaultName = filename.empty() ? nullptr : filename.c_str();
    args.parentWindow = parent_window(window_);
    nfdu8char_t* path{};
    auto const result{NFD_SaveDialogU8_With(&path, &args)};
    return result_path(result, path);
}

auto FileDialog::pick_folder(std::filesystem::path const& initial) const -> FileDialogResult {
    auto const folder{utf8(initial)};
    nfdpickfolderu8args_t args{};
    args.defaultPath = folder.empty() ? nullptr : folder.c_str();
    args.parentWindow = parent_window(window_);
    nfdu8char_t* path{};
    auto const result{NFD_PickFolderU8_With(&path, &args)};
    return result_path(result, path);
}

auto FileDialog::error() const -> std::string {
    return NFD_GetError();
}

} // namespace ioj::layout_planner
