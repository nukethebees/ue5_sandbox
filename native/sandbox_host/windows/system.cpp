#include "system.h"

#include "../engine.h"
#include <ioj/settings/screen_resolution.h>

#include <combaseapi.h>
#include <knownfolders.h>
#include <shlobj_core.h>
#include <UnrealAgilitySDKLink.inl>

#include <expected>
#include <filesystem>
#include <format>
#include <memory>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

namespace ioj::sandbox_host {
auto system_error(std::string_view operation, DWORD code) -> std::string {
    return std::format("{} ({}): {}", operation, code, std::system_category().message(code));
}

auto executable_directory() -> PathResult {
    std::wstring buffer(32768, L'\0');
    auto const size{GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()))};
    if (size == 0 || size >= buffer.size()) {
        return PathResult{std::unexpect, system_error("GetModuleFileNameW", GetLastError())};
    }

    buffer.resize(size);
    return std::filesystem::path{buffer}.parent_path();
}

auto preferences_file() -> PathResult {
    PWSTR local{};
    auto const status{SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &local)};
    std::unique_ptr<wchar_t, decltype(&CoTaskMemFree)> directory{local, CoTaskMemFree};
    if (FAILED(status)) {
        return PathResult{std::unexpect,
                          system_error("SHGetKnownFolderPath(LocalAppData)", status)};
    }

    return std::filesystem::path{directory.get()} / L"IsleOfJebbers/Config/user-settings.scm";
}

auto desktop_resolution() -> std::expected<ScreenResolution, std::string> {
    using ResolutionResult = std::expected<ScreenResolution, std::string>;
    if (!SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2)) {
        return ResolutionResult{std::unexpect,
                                system_error("SetThreadDpiAwarenessContext", GetLastError())};
    }

    return ResolutionResult{
        std::in_place, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN)};
}
}
