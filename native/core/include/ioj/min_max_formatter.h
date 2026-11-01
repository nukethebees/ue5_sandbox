#pragma once

#include <ioj/min_max.h>

#include <format>
#include <type_traits>

template <typename T, typename Char>
    requires std::formattable<T const&, Char>
struct std::formatter<ioj::MinMax<T>, Char> : std::formatter<std::remove_cvref_t<T>, Char> {
    using Base = std::formatter<std::remove_cvref_t<T>, Char>;

    template <typename FormatContext>
    auto format(ioj::MinMax<T> const& range, FormatContext& context) const ->
        typename FormatContext::iterator {
        auto output{context.out()};
        *output++ = Char{'['};
        context.advance_to(output);
        output = Base::format(range.min, context);

        *output++ = Char{','};
        *output++ = Char{' '};
        context.advance_to(output);
        output = Base::format(range.max, context);

        *output++ = Char{']'};
        return output;
    }
};
