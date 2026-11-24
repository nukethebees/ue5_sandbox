#pragma once

#include <ioj/host/game_host.h>
#include <ioj/settings/launch_display.h>
#include <ioj/settings/screen_resolution.h>

#include <cstdint>
#include <initializer_list>
#include <type_traits>

namespace ioj {
template <typename T>
constexpr bool host_record = std::is_standard_layout_v<T> && std::is_trivially_copyable_v<T>;
static_assert(host_record<GameHost> && host_record<HostDisplay> && host_record<ScreenResolution>);

constexpr std::uint32_t host_interface_version{2};

// Include compiled packing as well as the source fingerprint in the handshake.
constexpr auto host_layout_signature() -> std::uint64_t {
    constexpr std::uint64_t fnv1a_offset_basis{14695981039346656037ULL};
    constexpr std::uint64_t fnv1a_prime{1099511628211ULL};

    auto result{fnv1a_offset_basis};
    for (auto value : {sizeof(GameHost),
                       alignof(GameHost),
                       sizeof(HostDisplay),
                       alignof(HostDisplay),
                       sizeof(ScreenResolution),
                       alignof(ScreenResolution)}) {
        result = (result ^ value) * fnv1a_prime;
    }

    return result;
}

#ifdef SANDBOX_HOST_EXPORTS
#define SANDBOX_HOST_API __declspec(dllexport)
#else
#define SANDBOX_HOST_API
#endif
extern "C" {
    SANDBOX_HOST_API auto SandboxHost_Compatible(std::uint32_t version,
                                                 std::uint64_t layout,
                                                 char const* fingerprint) -> bool;
    // The host borrows the diagnostic until the next prepare. Detach after a successful prepare.
    SANDBOX_HOST_API auto SandboxHost_Prepare(GameHost* host,
                                              wchar_t const* scripts,
                                              wchar_t const* personal,
                                              ScreenResolution desktop,
                                              HostDisplay* saved) -> char const*;
    SANDBOX_HOST_API void SandboxHost_Detach();
    SANDBOX_HOST_API auto SandboxHost_GameReady() -> bool;
    SANDBOX_HOST_API auto SandboxHost_ExitRequested() -> bool;
    SANDBOX_HOST_API void SandboxHost_PumpMessages();
    SANDBOX_HOST_API auto
        SandboxHost_InitializeWindow(int x, int y, int width, int height, bool activate) -> bool;
    SANDBOX_HOST_API auto SandboxHost_LaunchMission(wchar_t const* path) -> bool;
}
#undef SANDBOX_HOST_API
}
