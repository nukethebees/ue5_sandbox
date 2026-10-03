#pragma once

#include <cstddef>

namespace ml {

template <typename Enum>
struct EnumTraits;

template <typename Enum>
[[nodiscard]] constexpr auto enum_count() noexcept -> std::size_t = delete;

} // namespace ml
