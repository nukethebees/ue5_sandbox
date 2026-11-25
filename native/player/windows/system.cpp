#include "system.h"

#include "../engine.h"
#include <ioj/settings/screen_resolution.h>
#include <ioj/system_error_message.h>

#include <combaseapi.h>
#include <knownfolders.h>
#include <shlobj_core.h>
#include <UnrealAgilitySDKLink.inl>

#include <expected>
#include <filesystem>
#include <memory>
#include <string>

namespace ioj::player {
auto local_app_data() -> PathResult {
    PWSTR local{};
    auto const status{SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &local)};
    std::unique_ptr<wchar_t, decltype(&CoTaskMemFree)> directory{local, CoTaskMemFree};
    if (FAILED(status)) {
        return PathResult{std::unexpect,
                          system_error_message("SHGetKnownFolderPath(LocalAppData)", status)};
    }

    return std::filesystem::path{directory.get()};
}

}
