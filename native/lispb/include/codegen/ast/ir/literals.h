#pragma once

#include <lispb/indices.h>

#include <concepts>
#include <cstdint>
#include <vector>

namespace lispb::codegen::ir {

template <typename T>
concept NumericLiteralValue = std::same_as<T, std::int8_t> || std::same_as<T, std::int16_t> ||
                              std::same_as<T, std::int32_t> || std::same_as<T, std::int64_t> ||
                              std::same_as<T, std::uint8_t> || std::same_as<T, std::uint16_t> ||
                              std::same_as<T, std::uint32_t> || std::same_as<T, std::uint64_t> ||
                              std::same_as<T, float> || std::same_as<T, double>;

struct Literals {
    std::vector<StringId> spelling;
};

struct NumericLiterals {
    std::vector<std::int8_t> i8;
    std::vector<std::int16_t> i16;
    std::vector<std::int32_t> i32;
    std::vector<std::int64_t> i64;
    std::vector<std::uint8_t> u8;
    std::vector<std::uint16_t> u16;
    std::vector<std::uint32_t> u32;
    std::vector<std::uint64_t> u64;
    std::vector<float> f32;
    std::vector<double> f64;
};

} // namespace lispb::codegen::ir
