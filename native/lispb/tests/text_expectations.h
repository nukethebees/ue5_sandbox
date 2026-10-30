#pragma once

#include <gtest/gtest.h>

#include <initializer_list>
#include <source_location>
#include <string_view>

namespace lispb::test_support {
template <typename Fragments = std::initializer_list<std::string_view>>
inline void
    expect_contains_all(std::string_view const text,
                        Fragments const& expected,
                        std::source_location const location = std::source_location::current()) {
    for (auto const fragment : expected) {
        if (!text.contains(fragment)) {
            ADD_FAILURE_AT(location.file_name(), static_cast<int>(location.line()))
                << "Missing expected substring: " << fragment << "\nActual text:\n"
                << text;
        }
    }
}

template <typename Fragments = std::initializer_list<std::string_view>>
inline void
    expect_contains_none(std::string_view const text,
                         Fragments const& forbidden,
                         std::source_location const location = std::source_location::current()) {
    for (auto const fragment : forbidden) {
        if (text.contains(fragment)) {
            ADD_FAILURE_AT(location.file_name(), static_cast<int>(location.line()))
                << "Unexpected substring: " << fragment << "\nActual text:\n"
                << text;
        }
    }
}
}
