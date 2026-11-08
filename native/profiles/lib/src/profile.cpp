#include <ioj/profiles/profile_id.h>
#include <ioj/profiles/profile_metadata.h>
#include <ioj/profiles/sample_profile_records.h>
#include <ioj/profiles/score_record.h>
#include <ioj/profiles/timestamp.h>

#include <algorithm>
#include <chrono>
#include <format>
#include <random>

namespace ioj::profiles {
void update_profile_metadata(ProfileMetadata& metadata, std::span<ScoreRecord const> records) {
    metadata.last_played_at = {};
    metadata.total_simulation_duration_seconds = 0.f;
    metadata.total_kills = 0;
    metadata.outcome_count = static_cast<std::int32_t>(records.size());
    for (auto const& record : records) {
        metadata.last_played_at = std::max(metadata.last_played_at, record.date);
        metadata.total_simulation_duration_seconds += record.time_seconds;
        metadata.total_kills += record.kills;
    }
}
auto current_timestamp() -> Timestamp {
    return std::chrono::duration_cast<std::chrono::seconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
}
auto make_profile_id() -> std::string {
    std::random_device random;
    return std::format("{:08x}{:08x}{:08x}{:08x}", random(), random(), random(), random());
}
auto sample_profile_records() -> std::vector<ScoreRecord> {
    using namespace std::chrono;
    auto const date = [](int y, unsigned m, unsigned d, int hour, int minute) {
        return duration_cast<seconds>(sys_days{year{y} / m / d}.time_since_epoch() + hours{hour} +
                                      minutes{minute})
            .count();
    };
    return {{.date = date(2026, 8, 20, 19, 5),
             .level_name = "VegaPatrol",
             .outcome = LevelOutcome::Victory,
             .kills = 18,
             .time_seconds = 3180.f},
            {.date = date(2026, 8, 24, 21, 15),
             .level_name = "ConvoyEscort",
             .outcome = LevelOutcome::Victory,
             .kills = 24,
             .time_seconds = 4946.f},
            {.date = date(2026, 8, 25, 9, 5),
             .level_name = "FleetInterception",
             .outcome = LevelOutcome::Defeat,
             .kills = 7,
             .time_seconds = 972.f},
            {.date = date(2026, 8, 27, 20, 30),
             .level_name = "VegaDefence",
             .outcome = LevelOutcome::Victory,
             .kills = 22,
             .time_seconds = 4354.f}};
}
}
