#pragma once

#include <compare>
#include <functional>
#include <string>
#include <utility>

namespace ioj::levels {
// Identify an authored symbol; compilation resolves it to a runtime ObjectiveId.
struct ObjectiveName {
    std::string value{};
    ObjectiveName() = default;
    explicit ObjectiveName(std::string name)
        : value{std::move(name)} {}
    auto empty() const noexcept -> bool { return value.empty(); }
    auto operator<=>(ObjectiveName const&) const = default;
};
}

template <>
struct std::hash<ioj::levels::ObjectiveName> {
    auto operator()(ioj::levels::ObjectiveName const& name) const noexcept -> std::size_t {
        return std::hash<std::string>{}(name.value);
    }
};
