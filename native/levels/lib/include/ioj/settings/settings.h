#pragma once

#include <cmath>
#include <concepts>
#include <cstdint>
#include <limits>
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

template <typename T>
auto convert_number(long double const number, T& target) -> bool {
    if (!std::isfinite(number) || number < std::numeric_limits<T>::lowest() ||
        number > std::numeric_limits<T>::max()) {
        return false;
    }
    if constexpr (std::integral<T>) {
        if (std::trunc(number) != number) {
            return false;
        }
    }

    target = static_cast<T>(number);
    return true;
}

template <typename T>
auto assign_number(SettingValue const& value, T& target) -> bool {
    if (auto const* integer{std::get_if<std::int64_t>(&value)}) {
        return convert_number(static_cast<long double>(*integer), target);
    }
    if (auto const* real{std::get_if<double>(&value)}) {
        return convert_number(static_cast<long double>(*real), target);
    }
    return false;
}

template <typename T>
auto assign_vector(SettingValue const& value, T& target) -> bool {
    auto const* source{std::get_if<std::vector<double>>(&value)};
    if (!source || source->size() != 3) {
        return false;
    }

    using Component = std::remove_cvref_t<decltype(target.x())>;
    Component x{}, y{}, z{};
    if (!convert_number((*source)[0], x) || !convert_number((*source)[1], y) ||
        !convert_number((*source)[2], z)) {
        return false;
    }

    target = T{x, y, z};
    return true;
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
        } else if constexpr (requires {
                                 target.x();
                                 target.y();
                                 target.z();
                             }) {
            return setting_assignment::assign_vector(value, target);
        }
        return false;
    }
};

// Apply assignments in order; omitted fields retain their existing values.
using Settings = std::vector<Setting>;
}
