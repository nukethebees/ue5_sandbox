#pragma once

#include <cstdio>
#include <cstdlib>
#include <string_view>

namespace ml {
inline void log_error(std::string_view const message) {
    std::fwrite(message.data(), 1, message.size(), stderr);
    std::fputc('\n', stderr);
    std::fflush(stderr);
}

[[noreturn]] inline void fatal_error(std::string_view const message) {
    log_error(message);
    std::abort();
}
}
