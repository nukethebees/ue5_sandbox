#include <ioj/sim/testing/sim_clock_test_access.h>
#pragma once

#include <ioj/column_math.h>
#include <ioj/frame_memory_resource.h>
#include <ioj/sim/levels/level_sim.h>
#include <ioj/vector_operations.h>

#include <algorithm>
#include <utility>

namespace ioj::sim {

struct LevelSimTestAccess {
    static void update_entity_lookup_tables(LevelSim& simulation) {
        auto const previous{simulation.clock_.phase()};
        SimClockTestAccess::set_phase(simulation.clock_, SimulationPhase::Preparation);
        simulation.capital_ships_simulation_.update_entity_lookup_table();
        simulation.fighters_simulation_.update_entity_lookup_table();
        SimClockTestAccess::set_phase(simulation.clock_, previous);
    }

    static auto player_simulation(LevelSim& simulation) -> player::Sim& {
        return simulation.player_ship_simulation_.value();
    }
    static void queue_fighter_spawns(LevelSim& simulation, FighterSpawnQueue::ConstView spawns) {
        fighters::CommandInterface{simulation.fighters_simulation_}.queue_spawns(spawns);
    }
    static void reassign_pending_fighter_spawns(LevelSim& simulation,
                                                EntityId parent,
                                                EntityId replacement) {
        fighters::CommandInterface{simulation.fighters_simulation_}.reassign_pending_spawns(
            parent, replacement);
    }
    static void refresh_fighter_membership(LevelSim& simulation,
                                           FrameMemoryResource* const scratch_resource) {
        simulation.capital_ships_simulation_.refresh_fighter_ids(scratch_resource);
    }
    static void set_fighter_parent(LevelSim& simulation, EntityId fighter, EntityId parent) {
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
                                    FrameMemoryResource* const scratch_resource) {
        auto const previous_phase{simulation.clock_.phase()};
        SimClockTestAccess::set_phase(simulation.clock_, SimulationPhase::Resolution);
        simulation.combat_events_.prepare(simulation.entity_tables_.lookups, scratch_resource);

        capital_ships::ResolutionInputs const capital_inputs{
            simulation.combat_events_, simulation.clock_, *scratch_resource};
        fighters::ResolutionInputs const fighter_inputs{simulation.combat_events_,
                                                        *scratch_resource};
        simulation.capital_ships_simulation_.resolve_damage_events(capital_inputs);
        simulation.fighters_simulation_.resolve_damage_events(fighter_inputs);
        simulation.capital_ships_simulation_.resolve_fighters_of_dying_capitals();
        simulation.capital_ships_simulation_.publish_deaths(capital_inputs);
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
                                   EntityId fighter,
                                   EntityId target,
                                   std::int8_t awareness_countdown = 0) {
        auto& fighters{simulation.fighters_simulation_};
        auto const index{simulation.fighters_simulation_.find_index(fighter)};
        fighters.set_target_id(fighter, target);
        auto const data{fighters.entity_buffers.current().get_view()};
        data.awareness_scan_countdowns()[index] = awareness_countdown;
        data.attack_reposition_countdowns()[index] = 0;
        data.navigation_update_countdowns_remaining_ticks()[index] = 1;
    }
    static void think_fighters(LevelSim& simulation, FrameMemoryResource* const scratch_resource) {
        auto const previous_phase{simulation.clock_.phase()};
        SimClockTestAccess::set_phase(simulation.clock_, SimulationPhase::Thinking);
        fighters::ThinkingInputs const inputs{
            static_cast<float>(simulation.clock_.get_tick_period()),
            simulation.query_manager_,
            *scratch_resource};
        simulation.fighters_simulation_.think(inputs);
        SimClockTestAccess::set_phase(simulation.clock_, previous_phase);
    }
    static void set_fighter_kinematics(LevelSim& simulation,
                                       EntityId fighter,
                                       Vector3f location,
                                       Vector3f velocity) {
        auto const index{simulation.fighters_simulation_.find_index(fighter)};
        auto const data{simulation.fighters_simulation_.entity_buffers.current().get_view()};
        set_vector(data.view_locations(), index, location);
        set_vector(data.view_velocities(), index, velocity);
        simulation.refresh_spatial_index();
    }
    static void refresh_fighter_targets_and_plan(LevelSim& simulation,
                                                 FrameMemoryResource* const scratch_resource) {
        auto const previous_phase{simulation.clock_.phase()};
        SimClockTestAccess::set_phase(simulation.clock_, SimulationPhase::Thinking);
        auto& fighters{simulation.fighters_simulation_};
        fighters::ThinkingInputs const inputs{
            static_cast<float>(simulation.clock_.get_tick_period()),
            simulation.query_manager_,
            *scratch_resource};
        fighters.refresh_target_data(inputs);
        fighters.plan_movement(inputs);
        SimClockTestAccess::set_phase(simulation.clock_, previous_phase);
    }
    static void queue_fighter_orders(LevelSim& simulation, FighterOrderQueue const& orders) {
        fighters::CommandInterface{simulation.fighters_simulation_}.queue_orders(orders);
    }
    static void queue_direct_damage_events(LevelSim& simulation,
                                           DirectDamageEvents::ConstView events) {
        simulation.combat_events_.queue_damage(events);
    }
    static void queue_laser_spawns(LevelSim& simulation,
                                   lasers::LaserSpawnRequests::ConstView requests) {
        auto const destination{simulation.lasers_simulation_.append_spawn_requests(requests.num())};
        copy_vectors(destination.view_locations(), requests.view_locations());
        auto const rotations{destination.view_rotations().resolve()};
        auto const source_rotations{requests.view_rotations().resolve()};
        std::ranges::copy(source_rotations.pitches(), rotations.pitches().begin());
        std::ranges::copy(source_rotations.yaws(), rotations.yaws().begin());
        std::ranges::copy(source_rotations.rolls(), rotations.rolls().begin());
        copy_vectors(destination.view_base_velocities(), requests.view_base_velocities());
        std::ranges::copy(requests.damages(), destination.damages().begin());
        std::ranges::copy(requests.speeds(), destination.speeds().begin());
        std::ranges::copy(requests.max_distances(), destination.max_distances().begin());
        std::ranges::copy(requests.instigator_ids(), destination.instigator_ids().begin());
        std::ranges::copy(requests.sources(), destination.sources().begin());
    }
    static auto complete_mission(LevelSim& simulation) -> bool {
        return simulation.mission_manager_.complete_mission();
    }
};

} // namespace ioj::sim
