#pragma once
#include <concepts>
#include <string_view>

namespace lispb::codegen::sexpr {
template <typename T>
concept Serialized = requires(T value) {
    { to_serialized_string(value) } -> std::convertible_to<std::string_view>;
};
}
