#include "ioj/sim/index_span.h"

#include <gtest/gtest.h>

namespace ioj::sim::tests {

TEST(IndexSpan, ReportsBounds) {
    constexpr IndexSpan empty{};
    static_assert(empty.is_empty());

    constexpr IndexSpan span{.offset = 7, .count = 4};
    static_assert(!span.is_empty());
    static_assert(span.start() == 7);
    static_assert(span.end() == 11);
}

} // namespace tests
