#pragma once

#include <algorithm>
#include <type_traits>

namespace ioj {
template <typename Enum>
    requires std::is_enum_v<Enum>
class EnumeratorRange {
  public:
    constexpr EnumeratorRange(Enum first, Enum last) noexcept
        : first_{std::min(first, last)}
        , last_{std::max(first, last)} {}

    [[nodiscard]] constexpr auto first() const noexcept -> Enum { return first_; }
    [[nodiscard]] constexpr auto last() const noexcept -> Enum { return last_; }

  private:
    Enum const first_;
    Enum const last_;
};
}
