#pragma once

#include <ioj/min_max.h>

#include <concepts>
#include <cstddef>
#include <type_traits>

namespace ioj {

template <typename Enum>
struct EnumTraits;

template <typename Enum>
[[nodiscard]] constexpr auto enum_count() noexcept -> std::size_t = delete;

template <typename Enum>
inline constexpr auto enum_range = nullptr;

template <typename Enum>
concept HasEnumRange = std::is_enum_v<Enum> &&
                       std::same_as<std::remove_cv_t<decltype(enum_range<std::remove_cv_t<Enum>>)>,
                                    MinMax<std::remove_cv_t<Enum>>>;

template <HasEnumRange Enum>
inline constexpr auto enum_min{enum_range<std::remove_cv_t<Enum>>.min};

template <HasEnumRange Enum>
inline constexpr auto enum_max{enum_range<std::remove_cv_t<Enum>>.max};

} // namespace ioj
