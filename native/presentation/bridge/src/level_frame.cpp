#include <ioj/bridge/level_frame.h>

#include <ioj/levels/launch_options.h>
#include <ioj/levels/level_definition.h>
#include <ioj/presentation/observer_camera.h>
#include <ioj/sim/levels/level_sim.h>

#include <algorithm>
#include <cassert>
#include <utility>
#include <vector>

namespace ioj::bridge {
namespace {
auto instance_changes(sim::EntityFrameChanges const& events) -> presentation::InstanceChanges {
    auto const view{events.const_view().resolve()};
    return {view.kinds(), view.indices(), view.locations(), view.rotations(), view.teams()};
}

auto frame_events(sim::SimulationFrameEvents const& events) -> presentation::FrameEvents {
    auto const deaths{events.deaths.const_view().resolve()};
    auto const impacts{events.laser_impacts.const_view().resolve()};
    return {events.base_tick,
            events.ticks,
            events.player,
            deaths.victims(),
            deaths.locations(),
            impacts,
            instance_changes(events.capital_changes),
            instance_changes(events.turret_changes)};
}

auto player_frame(sim::player::Sim const& player, sim::LevelReadAccess const& view)
    -> presentation::PlayerFrame {
    auto const socket{player.get_middle_socket()};
    auto const& physical{player.physical_state()};
    auto const local{physical.transform.inverse_transform_vector_no_scale(physical.velocity)};
    auto const& intent{player.flight_intent()};
    auto const speed_limit{std::max(player.target_speed(), 1.f)};
    return {.id = player.unique_entity_id,
            .team = player.team,
            .transform = physical.transform,
            .body_transform = player.presentation_state().body_transform,
            .middle_socket = socket,
            .firing_mode = player.laser_firing_mode,
            .weapon_range = player.laser_effective_range(),
            .status = {player.health(view.entity_tables().health),
                       player.speed(),
                       player.target_speed(),
                       player.energy(),
                       view.entity_ledger().kills(player.unique_entity_id),
                       player.laser_trigger_mode},
            .flight = {.flight_vector_debug = {{intent.yaw, intent.pitch},
                                               {static_cast<float>(local.y() / speed_limit),
                                                static_cast<float>(local.x() / speed_limit)}},
                       .ship_velocity = physical.velocity,
                       .target_velocity = physical.transform.forward() * player.target_speed(),
                       .throttle = intent.accelerator,
                       .boost_brake_state = player.controller_state().effective_action,
                       .crosshair_origin = socket.location,
                       .crosshair_direction = socket.forward()}};
}
}

auto make_level_presentation_setup(levels::LevelDefinition const& definition,
                                   std::string source_path,
                                   levels::LaunchOptions const& options)
    -> presentation::LevelPresentationSetup {
    auto const& settings{definition.settings()};
    presentation::LevelPresentationSetup init{
        .source_path = std::move(source_path),
        .control_context = options.control_context,
        .presentation_mode = options.presentation_mode,
        .launch_mode = options.launch_mode,
        .presentation = settings.presentation,
        .hud = settings.hud,
        .starfield = settings.starfield,
        .health_maximums = {settings.capital_ships.max_health.get().value(),
                            settings.fighters.health.get().value(),
                            settings.turrets.max_health.get().value()},
        .laser_reserve = settings.laser_projectiles.n_preallocated_instances.get()};
    if (auto const& camera{definition.camera()}) {
        std::vector<Vector3d> positions;
        positions.reserve(camera->target_entity_ids.size());
        for (auto const& id : camera->target_entity_ids) {
            positions.push_back(definition.entity(id).position);
        }
        auto const transform{presentation::make_observer_camera_transform(
            positions, camera->offset_direction, camera->distance)};
        assert(transform);
        init.camera = transform->transform;
    }
    return init;
}

auto level_init(presentation::LevelPresentationSetup const& setup, sim::LevelSim const& level)
    -> presentation::LevelInit {
    auto const& spatial{level.spatial_query_manager()};
    return {setup,
            level.level_metadata().id.value,
            spatial.grid_spec(),
            spatial.entity_type_radii(),
            spatial.static_collision_bounds().resolve()};
}

auto level_frame(sim::LevelSim const& level) -> presentation::LevelFrame {
    auto const view{level.read_access()};
    auto const capitals{view.capitals().get_entities()};
    auto const fighters{view.fighters().entities()};
    auto const turrets{view.turrets().get_entities()};
    auto const spinners{view.spinners().get_entities()};
    auto const lasers{view.lasers().get_entities()};
    auto const& health{view.entity_tables().health};
    auto const bounds{level.spatial_query_manager().entity_collision_bounds()};
    using enum sim::EntityType;
    presentation::LevelFrame frame{
        .status = {.completed_tick = level.clock().get_completed_ticks(),
                   .state = level.state(),
                   .ticks_remaining = level.run_ticks_remaining()},
        .agents = {{{CapitalShip,
                     capitals.entity_ids(),
                     capitals.view_locations().resolve(),
                     {},
                     health.const_view<CapitalShip>(),
                     capitals.teams()},
                    {Fighter,
                     fighters.entity_ids(),
                     fighters.view_locations().resolve(),
                     fighters.view_velocities().resolve(),
                     health.const_view<Fighter>(),
                     fighters.teams()},
                    {Turret,
                     turrets.entity_ids(),
                     turrets.view_locations().resolve(),
                     {},
                     health.const_view<Turret>(),
                     turrets.teams()},
                    {TubeSpinner,
                     spinners.entity_ids(),
                     spinners.view_locations().resolve(),
                     {},
                     {},
                     {}}}},
        .capital_rotations = capitals.view_rotations().resolve(),
        .fighter_aim = fighters.view_aim_directions().resolve(),
        .fighter_targets = {fighters.target_ids(), fighters.view_target_locations().resolve()},
        .turret_rotations = turrets.view_rotations().resolve(),
        .turret_targets = {turrets.target_ids(), turrets.view_target_locations().resolve()},
        .spinner_yaws = spinners.yaws(),
        .lasers = {lasers.view_locations().resolve(),
                   lasers.view_rotations().resolve(),
                   lasers.active(),
                   lasers.sources(),
                   lasers.initial_lifetimes(),
                   lasers.spawn_times()},
        .entity_bounds = sim::collision::world_aabbs::Schema::with_columns(
            bounds,
            [](auto... columns) { return sim::collision::world_aabbs::ConstView{columns...}; }),
        .entity_counts = {view.entity_ledger().count_alive_per_team_and_type()},
        .events = frame_events(view.frame_events())};
    if (auto const* player{view.player()}) {
        frame.player = player_frame(*player, view);
        frame.sampled_speed.emplace(player->speed_samples, player->speed_sample_index);
    }
    return frame;
}
}
