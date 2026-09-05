#pragma once

#include <cstdint>
#include <cstdlib>
#include <stdexcept>
#include <string>
#include <string_view>

using int32 = std::int32_t;
using int64 = std::int64_t;
using uint8 = std::uint8_t;
using TCHAR = char;
using FString = std::string;
using FStringView = std::string_view;

#define TEXT(value) value
#define RESTRICT
#define UENUM(...)
#define UMETA(...)
#define COMPILE_FIXTURE_API

template <typename... Args>
auto ensureMsgf(bool const expression, TCHAR const*, Args&&...) -> bool {
    return expression;
}

#if defined(CODEGEN_CHECK_ABORT)
#define check(expression)                                                                        \
    do {                                                                                         \
        if (!(expression)) {                                                                     \
            std::abort();                                                                         \
        }                                                                                        \
    } while (false)
#else
#define check(expression)                                                                        \
    do {                                                                                         \
        if (!(expression)) {                                                                     \
            throw std::runtime_error{"check failed"};                                           \
        }                                                                                        \
    } while (false)
#endif

#define checkf(expression, message, ...) check(expression)
