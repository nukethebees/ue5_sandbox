#include "TestTeamVisualData.h"

#include <Sandbox/logging/SandboxLogCategories.h>

#include <SandboxCore/array_utils.h>

UTestTeamVisualData::FColourArray::FColourArray() {
    ml::fill(colours, missing_colour);
}

UTestTeamVisualData::UTestTeamVisualData() {
    ensure_all_team_colours_exist();
}

void UTestTeamVisualData::PostLoad() {
    Super::PostLoad();

    ensure_all_team_colours_exist();
}

auto UTestTeamVisualData::build_team_colour_cache() const -> FColourArray {
    FColourArray out{};

    for (auto const& [team, colour] : colours) {
        out[team] = colour;
    }

    return out;
}
auto UTestTeamVisualData::build_team_colour_cache(UTestTeamVisualData const* const data)
    -> FColourArray {

    if (IsValid(data)) { return data->build_team_colour_cache(); }

    ensure(false);
    UE_LOG(
        LogSandbox, Warning, TEXT("UTestTeamVisualData::build_team_colour_cache: data is nullptr"));

    return FColourArray{};
}

void UTestTeamVisualData::ensure_all_team_colours_exist() {
    constexpr auto count{std::to_underlying(ETestTeam::COUNT)};

    for (int32 i{0}; i < count; ++i) {
        colours.FindOrAdd(static_cast<ETestTeam>(i), missing_colour);
    }

    TArray<ETestTeam> keys;
    colours.GetKeys(keys);
    for (auto k : keys) {
        if (!ml::is_valid(k)) { colours.Remove(k); }
    }
}
