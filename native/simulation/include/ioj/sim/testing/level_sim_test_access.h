#pragma once

#include <ioj/sim/level_sim.h>
#include <utility>

namespace ioj::sim {

struct LevelSimTestAccess {
    static void commit_fighter_spawns(LevelSim& simulation, FighterSpawnQueueConstView spawns) {
        simulation.clock_.phase = SimulationPhase::Preparation;
        auto& fighters{simulation.fighters_simulation_};
        auto const dt{static_cast<float>(simulation.clock_.get_tick_period())};
        fighters.prepare_tick(dt);
        fighters.queue_spawns(spawns);
        fighters.commit_spawns();
        fighters.prepare_tick(dt);
        simulation.rebuild_agent_indexes();
        simulation.query_manager_.refresh_spatial_index();
    }
    static void set_fighter_target(LevelSim& simulation,
                                   EntityUniqueId fighter,
                                   EntityUniqueId target,
                                   std::int8_t awareness_countdown = 0) {
        auto& fighters{simulation.fighters_simulation_};
        auto const index{simulation.agent_indexes_.find(fighter)};
        fighters.set_target_id(fighter, target);
        auto const data{fighters.entity_buffers.current().get_view().columns()};
        data.awareness_scan_countdowns[index] = awareness_countdown;
        data.attack_reposition_countdowns[index] = 0;
        data.navigation_update_countdowns_remaining_ticks[index] = 1;
    }
    static void think_fighters(LevelSim& simulation, ml::FrameScratch& scratch) {
        simulation.clock_.phase = SimulationPhase::Thinking;
        simulation.fighters_simulation_.think(
            static_cast<float>(simulation.clock_.get_tick_period()), scratch);
    }
    static void set_fighter_kinematics(LevelSim& simulation,
                                       EntityUniqueId fighter,
                                       Vector3f location,
                                       Vector3f velocity) {
        auto const index{simulation.agent_indexes_.find(fighter)};
        auto const data{
            simulation.fighters_simulation_.entity_buffers.current().get_view().columns()};
        data.locations.set(index, location);
        data.velocities.set(index, velocity);
        simulation.query_manager_.refresh_spatial_index();
    }
    static void resolve_fighter_damage(LevelSim& simulation, ml::FrameScratch& scratch) {
        simulation.clock_.phase = SimulationPhase::Resolution;
        simulation.combat_events_.prepare(simulation.agent_indexes_, scratch);
        simulation.fighters_simulation_.resolve_damage_events();
        simulation.fighters_simulation_.publish_deaths();
        simulation.combat_events_.reset();
    }
    static void remove_dead_fighters(LevelSim& simulation) {
        simulation.clock_.phase = SimulationPhase::ResolutionCommit;
        simulation.fighters_simulation_.remove_components();
        simulation.fighters_simulation_.remove_entities();
        simulation.rebuild_agent_indexes();
        simulation.query_manager_.refresh_spatial_index();
    }
    static void refresh_fighter_targets_and_plan(LevelSim& simulation, ml::FrameScratch& scratch) {
        simulation.clock_.phase = SimulationPhase::Thinking;
        auto& fighters{simulation.fighters_simulation_};
        fighters.refresh_target_data(scratch);
        fighters.plan_movement(static_cast<float>(simulation.clock_.get_tick_period()), scratch);
    }
    static void queue_fighter_orders(LevelSim& simulation, FighterOrderQueue const& orders) {
        fighters::CommandInterface{simulation.fighters_simulation_}.queue_orders(orders);
    }
    static void queue_direct_damage_events(LevelSim& simulation,
                                           DirectDamageEventsConstView events) {
        simulation.combat_events_.queue_damage(events);
    }
    static void queue_laser_spawns(LevelSim& simulation, lasers::SpawnRequestsConstView requests) {
        simulation.lasers_simulation_.queue_laser_spawns(requests);
    }
    static void begin_telemetry_run(LevelSim& simulation, LevelTelemetryRunMetadata metadata) {
        simulation.level_telemetry_manager_.begin_run(std::move(metadata));
    }
    static auto complete_mission(LevelSim& simulation) -> bool {
        return simulation.mission_manager_.complete_mission();
    }
};

} // namespace ioj::sim
