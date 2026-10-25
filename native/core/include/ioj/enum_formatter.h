#pragma once

#include <concepts>
#include <format>
#include <string_view>
#include <type_traits>
#include <utility>

namespace ioj {
template <typename E>
    requires (std::is_enum_v<E> &&
              requires(E value) {
                  { to_string(value) } -> std::convertible_to<std::string_view>;
              })
class EnumFormatter {
  public:
    constexpr auto parse(std::format_parse_context& context) {
        auto current{context.begin()};
        presentation_ = 'n';
        if (current != context.end() && (*current == 'n' || *current == 's' || *current == 'l')) {
            auto const selected{*current++};
            if (selected == 's' && !has_serialized_) {
                throw std::format_error{"Enum has no serialized string representation"};
            }
            if (selected == 'l' && !has_display_) {
                throw std::format_error{"Enum has no display string representation"};
            }
            presentation_ = selected;
        }
        context.advance_to(current);
        return formatter_.parse(context);
    }

    template <typename FormatContext>
    auto format(E value, FormatContext& context) const {
        switch (presentation_) {
            case 's': {
                if constexpr (has_serialized_) {
                    return formatter_.format(to_serialized_string(value), context);
                }
                std::unreachable();
            }
            case 'l': {
                if constexpr (has_display_) {
                    return formatter_.format(to_display_string(value), context);
                }
                std::unreachable();
            }
            default: {
                return formatter_.format(to_string(value), context);
            }
        }
    }
  private:
    static constexpr bool has_serialized_{requires(E value) {
        { to_serialized_string(value) } -> std::convertible_to<std::string_view>;
    }};
    static constexpr bool has_display_{requires(E value) {
        { to_display_string(value) } -> std::convertible_to<std::string_view>;
    }};

    std::formatter<std::string_view> formatter_;
    char presentation_{'n'};
};
}
