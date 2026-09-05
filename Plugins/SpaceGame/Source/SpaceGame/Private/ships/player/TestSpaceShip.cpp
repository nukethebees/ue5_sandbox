#include "SpaceGame/ships/player/TestSpaceShip.h"

#include <SpaceGame/entities/TestTeamVisualData.h>
#include <SpaceGame/support/logging/SandboxLogCategories.h>
#include <SpaceGame/support/mesh.h>

#include <SandboxCoreEngine/uobject_utils.h>

#include <Camera/CameraComponent.h>
#include <Components/SceneComponent.h>
#include <Components/StaticMeshComponent.h>
#include <DrawDebugHelpers.h>
#include <Engine/StaticMesh.h>
#include <Engine/World.h>
#include <NiagaraComponent.h>

#include "SandboxGameShared/utilities/macros/null_checks.hpp"

ATestSpaceShip::ATestSpaceShip()
    : camera(CreateDefaultSubobject<UCameraComponent>(TEXT("camera")))
    , ship_mesh(CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ship_mesh")))
    , boost_pulse{CreateDefaultSubobject<UNiagaraComponent>(TEXT("boost_effect"))}
    , boost_engine_effect{CreateDefaultSubobject<UNiagaraComponent>(TEXT("boost_engine_effect"))} {
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("root"));

    camera->SetupAttachment(RootComponent);
    ship_mesh->SetupAttachment(RootComponent);

    boost_pulse->SetupAttachment(RootComponent);
    boost_pulse->bAutoActivate = false;
    boost_pulse->SetAutoDestroy(false);

    boost_engine_effect->SetupAttachment(ship_mesh);
    boost_engine_effect->bAutoActivate = false;
    boost_engine_effect->SetAutoDestroy(false);

    PrimaryActorTick.bCanEverTick = false;
    PrimaryActorTick.bStartWithTickEnabled = false;
    configure_ship_mesh();
}

auto ATestSpaceShip::make_spawn_data() const -> ml::test_space_ship::FPlayerSpawnData {
    ml::test_space_ship::FPlayerSpawnData result;
    result.team = team;
    result.transform = GetActorTransform();
    result.visual_transform = ship_mesh ? ship_mesh->GetRelativeTransform() : FTransform::Identity;
    result.flight_mode = flight_mode;
    result.control_mode = control_mode;
    result.laser_mode = laser_mode;
    result.laser_fire_rate = laser_fire_rate;
    result.health = health;

    if (actor_config) {
        result.config = make_simulation_config(*actor_config);
    }
    if (ship_mesh) {
        result.left_socket = ship_mesh->GetSocketTransform(Sockets::left, RTS_Component);
        result.right_socket = ship_mesh->GetSocketTransform(Sockets::right, RTS_Component);
        result.middle_socket = ship_mesh->GetSocketTransform(Sockets::middle, RTS_Component);
        result.collision_radius = ml::get_mesh_sphere_bounds(*ship_mesh);
    }
    return result;
}

void ATestSpaceShip::bind_simulation(ml::test_space_ship::Simulation& new_simulation) {
    bound_simulation = &new_simulation;
}

void ATestSpaceShip::unbind_simulation() {
    bound_simulation = nullptr;
}

auto ATestSpaceShip::simulation() -> ml::test_space_ship::Simulation& {
    checkf(bound_simulation, TEXT("Player input requires a bound level simulation"));
    return *bound_simulation;
}

auto ATestSpaceShip::simulation() const -> ml::test_space_ship::Simulation const& {
    return const_cast<ATestSpaceShip*>(this)->simulation();
}

void ATestSpaceShip::begin_play_presentation() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestSpaceShip::begin_play_presentation);
    if (!actor_config) {
        UE_LOG(LogSandbox, Fatal, TEXT("ATestSpaceShip actor_config is nullptr."));
    }

    ml::fatal_if_uobject_ptrs_invalid({
        {
            SANDBOX_NAMED_UOBJECT_PTR(ship_mesh),
            SANDBOX_NAMED_UOBJECT_PTR(boost_pulse),
            SANDBOX_NAMED_UOBJECT_PTR(boost_engine_effect),
        },
        {
            SANDBOX_NAMED_UOBJECT_PTR(actor_config->team_visual_data),
        },
    });

    RETURN_IF_FALSE(ship_mesh->DoesSocketExist(Sockets::left));
    RETURN_IF_FALSE(ship_mesh->DoesSocketExist(Sockets::right));
    RETURN_IF_FALSE(ship_mesh->DoesSocketExist(Sockets::middle));

    configure_boost_pulse();
    configure_boost_engine_effect();
    presented_boost_brake_state = EBoostBrakeState::None;
}

void ATestSpaceShip::update_visual_data(float const dt) {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestSpaceShip::update_visual_data);
    log_config.tick(dt);

    auto const& state{simulation()};
    SetActorTransform(state.transform, false, nullptr, ETeleportType::TeleportPhysics);
    ship_mesh->SetRelativeTransform(state.visual_transform);
    boost_engine_effect->SetVectorParameter(TEXT("ship_velocity"), state.velocity);

    if (presented_boost_brake_state != state.boost_brake_state) {
        if (state.boost_brake_state == EBoostBrakeState::Boost) {
            boost_pulse->Activate();
            boost_engine_effect->Activate();
        } else {
            boost_engine_effect->Deactivate();
        }
        presented_boost_brake_state = state.boost_brake_state;
    }
    log_config.on_tick_end();
}

void ATestSpaceShip::commit_visual_data() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestSpaceShip::commit_visual_data);
    draw_debug_shapes();
}

void ATestSpaceShip::handle_simulation_death() {
    auto player_ship_died{MoveTemp(on_player_ship_died)};
    player_ship_died.ExecuteIfBound();
    Destroy();
}

void ATestSpaceShip::configure_boost_pulse() {
    boost_pulse->SetColorParameter(TEXT("colour"), actor_config->engine_colour);
    boost_pulse->SetFloatParameter(TEXT("ring_colour_intensity"),
                                   actor_config->boost_effect_colour_intensity);
    boost_pulse->SetFloatParameter(TEXT("sparks_colour_intensity"),
                                   actor_config->boost_effect_colour_intensity);
}

void ATestSpaceShip::configure_boost_engine_effect() {
    boost_engine_effect->SetColorParameter(TEXT("colour"), actor_config->engine_colour);
    boost_engine_effect->SetFloatParameter(TEXT("sparks_colour_intensity"),
                                           actor_config->boost_effect_colour_intensity);
}

void ATestSpaceShip::configure_ship_mesh() {
    ship_mesh->SetCanEverAffectNavigation(false);
    ship_mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    ship_mesh->SetGenerateOverlapEvents(false);
}

void ATestSpaceShip::draw_debug_shapes() {
#if WITH_EDITORONLY_DATA
    auto const& state{simulation()};
    if (debug_forward_socket_direction) {
        auto const middle{state.get_middle_socket()};
        auto const start{middle.GetLocation()};
        constexpr float length{5000.f};
        auto const end{start + middle.GetUnitAxis(EAxis::X) * length};
        DrawDebugLine(GetWorld(), start, end, FColor::Green, false, 0.f, 0, 10.f);
    }

    if (debug_forward_direction) {
        auto const start{state.transform.GetLocation()};
        constexpr float length{5000.f};
        auto const end{start + state.transform.GetUnitAxis(EAxis::X) * length};
        DrawDebugLine(GetWorld(), start, end, FColor::Green, false, 0.f, 0, 10.f);
    }

    if (debug_lock_on && state.laser_firing_mode == ELaserFiringState::lock_on_searching) {
        auto const middle{state.get_middle_socket()};
        auto const start{middle.GetLocation()};
        auto const end{start + middle.GetUnitAxis(EAxis::X) * actor_config->laser_lock_on_distance};
        DrawDebugLine(GetWorld(), start, end, FColor::Green, false, 0.f, 0, 10.f);
        DrawDebugSphere(GetWorld(), end, debug_lock_on_sphere_radius, 8, FColor::Orange);
    }
#endif
}

auto ATestSpaceShip::get_entity_handle() const noexcept -> FRegistryEntityHandle {
    return bound_simulation ? bound_simulation->registry_handle : FRegistryEntityHandle{};
}

auto ATestSpaceShip::get_unique_id() const -> TestEntityUniqueId {
    return bound_simulation ? bound_simulation->unique_entity_id : TestEntityUniqueId{};
}

auto ATestSpaceShip::get_entity_registry_handle() const -> FRegistryEntityHandle {
    return get_entity_handle();
}

auto ATestSpaceShip::get_team() const noexcept -> ETestTeam {
    return bound_simulation ? bound_simulation->team : team;
}

void ATestSpaceShip::set_team(ETestTeam const new_team) noexcept {
    team = new_team;
    if (bound_simulation) {
        bound_simulation->team = new_team;
    }
}

void ATestSpaceShip::set_actor_config(FPlayerShipConfig const* const new_config) noexcept {
    actor_config = new_config;
    if (new_config && bound_simulation) {
        bound_simulation->set_config(make_simulation_config(*new_config));
    }
}

auto ATestSpaceShip::get_kills() const -> int32 {
    return simulation().get_kills();
}

void ATestSpaceShip::set_move_input(FVector2D const input) {
    simulation().set_move_input(input);
}

void ATestSpaceShip::set_lateral_move_input(float const input) {
    simulation().set_lateral_move_input(input);
}

void ATestSpaceShip::set_vertical_move_input(float const input) {
    simulation().set_vertical_move_input(input);
}

void ATestSpaceShip::set_ship_2d_control(FVector2D const input) {
    simulation().set_ship_2d_control(input);
}

void ATestSpaceShip::set_ship_1d_control_x(float const input) {
    simulation().set_ship_1d_control_x(input);
}

void ATestSpaceShip::set_ship_1d_control_y(float const input) {
    simulation().set_ship_1d_control_y(input);
}

void ATestSpaceShip::select_next_control_mode() {
    simulation().select_next_control_mode();
}

void ATestSpaceShip::select_previous_control_mode() {
    simulation().select_previous_control_mode();
}

void ATestSpaceShip::start_sampling() {
    simulation().start_sampling();
}

void ATestSpaceShip::stop_sampling() {
    simulation().stop_sampling();
}

void ATestSpaceShip::turn(FVector2D const direction) {
#if WITH_EDITOR
    if (log_config.can_log(EActorLogVerbosity::VeryVerbose)) {
        UE_LOG(LogSandbox, Verbose, TEXT("Turning: %s"), *direction.ToString());
    }
#endif
    simulation().turn(direction);
}

void ATestSpaceShip::start_boost() {
    simulation().start_boost();
}

void ATestSpaceShip::stop_boost() {
    simulation().stop_boost();
}

void ATestSpaceShip::start_brake() {
    simulation().start_brake();
}

void ATestSpaceShip::stop_brake() {
    simulation().stop_brake();
}

auto ATestSpaceShip::get_velocity() const -> FVector {
    return bound_simulation ? bound_simulation->velocity : FVector::ZeroVector;
}

auto ATestSpaceShip::GetVelocity() const -> FVector {
    return get_velocity();
}

auto ATestSpaceShip::get_speed() const -> float {
    return simulation().get_speed();
}

void ATestSpaceShip::roll(float const direction) {
    simulation().roll(direction);
}

auto ATestSpaceShip::get_target_speed() const -> float {
    return simulation().target_speed;
}

auto ATestSpaceShip::get_move_input() const -> FVector2D {
    return simulation().planar_movement_direction;
}

auto ATestSpaceShip::get_control_mode() const -> ETestSpaceShipControlMode {
    return simulation().control_mode;
}

auto ATestSpaceShip::get_flight_mode() const -> ETestSpaceShipFlightMode {
    return simulation().flight_mode;
}

void ATestSpaceShip::set_flight_mode(ETestSpaceShipFlightMode const new_flight_mode) noexcept {
    flight_mode = new_flight_mode;
    if (bound_simulation) {
        bound_simulation->set_flight_mode(new_flight_mode);
    }
}

auto ATestSpaceShip::get_target_local_planar_velocity_scale() const -> FVector2D {
    return simulation().target_local_planar_velocity_scale;
}

auto ATestSpaceShip::get_target_local_planar_velocity() const -> FVector {
    return simulation().target_local_planar_velocity;
}

auto ATestSpaceShip::get_turn_input() const -> FVector2D {
    return simulation().rotation_input;
}

auto ATestSpaceShip::energy_is_full() const -> bool {
    return simulation().energy_is_full();
}

auto ATestSpaceShip::get_energy() const -> float {
    return simulation().get_energy();
}

auto ATestSpaceShip::get_lock_on_target() const -> FRegistryEntityHandle {
    return simulation().lock_on_target;
}

void ATestSpaceShip::start_fire_laser() {
    simulation().start_fire_laser();
}

void ATestSpaceShip::stop_fire_laser() {
    simulation().stop_fire_laser();
}

void ATestSpaceShip::upgrade_laser() {
    simulation().upgrade_laser();
}

auto ATestSpaceShip::get_laser_fire_rate() const noexcept -> ETestShipFireRate {
    return simulation().laser_fire_rate;
}

auto ATestSpaceShip::get_laser_firing_mode() const noexcept -> ELaserFiringState {
    return simulation().laser_firing_mode;
}

void ATestSpaceShip::select_next_laser_fire_rate() noexcept {
    simulation().select_next_laser_fire_rate();
}

void ATestSpaceShip::select_previous_laser_fire_rate() noexcept {
    simulation().select_previous_laser_fire_rate();
}

void ATestSpaceShip::set_laser_fire_rate(ETestShipFireRate const value) noexcept {
    laser_fire_rate = value;
    if (bound_simulation) {
        bound_simulation->set_laser_fire_rate(value);
    }
}

void ATestSpaceShip::add_health(int32 const added_health) {
    simulation().add_health(added_health);
    if (simulation().consume_death_notification()) {
        handle_simulation_death();
    }
}

auto ATestSpaceShip::get_health_info() const -> FShipHealth {
    return bound_simulation ? bound_simulation->health : health;
}

auto ATestSpaceShip::is_alive() const noexcept -> bool {
    return get_health_info().is_alive();
}

auto ATestSpaceShip::get_collision_mesh() const -> UStaticMesh const* {
    return ship_mesh ? ship_mesh->GetStaticMesh() : nullptr;
}

auto ATestSpaceShip::get_ship_forward_vector() const -> FVector {
    return simulation().get_middle_socket().GetLocation();
}

auto ATestSpaceShip::get_middle_socket() const -> FTransform {
    return simulation().get_middle_socket();
}

#if WITH_EDITOR
auto ATestSpaceShip::get_speed_samples() const noexcept -> TConstArrayView<FVector2d> {
    return simulation().speed_samples;
}

auto ATestSpaceShip::get_speed_sample_index() const noexcept -> int32 {
    return simulation().speed_sample_index;
}
#endif
