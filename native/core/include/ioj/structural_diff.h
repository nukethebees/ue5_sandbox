#pragma once

#include <ioj/enum_strings.h>
#include <ioj/enum_traits.h>
#include <ioj/format_writer.h>

#include <algorithm>
#include <concepts>
#include <cstddef>
#include <format>
#include <iterator>
#include <optional>
#include <ranges>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace ioj {
struct Difference {
    auto operator==(Difference const&) const -> bool = default;

    std::string path;
    std::string expected;
    std::string actual;
};

using DiffPathComponent = std::variant<std::string_view, std::size_t>;

struct DiffReport {
    template <typename T>
    void diff(DiffPathComponent component, T const& expected, T const& actual);

    [[nodiscard]] auto empty() const -> bool { return differences.empty() && omitted == 0; }
    [[nodiscard]] auto message() const -> std::string {
        std::string text;
        for (auto const& difference : differences) {
            std::format_to(std::back_inserter(text),
                           "{}\n  Expected: {}\n  Actual:   {}\n",
                           difference.path,
                           difference.expected,
                           difference.actual);
        }
        if (omitted != 0) {
            std::format_to(std::back_inserter(text), "{} further differences omitted\n", omitted);
        }
        return text;
    }

    std::vector<Difference> differences;
    std::size_t omitted{};
    std::size_t limit{64};
};

class DiffContext;

template <typename T>
void diff_value(T const& expected, T const& actual, DiffContext& context);

class DiffContext {
  public:
    explicit DiffContext(DiffReport& result)
        : result_{result} {}

    template <typename T>
    void diff(DiffPathComponent component, T const& expected, T const& actual) {
        path_.push_back(component);
        RestorePath const restore{path_};
        diff_value(expected, actual, *this);
    }

    template <typename T>
    void mismatch(T const& expected, T const& actual) {
        if (result_.differences.size() >= result_.limit) {
            ++result_.omitted;
            return;
        }
        std::string path;
        FormatWriter write{std::back_inserter(path)};
        for (auto const& component : path_) {
            if (auto const* name{std::get_if<std::string_view>(&component)}) {
                if (!path.empty()) {
                    path += '.';
                }
                path += *name;
            } else {
                write("[{}]", std::get<std::size_t>(component));
            }
        }
        result_.differences.push_back({std::move(path), describe(expected), describe(actual)});
    }
  private:
    struct RestorePath {
        ~RestorePath() { path.pop_back(); }

        std::vector<DiffPathComponent>& path;
    };

    template <typename T>
    static auto describe(T const& value) -> std::string {
        if constexpr (std::is_pointer_v<T> && std::is_convertible_v<T, void const*>) {
            return std::format("{}", static_cast<void const*>(value));
        } else if constexpr (std::formattable<T, char>) {
            return std::format("{}", value);
        } else if constexpr (std::is_enum_v<T>) {
            return std::format("{}", std::to_underlying(value));
        } else if constexpr (requires {
                                 T::is_valid_value(value.value());
                                 value.raw_value();
                             }) {
            return std::format("{}", value.value());
        } else {
            return "<unformattable value>";
        }
    }

    DiffReport& result_;
    std::vector<DiffPathComponent> path_;
};

template <typename T>
void DiffReport::diff(DiffPathComponent component, T const& expected, T const& actual) {
    DiffContext{*this}.diff(component, expected, actual);
}

namespace diff_detail {
template <typename T>
concept Sequence = std::ranges::random_access_range<T const> && std::ranges::sized_range<T const> &&
                   !std::convertible_to<T const&, std::string_view>;

template <typename T>
concept Optional =
    requires { typename T::value_type; } && std::same_as<T, std::optional<typename T::value_type>>;

template <typename T>
concept Constrained = requires(T const& value, typename T::value_type const& inner) {
    { value.get_ref() } -> std::same_as<typename T::value_type const&>;
    { T::valid(inner) } -> std::same_as<bool>;
    { T::try_make(inner) } -> std::same_as<std::optional<T>>;
};

template <typename T>
concept Record = requires(typename T::Field field) {
    { enum_count<typename T::Field>() } -> std::same_as<std::size_t>;
    { to_string(field) } -> std::convertible_to<std::string_view>;
};

template <typename T>
concept Custom = requires(T const& expected, T const& actual, DiffContext& context) {
    diff_members(expected, actual, context);
};
}

template <typename T>
void diff_value(T const& expected, T const& actual, DiffContext& context) {
    if constexpr (diff_detail::Custom<T>) {
        diff_members(expected, actual, context);
    } else if constexpr (diff_detail::Record<T>) {
        using Field = typename T::Field;
        constexpr auto count{enum_count<Field>()};
        for (std::size_t index{}; index < count; ++index) {
            T::visit_field(
                expected, static_cast<Field>(index), [&]<Field field>(auto const& member) {
                    context.diff(to_string(field), member, T::template get_field<field>(actual));
                });
        }
    } else if constexpr (diff_detail::Constrained<T>) {
        diff_value(expected.get_ref(), actual.get_ref(), context);
    } else if constexpr (diff_detail::Optional<T>) {
        if (expected.has_value() != actual.has_value()) {
            context.mismatch(std::string_view{expected ? "present" : "absent"},
                             std::string_view{actual ? "present" : "absent"});
        } else if (expected) {
            diff_value(*expected, *actual, context);
        }
    } else if constexpr (diff_detail::Sequence<T>) {
        context.diff("size", std::ranges::size(expected), std::ranges::size(actual));
        auto const count{std::min(std::ranges::size(expected), std::ranges::size(actual))};
        auto const expected_begin{std::ranges::begin(expected)};
        auto const actual_begin{std::ranges::begin(actual)};
        for (std::size_t index{}; index < count; ++index) {
            context.diff(index, expected_begin[index], actual_begin[index]);
        }
    } else if constexpr (std::equality_comparable<T>) {
        if (!(expected == actual)) {
            context.mismatch(expected, actual);
        }
    } else {
        static_assert(sizeof(T) == 0,
                      "Structural diff requires a generated visitor, supported container, "
                      "equality, or ADL diff_members");
    }
}
}
