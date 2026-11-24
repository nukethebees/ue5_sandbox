#include "library.h"

#include "../engine.h"
#include "SandboxHostFingerprint.h"
#include "system.h"
#include <ioj/files.h>
#include <ioj/host/game_host_api.h>

#include <expected>
#include <filesystem>
#include <format>
#include <string>
#include <type_traits>
#include <utility>

namespace ioj::sandbox_host {
auto Library::load(std::filesystem::path const& path) -> Result {
    // Unreal stays loaded until process exit, including after UELibrary_Shutdown.
    auto const module{LoadLibraryW(path.c_str())};
    if (!module) {
        auto const code{GetLastError()};
        return Result{std::unexpect, system_error("LoadLibraryW " + path_to_utf8(path), code)};
    }

    std::string error;
    auto bind = [&](auto& function, char const* name) {
        if (!error.empty()) {
            return;
        }

        auto const address{GetProcAddress(module, name)};
        if (!address) {
            auto const code{GetLastError()};
            error = system_error(std::format("GetProcAddress {} in {}", name, path_to_utf8(path)),
                                 code);
            return;
        }

        function = reinterpret_cast<std::remove_reference_t<decltype(function)>>(address);
    };

    decltype(&SandboxHost_Compatible) compatible{};
    bind(compatible, "SandboxHost_Compatible");
    if (!compatible) {
        return Result{std::unexpect, std::move(error)};
    }

    if (!compatible(host_interface_version, host_layout_signature(), SANDBOX_HOST_FINGERPRINT)) {
        return Result{std::unexpect, "Incompatible host interface in " + path_to_utf8(path)};
    }

    bind(init, "UELibrary_InitW");
    bind(tick, "UELibrary_Tick");
    bind(shutdown, "UELibrary_Shutdown");
    bind(wnd_proc, "UELibrary_WndProc");

    bind(prepare, "SandboxHost_Prepare");
    bind(detach, "SandboxHost_Detach");
    bind(ready, "SandboxHost_GameReady");
    bind(wants_exit, "SandboxHost_ExitRequested");
    bind(pump, "SandboxHost_PumpMessages");
    bind(sync, "SandboxHost_InitializeWindow");
    bind(launch, "SandboxHost_LaunchMission");

    if (!error.empty()) {
        return Result{std::unexpect, std::move(error)};
    }
    return {};
}
} // namespace ioj::sandbox_host
