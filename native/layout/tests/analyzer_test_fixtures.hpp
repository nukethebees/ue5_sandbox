#pragma once

#include <ioj/layout/analyzer.hpp>
#include <ioj/layout/document.hpp>

#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <string_view>

namespace ioj::layout {

struct AnalysisCase {
    std::string_view name;
    std::string_view source;
    void (*verify)(Document const&);
};

}
