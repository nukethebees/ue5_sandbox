#include <ioj/require.h>

#include <ioj/diagnostics.h>

#include <cstdio>

namespace ioj::detail {
[[noreturn]] void require_failed(std::string_view const message,
                                 std::source_location const location) noexcept {
    // NOLINTNEXTLINE(modernize-use-std-print) -- Keep contract-failure reporting non-throwing.
    std::fprintf(stderr,
                 "%s:%u: %s: ",
                 location.file_name(),
                 static_cast<unsigned>(location.line()),
                 location.function_name());
    fatal_error(message);
}
} // namespace ioj::detail
