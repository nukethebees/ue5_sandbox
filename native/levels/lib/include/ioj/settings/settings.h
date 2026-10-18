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

struct Setting {
    std::string path;
    SettingValue value;

    // Return false on a type mismatch or a numeric value outside the destination's range.
    template <typename T>
    [[nodiscard]] auto assign(T& target) const -> bool {
        if constexpr (std::same_as<T, bool> || std::same_as<T, std::string> ||
                      std::same_as<T, std::vector<double>>) {
            if (auto const* source{std::get_if<T>(&value)}) {
                target = *source;
                return true;
            }
        } else if constexpr (std::is_arithmetic_v<T>) {
            auto const* integer{std::get_if<std::int64_t>(&value)};
            auto const* real{std::get_if<double>(&value)};
            if (integer || real) {
                auto const number{integer ? static_cast<long double>(*integer)
                                          : static_cast<long double>(*real)};
                if (std::isfinite(number) && number >= std::numeric_limits<T>::lowest() &&
                    number <= std::numeric_limits<T>::max()) {
                    if constexpr (std::integral<T>) {
                        if (std::trunc(number) != number) {
                            return false;
                        }
                    }
                    target = static_cast<T>(number);
                    return true;
                }
            }
        } else if constexpr (requires {
                                 target.x();
                                 target.y();
                                 target.z();
                             }) {
            if (auto const* source{std::get_if<std::vector<double>>(&value)};
                source && source->size() == 3) {
                using Component = std::remove_cvref_t<decltype(target.x())>;
                Component x{}, y{}, z{};
                if (!Setting{{}, (*source)[0]}.assign(x) || !Setting{{}, (*source)[1]}.assign(y) ||
                    !Setting{{}, (*source)[2]}.assign(z)) {
                    return false;
                }
                target = T{x, y, z};
                return true;
            }
        }
        return false;
    }
};

// Apply assignments in order; omitted fields retain their existing values.
using Settings = std::vector<Setting>;
}
