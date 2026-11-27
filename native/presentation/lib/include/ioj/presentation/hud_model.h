#pragma once

#include <ioj/fixed_tick_loop.h>
#include <ioj/periodic_tick_countdown.h>
#include <ioj/presentation/entity_count_hud_data.h>
#include <ioj/presentation/hud_changes.h>
#include <ioj/presentation/level_frame.h>
#include <ioj/presentation/player_flight_hud_data.h>
#include <ioj/presentation/player_status_hud_data.h>
#include <ioj/presentation/sampled_speed_hud_view.h>
#include <ioj/sim/levels/hud_settings.h>

#include <array>
#include <cstdint>
#include <optional>
#include <string>

namespace ioj::presentation {
class HudModel {
  public:
    void configure(sim::HudSettings settings) { settings_ = settings; }
    auto settings() const -> sim::HudSettings const& { return settings_; }
    void initialise(LevelFrame const& frame);
    void deactivate();
    auto advance(double delta_seconds, LevelFrame const& frame) -> HudChanges;
    auto tick(PeriodicTickCountdown8::counter_type num_ticks, LevelFrame const& frame)
        -> HudChanges;
    auto force_sample(LevelFrame const& frame) -> HudChanges;

    auto active() const -> bool { return active_; }
    auto entity_count_data() const -> EntityCountHudData const& { return entity_count_data_; }
    auto player_status_data() const -> std::optional<PlayerStatusHudData> const& {
        return player_status_data_;
    }
    auto player_flight_data() const -> std::optional<PlayerFlightHudData> const& {
        return player_flight_data_;
    }
    auto seconds_per_tick() const -> float;
  private:
    sim::HudSettings settings_;
    bool active_{};
    FixedTickLoop tick_loop_;
    PeriodicTickCountdown8 update_timers_;
    EntityCountHudData entity_count_data_;
    std::optional<PlayerStatusHudData> player_status_data_;
    std::optional<PlayerFlightHudData> player_flight_data_;
};
}
