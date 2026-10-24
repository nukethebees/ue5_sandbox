#pragma once
#include <ioj/sim/missions/mission.h>

#include <string>

namespace ioj::sim {
struct LevelMissionInitialisationData {
    CompiledMission definition{};
    std::string level_id{};
    std::string level_title{};
    bool save_results{true};
};
}
