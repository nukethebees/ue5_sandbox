#include <ioj/presentation/hud_model.h>

#include <ioj/sim/capital_ships/sim.h>
#include <ioj/sim/entities/entity_ledger.h>
#include <ioj/sim/fighters/sim.h>
#include <ioj/sim/missions/mission.h>
#include <ioj/sim/player/sim.h>
#include <ioj/sim/sim_clock.h>
#include <ioj/sim/spinners/sim.h>
#include <ioj/sim/turrets/sim.h>

#include <algorithm>
#include <cassert>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <utility>

namespace ioj::presentation {
namespace hud_detail {
template <typename T>
auto replace(T& current, T next) -> bool {
    auto const changed{current != next};
    current = std::move(next);
    return changed;
}
}

void HudModel::initialise(sim::Mission const& mission, sim::LevelReadAccess const view) {
    deactivate();
    tick_loop_.initialise();
    std::array const periods{settings_.player_status_update_period,
                             settings_.entity_count_update_period,
                             settings_.mission_status_update_period};
    for (auto const period : periods) {
        auto const ticks{std::ceil(period.get() * tick_loop_.tick_rate.hz())};
        if (ticks < 1 || ticks > std::numeric_limits<PeriodicTickCountdown8::counter_type>::max()) {
            throw std::invalid_argument{"HUD update period is outside the countdown range"};
        }
        update_timers_.add_started(PeriodicTickCountdown8::period_type::make(
            static_cast<PeriodicTickCountdown8::counter_type>(ticks)));
    }
    simulation_.emplace(view);
    mission_ = &mission;
    top_killer_ids_.reserve(view.entity_ledger().num_unique_ids_issued());
    force_sample();
}
void HudModel::deactivate() {
    simulation_.reset();
    mission_ = nullptr;
    update_timers_.reset();
    mission_data_ = {};
    entity_count_data_ = {};
    kill_data_ = {};
    player_status_data_ = {};
    player_flight_data_ = {};
    top_killer_ids_.clear();
    selected_mapping_context_.clear();
}
auto HudModel::advance(double const delta_seconds) -> HudChanges {
    HudChanges changes;
    if (!active()) {
        return changes;
    }
    tick_loop_.add_time(delta_seconds);
    while (tick_loop_.try_tick()) {
        auto const next{tick(1)};
        changes.mission |= next.mission;
        changes.entity_counts |= next.entity_counts;
        changes.player_status |= next.player_status;
        changes.player_flight |= next.player_flight;
        changes.sampled_speed |= next.sampled_speed;
    }
    return changes;
}
auto HudModel::tick(PeriodicTickCountdown8::counter_type const num_ticks) -> HudChanges {
    assert(num_ticks >= 0);
    HudChanges changes;
    if (!active()) {
        return changes;
    }
    changes.player_flight = collect_player_flight_data();
    changes.sampled_speed = num_ticks > 0 && player_for_collection() != nullptr;
    update_timers_.tick(num_ticks);
    if (update_timers_.try_consume(0)) {
        changes.player_status = collect_player_status_data();
    }
    if (update_timers_.try_consume(1)) {
        changes.entity_counts = collect_entity_count_data();
        collect_kill_data();
    }
    if (update_timers_.try_consume(2)) {
        changes.mission = collect_mission_data();
    }
    return changes;
}
auto HudModel::force_sample() -> HudChanges {
    if (!active()) {
        return {};
    }
    auto const mission{collect_mission_data()};
    auto const entity_counts{collect_entity_count_data()};
    collect_kill_data();
    return {.mission = mission,
            .entity_counts = entity_counts,
            .player_status = collect_player_status_data(),
            .player_flight = collect_player_flight_data(),
            .sampled_speed = player_for_collection() != nullptr};
}
void HudModel::set_selected_mapping_context(std::string context) {
    selected_mapping_context_ = std::move(context);
}
auto HudModel::seconds_per_tick() const -> float {
    return static_cast<float>(tick_loop_.get_tick_period());
}

auto HudModel::has_mission_data() const -> bool {
    return mission_ && mission_->objectives().size() != 0;
}
auto HudModel::display_batches() const -> std::array<AgentDisplayBatch, 4> {
    assert(simulation_);
    auto const& view{*simulation_};
    auto const& capitals_sim{view.capitals()};
    auto const& fighters_sim{view.fighters()};
    auto const& turrets_sim{view.turrets()};
    auto const capitals{capitals_sim.get_entities()};
    auto const fighters{fighters_sim.entities()};
    auto const turrets{turrets_sim.get_entities()};
    auto const spinners{view.spinners().get_entities()};
    using sim::EntityType;
    return {
        {{EntityType::CapitalShip,
          capitals.entity_ids(),
          capitals.view_locations(),
          {},
          capitals_sim.healths(),
          capitals.teams()},
         {EntityType::Fighter,
          fighters.entity_ids(),
          fighters.view_locations(),
          fighters.view_velocities(),
          fighters_sim.healths(),
          fighters.teams()},
         {EntityType::Turret,
          turrets.entity_ids(),
          turrets.view_locations(),
          {},
          turrets_sim.healths(),
          turrets.teams()},
         {EntityType::TubeSpinner, spinners.entity_ids(), spinners.view_locations(), {}, {}, {}}}};
}
auto HudModel::collect_mission_data() -> bool {
    assert(mission_);
    MissionHudData next;
    next.active = static_cast<std::int32_t>(mission_->active_objectives().size());
    next.completed = static_cast<std::int32_t>(mission_->completed_objectives().size());
    next.inactive = static_cast<std::int32_t>(mission_->inactive_objectives().size());
    next.elapsed_seconds = simulation_->clock().simulation_time();
    auto const& objectives{mission_->objectives()};
    for (auto const id : mission_->active_objectives()) {
        if (!next.active_titles.empty()) {
            next.active_titles += " / ";
        }
        next.active_titles += to_string(objectives.team(id));
        next.active_titles += ": ";
        next.active_titles += objectives.title(id);
    }
    return hud_detail::replace(mission_data_, std::move(next));
}
auto HudModel::collect_entity_count_data() -> bool {
    return hud_detail::replace(
        entity_count_data_,
        EntityCountHudData{simulation_->entity_ledger().count_alive_per_team_and_type()});
}
void HudModel::collect_kill_data() {
    auto const& ledger{simulation_->entity_ledger()};
    auto const& entities{ledger.unique_entities()};
    auto const count{entities.num()};
    auto const kills{entities.kills()};
    auto const ids{entities.entity_ids()};
    auto const types{entities.entity_types()};
    auto const teams{entities.teams()};
    auto const killed_by{entities.killed_by()};
    top_killer_ids_.clear();
    for (std::uint32_t index{}; index < count; ++index) {
        if (kills[index] > 0) {
            top_killer_ids_.push_back(ids[index]);
        }
    }
    std::ranges::sort(top_killer_ids_, [&](sim::EntityId const lhs, sim::EntityId const rhs) {
        auto const left{ledger.history_index(lhs)};
        auto const right{ledger.history_index(rhs)};
        return kills[left] != kills[right] ? kills[left] > kills[right] : left < right;
    });
    kill_data_.top_killers.clear();
    for (auto const id : top_killer_ids_) {
        auto const index{ledger.history_index(id)};
        kill_data_.top_killers.push_back(
            {id, types[index], teams[index], static_cast<std::int32_t>(kills[index])});
    }
    kill_data_.team_kill_matrix = {};
    for (std::uint32_t index{}; index < count; ++index) {
        if (killed_by[index].is_valid()) {
            auto const killer{ledger.history_index(killed_by[index])};
            ++kill_data_.team_kill_matrix[std::to_underlying(teams[killer])]
                                         [std::to_underlying(types[index])];
        }
    }
}
auto HudModel::collect_player_status_data() -> bool {
    std::optional<PlayerStatusHudData> next;
    if (auto const* player{player_for_collection()}) {
        next.emplace(PlayerStatusHudData{
            .health = player->health(),
            .speed = player->speed(),
            .target_speed = player->controller_state().persistent_forward_target_speed,
            .energy = player->energy(),
            .points = player->kills(),
            .fire_rate = player->laser_fire_rate});
    }
    return hud_detail::replace(player_status_data_, next);
}
auto HudModel::collect_player_flight_data() -> bool {
    std::optional<PlayerFlightHudData> next;
    if (auto const* player{player_for_collection()}) {
        auto& data{next.emplace()};
        data.selected_mapping_context = selected_mapping_context_;
        auto const socket{player->get_middle_socket()};
        auto const& physical{player->physical_state()};
        auto const local{physical.transform.inverse_transform_vector_no_scale(physical.velocity)};
        auto const& intent{player->flight_intent()};
        auto const& controller{player->controller_state()};
        auto const& profile{player->active_flight_model_profile()};
        auto const forward_limit{
            std::max(profile.config.translation.forward.normal.positive_speed_limit, 1.f)};
        auto const right_limit{
            std::max(profile.config.translation.right.normal.positive_speed_limit, 1.f)};
        data.flight_vector_debug = {
            .turn_input = {intent.rotation.y(), intent.rotation.x()},
            .move_input = {intent.translation.y(), intent.translation.x()},
            .target_velocity = Vector2f{player->sampled_target_speed_scale()},
            .local_velocity = {static_cast<float>(local.y() / right_limit),
                               static_cast<float>(local.x() / forward_limit)}};
        data.ship_velocity = physical.velocity;
        data.target_velocity =
            physical.transform.forward() * controller.persistent_forward_target_speed +
            physical.transform.right() * controller.persistent_right_target_speed;
        data.flight_model_preset = profile.base_preset;
        data.flight_model_customized = profile.customized;
        data.throttle = intent.accelerator;
        data.boost_brake_state = controller.effective_action;
        data.crosshair_origin = socket.location;
        data.crosshair_direction = socket.forward();
    }
    return hud_detail::replace(player_flight_data_, std::move(next));
}
auto HudModel::sampled_speed_view() const -> std::optional<SampledSpeedHudView> {
    if (!active()) {
        return std::nullopt;
    }
    auto const* player{player_for_collection()};
    if (!player) {
        return std::nullopt;
    }
    return SampledSpeedHudView{player->speed_samples, player->speed_sample_index};
}
auto HudModel::player_for_collection() const -> sim::player::Sim const* {
    auto const* player{simulation_->player()};
    return player && player->unique_entity_id.is_valid() &&
                   simulation_->entity_ledger().is_valid_unique_id(player->unique_entity_id)
             ? player
             : nullptr;
}
}
