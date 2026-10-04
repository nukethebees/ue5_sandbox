#include <ioj/sim/testing/sim_clock_test_access.h>
#pragma once

#include <ioj/sim/column_math.h>
#include <ioj/sim/level_sim.h>

#include <sandbox/core/frame_memory_resource.h>

#include <utility>

namespace ioj::sim {

struct LevelSimTestAccess {
    static void enter_thinking_phase(LevelSim& simulation) {
        SimClockTestAccess::set_phase(simulation.clock_, SimulationPhase::Thinking);
    }
    static void update_entity_lookup_tables(LevelSim& simulation) {
        auto const previous{simulation.clock_.phase()};
        SimClockTestAccess::set_phase(simulation.clock_, SimulationPhase::Preparation);
        simulation.capital_ships_simulation_.update_entity_lookup_table();
        simulation.fighters_simulation_.update_entity_lookup_table();
        SimClockTestAccess::set_phase(simulation.clock_, previous);
    }
#ifndef NDEBUG
    static auto capture_thinking_phase_state(LevelSim& simulation) {
        SimClockTestAccess::set_phase(simulation.clock_, SimulationPhase::Thinking);
        return simulation.capture_thinking_phase_state();
    }
    static auto check_thinking_entry_invariants(
        LevelSim const& simulation, std::span<LevelSim::ThinkingEntityState const> state) -> bool {
        return simulation.check_thinking_entry_invariants(state);
    }
    static auto check_thinking_phase_invariants(
        LevelSim const& simulation, std::span<LevelSim::ThinkingEntityState const> state) -> bool {
        return simulation.check_phase_invariants(state);
    }
    static auto capital_entities(LevelSim& simulation) -> CapitalEntityData::View {
        return simulation.capital_ships_simulation_.entities.get_view();
    }
    static void set_capital_health(LevelSim& simulation, std::uint32_t row, Health health) {
        auto const data{capital_entities(simulation)};
        simulation.entity_tables_.health.get_view<EntityType::CapitalShip>(data.num())
            .set_health(row, health);
    }
#endif

    static auto player_simulation(LevelSim& simulation) -> player::Sim& {
        return simulation.player_ship_simulation_.value();
    }
    static void queue_fighter_spawns(LevelSim& simulation, FighterSpawnQueue::ConstView spawns) {
        fighters::CommandInterface{simulation.fighters_simulation_}.queue_spawns(spawns);
    }
    static void reassign_pending_fighter_spawns(LevelSim& simulation,
                                                EntityUniqueId parent,
                                                EntityUniqueId replacement) {
        fighters::CommandInterface{simulation.fighters_simulation_}.reassign_pending_spawns(
            parent, replacement);
    }
    static void refresh_fighter_membership(LevelSim& simulation,
                                           ml::FrameMemoryResource* const scratch_resource) {
        simulation.capital_ships_simulation_.refresh_fighter_ids(scratch_resource);
    }
    static void
        set_fighter_parent(LevelSim& simulation, EntityUniqueId fighter, EntityUniqueId parent) {
        simulation.fighters_simulation_.set_parent_id(fighter, parent);
    }
    static void prepare_fighters(LevelSim& simulation) {
        auto const previous_phase{simulation.clock_.phase()};
        SimClockTestAccess::set_phase(simulation.clock_, SimulationPhase::Preparation);
        simulation.fighters_simulation_.commit_orders();
        simulation.fighters_simulation_.prepare_tick(
            static_cast<float>(simulation.clock_.get_tick_period()));
        update_entity_lookup_tables(simulation);
        SimClockTestAccess::set_phase(simulation.clock_, previous_phase);
    }
    static void resolve_ship_damage(LevelSim& simulation,
                                    ml::FrameMemoryResource* const scratch_resource) {
        auto const previous_phase{simulation.clock_.phase()};
        SimClockTestAccess::set_phase(simulation.clock_, SimulationPhase::Resolution);
        simulation.combat_events_.prepare(simulation.entity_tables_.lookups, scratch_resource);
        simulation.capital_ships_simulation_.resolve_damage_events();
        simulation.fighters_simulation_.resolve_damage_events(scratch_resource);
        simulation.capital_ships_simulation_.resolve_fighters_of_dying_capitals();
        simulation.capital_ships_simulation_.publish_deaths();
        simulation.fighters_simulation_.publish_deaths();
        simulation.combat_events_.reset();
        SimClockTestAccess::set_phase(simulation.clock_, previous_phase);
    }
    static void remove_dead_ships(LevelSim& simulation) {
        auto const previous_phase{simulation.clock_.phase()};
        SimClockTestAccess::set_phase(simulation.clock_, SimulationPhase::ResolutionCommit);
        simulation.capital_ships_simulation_.remove_components();
        simulation.fighters_simulation_.remove_components();
        simulation.capital_ships_simulation_.remove_entities();
        simulation.fighters_simulation_.remove_entities();
        update_entity_lookup_tables(simulation);
        simulation.refresh_spatial_index();
        SimClockTestAccess::set_phase(simulation.clock_, previous_phase);
    }
    static void register_capitals(LevelSim& simulation, LevelCapitalSpawnEvents::ConstView spawns) {
        auto const previous_phase{simulation.clock_.phase()};
        SimClockTestAccess::set_phase(simulation.clock_, SimulationPhase::Preparation);
        simulation.capital_ships_simulation_.register_ships(spawns);
        update_entity_lookup_tables(simulation);
        SimClockTestAccess::set_phase(simulation.clock_, previous_phase);
    }
    static void commit_fighter_spawns(LevelSim& simulation, FighterSpawnQueue::ConstView spawns) {
        auto const previous_phase{simulation.clock_.phase()};
        SimClockTestAccess::set_phase(simulation.clock_, SimulationPhase::Preparation);
        auto& fighters{simulation.fighters_simulation_};
        auto const dt{static_cast<float>(simulation.clock_.get_tick_period())};
        fighters.prepare_tick(dt);
        fighters.queue_spawns(spawns);
        fighters.commit_spawns();
        fighters.prepare_tick(dt);
        update_entity_lookup_tables(simulation);
        simulation.refresh_spatial_index();
        SimClockTestAccess::set_phase(simulation.clock_, previous_phase);
    }
    static void set_fighter_target(LevelSim& simulation,
                                   EntityUniqueId fighter,
                                   EntityUniqueId target,
                                   std::int8_t awareness_countdown = 0) {
        auto& fighters{simulation.fighters_simulation_};
        auto const index{simulation.fighters_simulation_.find_index(fighter)};
        fighters.set_target_id(fighter, target);
        auto const data{fighters.entity_buffers.current().get_view()};
        data.awareness_scan_countdowns()[index] = awareness_countdown;
        data.attack_reposition_countdowns()[index] = 0;
        data.navigation_update_countdowns_remaining_ticks()[index] = 1;
    }
    static void think_fighters(LevelSim& simulation,
                               ml::FrameMemoryResource* const scratch_resource) {
        auto const previous_phase{simulation.clock_.phase()};
        SimClockTestAccess::set_phase(simulation.clock_, SimulationPhase::Thinking);
        simulation.fighters_simulation_.think(
            static_cast<float>(simulation.clock_.get_tick_period()), scratch_resource);
        SimClockTestAccess::set_phase(simulation.clock_, previous_phase);
    }
    static void set_fighter_kinematics(LevelSim& simulation,
                                       EntityUniqueId fighter,
                                       Vector3f location,
                                       Vector3f velocity) {
        auto const index{simulation.fighters_simulation_.find_index(fighter)};
        auto const data{simulation.fighters_simulation_.entity_buffers.current().get_view()};
        set_vector(data.view_locations(), index, location);
        set_vector(data.view_velocities(), index, velocity);
        simulation.refresh_spatial_index();
    }
    static void resolve_fighter_damage(LevelSim& simulation,
                                       ml::FrameMemoryResource* const scratch_resource) {
        auto const previous_phase{simulation.clock_.phase()};
        SimClockTestAccess::set_phase(simulation.clock_, SimulationPhase::Resolution);
        simulation.combat_events_.prepare(simulation.entity_tables_.lookups, scratch_resource);
        simulation.fighters_simulation_.resolve_damage_events(scratch_resource);
        simulation.fighters_simulation_.publish_deaths();
        simulation.combat_events_.reset();
        SimClockTestAccess::set_phase(simulation.clock_, previous_phase);
    }
    static void remove_dead_fighters(LevelSim& simulation) {
        auto const previous_phase{simulation.clock_.phase()};
        SimClockTestAccess::set_phase(simulation.clock_, SimulationPhase::ResolutionCommit);
        simulation.fighters_simulation_.remove_components();
        simulation.fighters_simulation_.remove_entities();
        update_entity_lookup_tables(simulation);
        simulation.refresh_spatial_index();
        SimClockTestAccess::set_phase(simulation.clock_, previous_phase);
    }
    static void refresh_fighter_targets_and_plan(LevelSim& simulation,
                                                 ml::FrameMemoryResource* const scratch_resource) {
        auto const previous_phase{simulation.clock_.phase()};
        SimClockTestAccess::set_phase(simulation.clock_, SimulationPhase::Thinking);
        auto& fighters{simulation.fighters_simulation_};
        fighters.refresh_target_data(scratch_resource);
        fighters.plan_movement(static_cast<float>(simulation.clock_.get_tick_period()),
                               scratch_resource);
        SimClockTestAccess::set_phase(simulation.clock_, previous_phase);
    }
    static void queue_fighter_orders(LevelSim& simulation, FighterOrderQueue const& orders) {
        fighters::CommandInterface{simulation.fighters_simulation_}.queue_orders(orders);
    }
    static void queue_direct_damage_events(LevelSim& simulation,
                                           DirectDamageEventsConstView events) {
        simulation.combat_events_.queue_damage(events);
    }
    static void queue_laser_spawns(LevelSim& simulation,
                                   lasers::LaserSpawnRequests::ConstView requests) {
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
