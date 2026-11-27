#pragma once

#include <ioj/memory/bump_resource.h>
#include <ioj/presentation/level_init.h>
#include <ioj/sim/levels/level_sim.h>

#include <optional>

namespace ioj {
struct LoadedLevel {
    LoadedLevel(sim::LevelSimInitData&& data,
                memory::BumpResource& arena,
                presentation::LevelPresentationSetup setup);

    sim::LevelSim simulation;
    std::optional<presentation::LevelPresentationSetup> presentation_setup;
};

class LevelStorage {
  public:
    LevelStorage();

    auto get() -> LoadedLevel* { return level_ ? &*level_ : nullptr; }
    void load(sim::LevelSimInitData&& data, presentation::LevelPresentationSetup setup);
    void unload();

  private:
    memory::BumpResource arena_;
    std::optional<LoadedLevel> level_;
};
}
