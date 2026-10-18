#pragma once

#include <codegen/schema/packed_integer_value.h>

#include <string>

namespace ioj::layout {

struct PackedNamedCodeAnalysis {
    std::string name{};
    lispb::codegen::PackedIntegerValue value{};
    bool sentinel{};
};

}
