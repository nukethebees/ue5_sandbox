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
auto vectors(auto const& view) -> presentation::vectors3f::ConstView {
    return {view.xs(), view.ys(), view.zs()};
}

auto rotations(auto const& view) -> presentation::rotators3f::ConstView {
    return {view.pitches(), view.yaws(), view.rolls()};
}

auto bounds_view(auto const& view) -> presentation::world_aabbs::ConstView {
    return {vectors(view.view_min_points()), vectors(view.view_max_points())};
}

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
            {vectors(impacts.view_locations()),
             vectors(impacts.view_emission_directions()),
             impacts.sources()},
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
                       .crosshair_direction = socket.forward()},
            .sampled_speed = {player.speed_samples, player.speed_sample_index}};
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
            bounds_view(spatial.static_collision_bounds())};
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
                     vectors(capitals.view_locations()),
                     {},
                     health.const_view<CapitalShip>(),
                     capitals.teams()},
                    {Fighter,
                     fighters.entity_ids(),
                     vectors(fighters.view_locations()),
                     vectors(fighters.view_velocities()),
                     health.const_view<Fighter>(),
                     fighters.teams()},
                    {Turret,
                     turrets.entity_ids(),
                     vectors(turrets.view_locations()),
                     {},
                     health.const_view<Turret>(),
                     turrets.teams()},
                    {TubeSpinner,
                     spinners.entity_ids(),
                     vectors(spinners.view_locations()),
                     {},
                     {},
                     {}}}},
        .capital_rotations = rotations(capitals.view_rotations()),
        .fighter_aim = vectors(fighters.view_aim_directions()),
        .fighter_targets = {fighters.target_ids(), vectors(fighters.view_target_locations())},
        .turret_rotations = rotations(turrets.view_rotations()),
        .turret_targets = {turrets.target_ids(), vectors(turrets.view_target_locations())},
        .spinner_yaws = spinners.yaws(),
        .lasers = {vectors(lasers.view_locations()),
                   rotations(lasers.view_rotations()),
                   lasers.active(),
                   lasers.sources(),
                   lasers.initial_lifetimes(),
                   lasers.spawn_times()},
        .entity_bounds = bounds_view(bounds),
        .entity_counts = view.entity_ledger().count_alive_per_team_and_type(),
        .events = frame_events(view.frame_events())};
    if (auto const* player{view.player()}) {
        frame.player = player_frame(*player, view);
    }
    return frame;
}
}
