#pragma once

#include <cstddef>
#include <string_view>
#include <utility>

namespace ml {
class StaticStringView {
  public:
    template <std::size_t N>
    consteval StaticStringView(char const (&string)[N]) noexcept
        : data_{string}
        , size_{N - 1} {
        if (string[N - 1] != '\0') {
            std::unreachable();
        }
    }

    template <std::size_t N>
    StaticStringView(char (&)[N]) = delete;

    [[nodiscard]] constexpr auto view() const noexcept -> std::string_view {
        return {data_, size_};
    }

    [[nodiscard]] constexpr auto data() const noexcept -> char const* { return data_; }

    [[nodiscard]] constexpr auto size() const noexcept -> std::size_t { return size_; }
  private:
    char const* data_;
    std::size_t size_;
};
}
