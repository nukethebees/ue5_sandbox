#pragma once

#include <compare>
#include <functional>
#include <string>
#include <utility>

namespace ioj::levels {
struct LevelId {
    std::string value{};
    LevelId() = default;
    explicit LevelId(std::string text)
        : value{std::move(text)} {}
    [[nodiscard]] auto empty() const noexcept -> bool { return value.empty(); }
    auto operator<=>(LevelId const&) const = default;
};
struct CampaignId {
    std::string value{};
    CampaignId() = default;
    explicit CampaignId(std::string text)
        : value{std::move(text)} {}
    [[nodiscard]] auto empty() const noexcept -> bool { return value.empty(); }
    auto operator<=>(CampaignId const&) const = default;
};
struct EntityId {
    std::string value{};
    EntityId() = default;
    explicit EntityId(std::string text)
        : value{std::move(text)} {}
    [[nodiscard]] auto empty() const noexcept -> bool { return value.empty(); }
    auto operator<=>(EntityId const&) const = default;
};
struct TeamId {
    std::string value{};
    TeamId() = default;
    explicit TeamId(std::string text)
        : value{std::move(text)} {}
    [[nodiscard]] auto empty() const noexcept -> bool { return value.empty(); }
    auto operator<=>(TeamId const&) const = default;
};
}

namespace std {
template <>
struct hash<ioj::levels::LevelId> {
    auto operator()(ioj::levels::LevelId const& id) const noexcept -> size_t {
        return hash<string>{}(id.value);
    }
};
template <>
struct hash<ioj::levels::CampaignId> {
    auto operator()(ioj::levels::CampaignId const& id) const noexcept -> size_t {
        return hash<string>{}(id.value);
    }
};
template <>
struct hash<ioj::levels::EntityId> {
    auto operator()(ioj::levels::EntityId const& id) const noexcept -> size_t {
        return hash<string>{}(id.value);
    }
};
template <>
struct hash<ioj::levels::TeamId> {
    auto operator()(ioj::levels::TeamId const& id) const noexcept -> size_t {
        return hash<string>{}(id.value);
    }
};
}
