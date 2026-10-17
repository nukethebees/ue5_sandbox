#pragma once

#include <codegen/schema/packed_integer_value.h>

#include <string>

namespace lispb::schema {
struct PackedNamedCode {
    std::string name;
    codegen::PackedIntegerValue value;
    bool sentinel{};
};
}
