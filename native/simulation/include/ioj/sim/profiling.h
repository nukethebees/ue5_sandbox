#pragma once

#include <cstdint>

#if defined(SANDBOX_WITH_TRACY)
#include <tracy/Tracy.hpp>
#endif

namespace ioj::sim::profiling {
namespace stub_backend {
inline constexpr bool available{false};
inline auto is_connected() -> bool {
    return false;
}
inline void plot(char const*, std::int64_t) {}
inline void mark_frame(char const*) {}
}

#if defined(SANDBOX_WITH_TRACY)
namespace tracy_backend {
inline constexpr bool available{true};
inline auto is_connected() -> bool {
    return TracyIsConnected;
}
inline void plot(char const* const name, std::int64_t const value) {
    TracyPlot(name, value);
}
inline void mark_frame(char const* const name) {
    FrameMarkNamed(name);
}
}

using namespace tracy_backend;
#else
using namespace stub_backend;
#endif
}

#if defined(SANDBOX_WITH_TRACY)
#define SANDBOX_PROFILE_SCOPE(name_literal) ZoneScopedN(name_literal)
#else
#define SANDBOX_PROFILE_SCOPE(name_literal) static_cast<void>(0)
#endif
