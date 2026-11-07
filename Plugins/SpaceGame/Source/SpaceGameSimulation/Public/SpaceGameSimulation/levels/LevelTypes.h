#pragma once

#include <ioj/levels/team.h>

#include <CoreMinimal.h>

namespace ioj {
struct FLevelId {
    FName value{NAME_None};

    auto operator==(FLevelId const&) const -> bool = default;
    auto is_set() const noexcept -> bool { return !value.IsNone(); }
};

inline auto GetTypeHash(FLevelId const& id) -> uint32 {
    return GetTypeHash(id.value);
}

struct FCampaignId {
    FName value{NAME_None};

    auto operator==(FCampaignId const&) const -> bool = default;
    auto is_set() const noexcept -> bool { return !value.IsNone(); }
};

inline auto GetTypeHash(FCampaignId const& id) -> uint32 {
    return GetTypeHash(id.value);
}

}
