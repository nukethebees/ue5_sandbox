#include "sandbox/simulation/index_span.h"

#include <gtest/gtest.h>

TEST(IndexSpan, ReportsBounds) {
    constexpr FIndexSpan empty{};
    static_assert(empty.is_empty());

    constexpr FIndexSpan span{.offset = 7, .count = 4};
    static_assert(!span.is_empty());
    static_assert(span.start() == 7);
    static_assert(span.end() == 11);
}
