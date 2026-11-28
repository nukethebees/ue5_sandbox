#include "player/loaded_level.h"

#include <sbx/memory_resource.h>

#include <algorithm>
#include <cassert>
#include <utility>

namespace ioj {
LoadedLevel::LoadedLevel(sim::LevelSimInitData&& data,
                         memory::BumpResource& arena,
                         presentation::LevelPresentationSetup setup)
    : control_context{setup.presentation_mode == levels::PresentationMode::SimulationOnly
                          ? levels::ControlContext::Benchmark
                          : setup.control_context}
    , simulation{std::move(data), arena}
    , presentation_setup{std::move(setup)} {}

auto LoadedLevel::input_context() const -> InputContext {
    switch (control_context) {
        case levels::ControlContext::Observer:
            return InputContext::Observer;
        case levels::ControlContext::Benchmark:
            return InputContext::Benchmark;
        case levels::ControlContext::Player: {
            auto const* ship{simulation.player_ship_simulation()};
            if (ship && ship->is_alive(simulation.entity_tables().health)) {
                return InputContext::Gameplay;
            }
            break;
        }
        case levels::ControlContext::None:
            break;
    }
    return InputContext::Menu;
}
void LoadedLevel::set_simulation_mode(levels::LaunchMode mode) {
    if (mode == levels::LaunchMode::Paused && simulation.state() == sim::LevelState::Running) {
        simulation.pause();
    } else if (mode == levels::LaunchMode::Running &&
               simulation.state() == sim::LevelState::Paused) {
        simulation.start();
        previous_frame = std::chrono::steady_clock::now();
    }
}
void LoadedLevel::update_pause(bool menu_open) {
    if (menu_open && !resume_when_paused) {
        set_simulation_mode(levels::LaunchMode::Paused);
        resume_when_paused = true;
    } else if (resume_when_paused && !menu_open && simulation.state() == sim::LevelState::Paused) {
        set_simulation_mode(levels::LaunchMode::Running);
        resume_when_paused = false;
    }
}
auto LoadedLevel::elapsed_seconds() -> double {
    auto const now{std::chrono::steady_clock::now()};
    return std::min(std::chrono::duration<double>{now - std::exchange(previous_frame, now)}.count(),
                    0.1);
}

LevelStorage::LevelStorage()
    : arena_{memory::BumpResource::default_capacity_bytes, *sbx::memory::mimalloc_resource()} {}

void LevelStorage::load(sim::LevelSimInitData&& data, presentation::LevelPresentationSetup setup) {
    assert(!level_);
    try {
        level_.emplace(std::move(data), arena_, std::move(setup));
    } catch (...) {
        arena_.reset();
        throw;
    }
}
void LevelStorage::unload() {
    level_.reset();
    arena_.reset();
}
}
