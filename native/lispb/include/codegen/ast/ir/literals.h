#pragma once

#include <lispb/indices.h>

#include <concepts>
#include <vector>

namespace lispb::codegen::ir {

template <typename T>
concept NumericLiteralValue =
    std::same_as<T, signed char> || std::same_as<T, unsigned char> || std::same_as<T, short> ||
    std::same_as<T, unsigned short> || std::same_as<T, int> || std::same_as<T, unsigned int> ||
    std::same_as<T, long> || std::same_as<T, unsigned long> || std::same_as<T, long long> ||
    std::same_as<T, unsigned long long> || std::same_as<T, float> || std::same_as<T, double> ||
    std::same_as<T, long double>;

struct Literals {
    std::vector<StringId> spelling;
};

struct NumericLiterals {
    std::vector<int> ints;
    std::vector<unsigned int> unsigned_ints;
    std::vector<long> longs;
    std::vector<unsigned long> unsigned_longs;
    std::vector<long long> long_longs;
    std::vector<unsigned long long> unsigned_long_longs;
    std::vector<float> floats;
    std::vector<double> doubles;
    std::vector<long double> long_doubles;
};

} // namespace lispb::codegen::ir
