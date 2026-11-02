#pragma once

#include <lispb/indices.h>

#include <variant>
#include <vector>

namespace lispb::codegen::ir {

using NumericLiteral = std::variant<signed char,
                                    unsigned char,
                                    short,
                                    unsigned short,
                                    int,
                                    unsigned int,
                                    long,
                                    unsigned long,
                                    long long,
                                    unsigned long long,
                                    float,
                                    double,
                                    long double>;

struct Literals {
    std::vector<StringId> spelling;
};

struct NumericLiterals {
    std::vector<NumericLiteral> value;
};

} // namespace lispb::codegen::ir
