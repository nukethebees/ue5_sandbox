#pragma once

#include <ioj/type_traits.h>

#include <compare>
#include <functional>
#include <string>
#include <utility>

namespace ioj::levels {
struct LevelId {
    LevelId() = default;
    explicit LevelId(std::string text)
        : value{std::move(text)} {}

    auto operator<=>(LevelId const&) const = default;

    [[nodiscard]] auto empty() const noexcept -> bool { return value.empty(); }

    std::string value{};
};
struct CampaignId {
    CampaignId() = default;
    explicit CampaignId(std::string text)
        : value{std::move(text)} {}

    auto operator<=>(CampaignId const&) const = default;

    [[nodiscard]] auto empty() const noexcept -> bool { return value.empty(); }

    std::string value{};
};
struct EntityId {
    EntityId() = default;
    explicit EntityId(std::string text)
        : value{std::move(text)} {}

    auto operator<=>(EntityId const&) const = default;

    [[nodiscard]] auto empty() const noexcept -> bool { return value.empty(); }

    std::string value{};
};
}

namespace ioj::levels {
template <typename T>
concept Identifier = ioj::SameAsAny<T, LevelId, CampaignId, EntityId>;
}

namespace std {
template <ioj::levels::Identifier Id>
struct hash<Id> {
    auto operator()(Id const& id) const noexcept -> size_t { return hash<string>{}(id.value); }
};
}
