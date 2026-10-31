#pragma once

#include <ioj/periodic_tick_countdown.h>
#include <ioj/presentation/agent_display_batch.h>
#include <ioj/presentation/entity_count_hud_data.h>
#include <ioj/presentation/hud_changes.h>
#include <ioj/presentation/kill_hud_data.h>
#include <ioj/presentation/mission_hud_data.h>
#include <ioj/presentation/player_flight_hud_data.h>
#include <ioj/presentation/player_status_hud_data.h>
#include <ioj/presentation/sampled_speed_hud_view.h>
#include <ioj/sim/fixed_tick_loop.h>
#include <ioj/sim/levels/hud_settings.h>
#include <ioj/sim/levels/level_read_access.h>

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace ioj::sim {
class Mission;
}

namespace ioj::presentation {
class HudModel {
  public:
    void configure(sim::HudSettings settings) { settings_ = settings; }
    auto settings() const -> sim::HudSettings const& { return settings_; }
    // The mission and simulation must outlive this model and remain stable while sampling.
    void initialise(sim::Mission const& mission, sim::LevelReadAccess view);
    void deactivate();
    auto advance(double delta_seconds) -> HudChanges;
    auto tick(PeriodicTickCountdown8::counter_type num_ticks) -> HudChanges;
    auto force_sample() -> HudChanges;
    void set_selected_mapping_context(std::string context);

    auto active() const -> bool { return simulation_.has_value(); }
    auto has_mission_data() const -> bool;
    auto mission_data() const -> MissionHudData const& { return mission_data_; }
    auto entity_count_data() const -> EntityCountHudData const& { return entity_count_data_; }
    auto kill_data() const -> KillHudData const& { return kill_data_; }
    auto player_status_data() const -> std::optional<PlayerStatusHudData> const& {
        return player_status_data_;
    }
    auto player_flight_data() const -> std::optional<PlayerFlightHudData> const& {
        return player_flight_data_;
    }
    // Borrowed until the player simulation is advanced, reconfigured, or destroyed.
    auto sampled_speed_view() const -> std::optional<SampledSpeedHudView>;
    auto seconds_per_tick() const -> float;
    auto player_for_collection() const -> sim::player::Sim const*;
    auto display_batches() const -> std::array<AgentDisplayBatch, 4>;
  private:
    auto collect_mission_data() -> bool;
    auto collect_entity_count_data() -> bool;
    void collect_kill_data();
    auto collect_player_status_data() -> bool;
    auto collect_player_flight_data() -> bool;

    sim::HudSettings settings_;
    std::optional<sim::LevelReadAccess> simulation_;
    sim::Mission const* mission_{};
    sim::FixedTickLoop tick_loop_;
    PeriodicTickCountdown8 update_timers_;
    MissionHudData mission_data_;
    EntityCountHudData entity_count_data_;
    KillHudData kill_data_;
    std::optional<PlayerStatusHudData> player_status_data_;
    std::optional<PlayerFlightHudData> player_flight_data_;
    std::vector<sim::EntityId> top_killer_ids_;
    std::string selected_mapping_context_;
};
}
