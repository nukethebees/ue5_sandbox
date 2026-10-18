#pragma once

#include <ioj/layout/analyzer/enum_code_value.hpp>

#include <optional>
#include <string>

namespace ioj::layout {

struct EnumeratorAnalysis {
    std::string name{};
    bool sentinel{};
    bool count_sentinel{};
    std::optional<EnumCodeValue> code{};
};

}
