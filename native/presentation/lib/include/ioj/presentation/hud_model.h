#pragma once

#include <ioj/fixed_tick_loop.h>
#include <ioj/periodic_tick_countdown.h>
#include <ioj/presentation/hud_changes.h>
#include <ioj/presentation/level_frame.h>
#include <ioj/presentation/player_flight_hud_data.h>
#include <ioj/presentation/player_status_hud_data.h>
#include <ioj/presentation/sampled_speed_hud_view.h>
#include <ioj/sim/entities/team_entity_array.h>
#include <ioj/sim/levels/hud_settings.h>

#include <array>
#include <cstdint>
#include <optional>
#include <string>

namespace ioj::presentation {
struct PlayerHudData {
    PlayerStatusHudData status;
    PlayerFlightHudData flight;
};

class HudModel {
  public:
    void configure(sim::HudSettings settings) { settings_ = settings; }
    auto settings() const -> sim::HudSettings const& { return settings_; }
    void initialise(sim::EntityCounts const& counts, std::optional<PlayerFrame> const& player);
    void deactivate();
    auto advance(double delta_seconds,
                 sim::EntityCounts const& counts,
                 std::optional<PlayerFrame> const& player) -> HudChanges;
    auto tick(PeriodicTickCountdown8::counter_type num_ticks,
              sim::EntityCounts const& counts,
              std::optional<PlayerFrame> const& player) -> HudChanges;
    auto force_sample(sim::EntityCounts const& counts, std::optional<PlayerFrame> const& player)
        -> HudChanges;

    auto active() const -> bool { return active_; }
    auto entity_count_data() const -> sim::EntityCounts const& { return entity_count_data_; }
    auto player_data() const -> std::optional<PlayerHudData> const& { return player_data_; }
    auto seconds_per_tick() const -> float;
  private:
    sim::HudSettings settings_;
    bool active_{};
    FixedTickLoop tick_loop_;
    PeriodicTickCountdown8 update_timers_;
    sim::EntityCounts entity_count_data_;
    std::optional<PlayerHudData> player_data_;
};
}
