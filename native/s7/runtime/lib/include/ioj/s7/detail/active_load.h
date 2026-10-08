#pragma once

#include <ioj/s7/detail/load_token.h>

#include <cstddef>
#include <string>

namespace ioj::s7::detail {
struct ActiveLoad {
    LoadToken token;
    std::string key;
    std::size_t bytes{};
};
}
