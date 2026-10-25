#pragma once

#include <ioj/constrained_value.h>

#include <algorithm>
#include <array>
#include <concepts>
#include <cstddef>
#include <format>
#include <iterator>
#include <optional>
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

struct DiffResult {
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

using DiffPathComponent = std::variant<std::string_view, std::size_t>;

template <typename T>
void diff_value(T const& expected, T const& actual, DiffContext& context);

class DiffContext {
  public:
    explicit DiffContext(DiffResult& result)
        : result_{result} {}

    template <typename T>
    void diff(DiffPathComponent component, T const& expected, T const& actual) {
        path_.push_back(component);
        struct RestorePath {
            ~RestorePath() { path.pop_back(); }

            std::vector<DiffPathComponent>& path;
        } restore{path_};
        diff_value(expected, actual, *this);
    }

    template <typename T>
    void mismatch(T const& expected, T const& actual) {
        if (result_.differences.size() >= result_.limit) {
            ++result_.omitted;
            return;
        }
        std::string path;
        for (auto const& component : path_) {
            if (auto const* name{std::get_if<std::string_view>(&component)}) {
                if (!path.empty()) {
                    path += '.';
                }
                path += *name;
            } else {
                std::format_to(std::back_inserter(path), "[{}]", std::get<std::size_t>(component));
            }
        }
        result_.differences.push_back({std::move(path), describe(expected), describe(actual)});
    }
  private:
    template <typename T>
    static auto describe(T const& value) -> std::string {
        if constexpr (std::is_pointer_v<T> && std::is_convertible_v<T, void const*>) {
            return std::format("{}", static_cast<void const*>(value));
        } else if constexpr (std::formattable<T, char>) {
            return std::format("{}", value);
        } else if constexpr (std::is_enum_v<T>) {
            return std::format("{}", std::to_underlying(value));
        } else {
            return "<unformattable value>";
        }
    }

    DiffResult& result_;
    std::vector<DiffPathComponent> path_;
};

namespace diff_detail {
template <typename T>
inline constexpr bool sequence = false;
template <typename T, typename Allocator>
inline constexpr bool sequence<std::vector<T, Allocator>> = true;
template <typename T, std::size_t N>
inline constexpr bool sequence<std::array<T, N>> = true;

template <typename T>
inline constexpr bool optional = false;
template <typename T>
inline constexpr bool optional<std::optional<T>> = true;

template <typename T>
inline constexpr bool constrained = false;
template <typename T, ConstrainedValuePolicy policy>
inline constexpr bool constrained<ConstrainedValue<T, policy>> = true;

template <typename T>
concept Record = requires(typename T::Field field) {
    T::Field::Count;
    { T::field_name(field) } -> std::convertible_to<std::string_view>;
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
        constexpr auto count{static_cast<std::size_t>(Field::Count)};
        for (std::size_t index{}; index < count; ++index) {
            T::visit_field(
                expected, static_cast<Field>(index), [&]<Field field>(auto const& member) {
                    context.diff(
                        T::field_name(field), member, T::template get_field<field>(actual));
                });
        }
    } else if constexpr (diff_detail::constrained<T>) {
        diff_value(expected.get_ref(), actual.get_ref(), context);
    } else if constexpr (diff_detail::optional<T>) {
        if (expected.has_value() != actual.has_value()) {
            context.mismatch(std::string_view{expected ? "present" : "absent"},
                             std::string_view{actual ? "present" : "absent"});
        } else if (expected) {
            diff_value(*expected, *actual, context);
        }
    } else if constexpr (diff_detail::sequence<T>) {
        context.diff("size", expected.size(), actual.size());
        auto const count{std::min(expected.size(), actual.size())};
        for (std::size_t index{}; index < count; ++index) {
            context.diff(index, expected[index], actual[index]);
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
