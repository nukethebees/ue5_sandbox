#pragma once

#include "player/pause_menu.h"
#include <ioj/input/dispatch/events.h>
#include <ioj/memory/bump_resource.h>
#include <ioj/presentation/level_init.h>
#include <ioj/sim/levels/level_sim.h>

#include <chrono>
#include <optional>

namespace ioj {
struct LoadedLevel {
    LoadedLevel(sim::LevelSimInitData&& data,
                memory::BumpResource& arena,
                presentation::LevelPresentationSetup setup);

    auto input_context() const -> InputContext;
    void set_simulation_mode(levels::LaunchMode mode);
    auto elapsed_seconds() -> double;

    std::chrono::steady_clock::time_point previous_frame{std::chrono::steady_clock::now()};
    bool end_selected{};
    PauseMenu pause_menu;
    levels::ControlContext control_context;
    sim::LevelSim simulation;
    std::optional<presentation::LevelPresentationSetup> presentation_setup;
};

class LevelStorage {
  public:
    LevelStorage();

    auto get() -> LoadedLevel* { return level_ ? &*level_ : nullptr; }
    auto get() const -> LoadedLevel const* { return level_ ? &*level_ : nullptr; }
    void load(sim::LevelSimInitData&& data, presentation::LevelPresentationSetup setup);
    void unload();

  private:
    memory::BumpResource arena_;
    std::optional<LoadedLevel> level_;
};
}
