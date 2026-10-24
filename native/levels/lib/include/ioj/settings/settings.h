#pragma once

#include <ioj/try_numeric_cast.h>
#include <ioj/vector.h>

#include <cmath>
#include <concepts>
#include <cstdint>
#include <string>
#include <type_traits>
#include <variant>
#include <vector>

namespace ioj::settings {
using SettingValue = std::variant<bool, std::int64_t, double, std::string, std::vector<double>>;

namespace setting_assignment {
template <typename T>
auto copy_value(SettingValue const& value, T& target) -> bool {
    auto const* source{std::get_if<T>(&value)};
    if (!source) {
        return false;
    }

    target = *source;
    return true;
}

template <typename From, typename T>
auto convert_number(From const number, T& target) -> bool {
    if (!std::isfinite(number)) {
        return false;
    }

    auto const converted{try_numeric_cast<T>(number)};
    if (!converted) {
        return false;
    }

    target = *converted;
    return true;
}

template <typename T>
auto assign_number(SettingValue const& value, T& target) -> bool {
    if (auto const* integer{std::get_if<std::int64_t>(&value)}) {
        return convert_number(*integer, target);
    }
    if (auto const* real{std::get_if<double>(&value)}) {
        return convert_number(*real, target);
    }
    return false;
}

template <typename T, typename Size>
auto assign_components(SettingValue const& value, T& target, Size const count) -> bool {
    auto const* source{std::get_if<std::vector<double>>(&value)};
    if (!source || source->size() != static_cast<std::size_t>(count)) {
        return false;
    }

    T converted{};
    for (Size i{}; i < count; ++i) {
        if (!convert_number((*source)[static_cast<std::size_t>(i)], converted[i])) {
            return false;
        }
    }

    target = converted;
    return true;
}

template <typename T, std::size_t N>
auto assign_vector(SettingValue const& value, Vector<T, N>& target) -> bool {
    return assign_components(value, target, N);
}

template <IndexedComponents T>
auto assign_vector(SettingValue const& value, T& target) -> bool {
    return assign_components(value, target, T::NumComponents);
}
}

struct Setting {
    std::string path;
    SettingValue value;

    // Return false on a type mismatch or a numeric value outside the destination's range.
    template <typename T>
    [[nodiscard]] auto assign(T& target) const -> bool {
        if constexpr (std::same_as<T, bool> || std::same_as<T, std::string> ||
                      std::same_as<T, std::vector<double>>) {
            return setting_assignment::copy_value(value, target);
        } else if constexpr (std::is_arithmetic_v<T>) {
            return setting_assignment::assign_number(value, target);
        } else if constexpr (requires { setting_assignment::assign_vector(value, target); }) {
            return setting_assignment::assign_vector(value, target);
        }
        return false;
    }
};

// Apply assignments in order; omitted fields retain their existing values.
using Settings = std::vector<Setting>;
}
