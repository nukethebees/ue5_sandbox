#pragma once

#include <string_view>

namespace ioj::testing {
template <typename Input, typename Expected>
struct TestCase {
    std::string_view name;
    Input input;
    Expected expected;
};
}
