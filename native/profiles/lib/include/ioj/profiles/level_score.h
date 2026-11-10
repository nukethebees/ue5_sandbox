#pragma once
#include <ioj/profiles/score_record.h>

#include <optional>
namespace ioj::sim {
struct LevelSim;
}
namespace ioj::profiles {
auto make_level_score(sim::LevelSim const& simulation, Timestamp date)
    -> std::optional<ScoreRecord>;
}
