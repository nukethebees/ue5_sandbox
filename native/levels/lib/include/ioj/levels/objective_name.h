#pragma once

#include <compare>
#include <functional>
#include <string>
#include <utility>

namespace ioj::levels {
// Identify an authored symbol; compilation resolves it to a runtime ObjectiveId.
struct ObjectiveName {
    ObjectiveName() = default;
    explicit ObjectiveName(std::string name)
        : value{std::move(name)} {}

    auto operator<=>(ObjectiveName const&) const = default;

    auto empty() const noexcept -> bool { return value.empty(); }

    std::string value{};
};
}

template <>
struct std::hash<ioj::levels::ObjectiveName> {
    auto operator()(ioj::levels::ObjectiveName const& name) const noexcept -> std::size_t {
        return std::hash<std::string>{}(name.value);
    }
};
