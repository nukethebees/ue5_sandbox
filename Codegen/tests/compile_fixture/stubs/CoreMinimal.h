#pragma once

#include <cstdint>
#include <stdexcept>

using int32 = std::int32_t;

#define check(expression)                                                                        \
    do {                                                                                         \
        if (!(expression)) {                                                                     \
            throw std::runtime_error{"check failed"};                                           \
        }                                                                                        \
    } while (false)
