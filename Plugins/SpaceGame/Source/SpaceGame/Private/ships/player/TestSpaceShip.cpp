#include "SpaceGame/ships/player/TestSpaceShip.h"
#include <SpaceGame/simulation/SimulationConfigConversion.h>
#include <SpaceGameSimulation/simulation/NativeTransformTypes.h>

#include <SpaceGamePresentation/entities/TestTeamVisualData.h>
#include <SpaceGameSimulation/support/logging/SandboxLogCategories.h>

#include <SandboxCoreEngine/uobject_utils.h>

#include <Camera/CameraComponent.h>
#include <Components/SceneComponent.h>
#include <Components/StaticMeshComponent.h>
#include <DrawDebugHelpers.h>
#include <Engine/StaticMesh.h>
#include <Engine/World.h>
#include <NiagaraComponent.h>

#include "SandboxGameShared/utilities/macros/null_checks.hpp"

/* **************************************** */
// Lifecycle and simulation binding
/* **************************************** */
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

auto ATestSpaceShip::make_spawn_data() const -> ::ioj::sim::player::PlayerSpawnData {
    ::ioj::sim::player::PlayerSpawnData result;
    result.team = ml::to_native(team);
    result.transform = ml::to_native(GetActorTransform());
    result.body_transform =
        ml::to_native(ship_mesh ? ship_mesh->GetRelativeTransform() : FTransform::Identity);
    result.flight_mode = ml::to_native(flight_mode);
    result.control_mode = ml::to_native(control_mode);
    result.laser_mode = ml::to_native(laser_mode);
    result.laser_fire_rate = ml::to_native(laser_fire_rate);
    result.health = ml::to_native(health);

    if (actor_config) {
        result.config = make_simulation_config(*actor_config);
    }
    if (ship_mesh) {
        result.left_socket =
            ml::to_native(ship_mesh->GetSocketTransform(Sockets::left, RTS_Component));
        result.right_socket =
            ml::to_native(ship_mesh->GetSocketTransform(Sockets::right, RTS_Component));
        result.middle_socket =
            ml::to_native(ship_mesh->GetSocketTransform(Sockets::middle, RTS_Component));
    }
    return result;
}

void ATestSpaceShip::bind_simulation(::ioj::sim::player::CommandInterface& new_commands,
                                     ::ioj::sim::player::Sim const& new_simulation) {
    bound_commands_ = &new_commands;
    bound_simulation = &new_simulation;
#if WITH_EDITOR
    new_commands.set_speed_sampling_enabled(true);
#endif
}

void ATestSpaceShip::unbind_simulation() {
    bound_commands_ = nullptr;
    bound_simulation = nullptr;
}

auto ATestSpaceShip::commands() -> ::ioj::sim::player::CommandInterface& {
    checkf(bound_commands_, TEXT("Player input requires bound simulation commands"));
    return *bound_commands_;
}

auto ATestSpaceShip::simulation() const -> ::ioj::sim::player::Sim const& {
    checkf(bound_simulation, TEXT("Player status requires a bound level simulation"));
    return *bound_simulation;
}

void ATestSpaceShip::handle_simulation_death() {
    auto player_ship_died{MoveTemp(on_player_ship_died)};
    player_ship_died.ExecuteIfBound();
    Destroy();
}

void ATestSpaceShip::configure_ship_mesh() {
    ship_mesh->SetCanEverAffectNavigation(false);
    ship_mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    ship_mesh->SetGenerateOverlapEvents(false);
}

/* **************************************** */
// Entity identity and configuration
/* **************************************** */
auto ATestSpaceShip::get_unique_id() const noexcept -> ::ioj::sim::EntityUniqueId {
    return bound_simulation ? bound_simulation->unique_entity_id : ::ioj::sim::EntityUniqueId{};
}

auto ATestSpaceShip::get_team() const noexcept -> ETestTeam {
    return bound_simulation ? ml::to_unreal(bound_simulation->team) : team;
}

void ATestSpaceShip::set_team(ETestTeam const new_team) noexcept {
    team = new_team;
    if (bound_simulation) {
        commands().set_team(ml::to_native(new_team));
    }
}

void ATestSpaceShip::set_actor_config(FPlayerShipConfig const* const new_config) noexcept {
    actor_config = new_config;
    if (new_config && bound_simulation) {
        commands().set_config(make_simulation_config(*new_config));
    }
}

auto ATestSpaceShip::get_kills() const -> int32 {
    return simulation().get_kills();
}

/* **************************************** */
// Flight controls
/* **************************************** */
void ATestSpaceShip::set_move_input(FVector2D const input) {
    commands().set_move_input(ml::to_native(input));
}

void ATestSpaceShip::set_lateral_move_input(float const input) {
    commands().set_lateral_move_input(input);
}

void ATestSpaceShip::set_vertical_move_input(float const input) {
    commands().set_vertical_move_input(input);
}

void ATestSpaceShip::set_ship_2d_control(FVector2D const input) {
    commands().set_ship_2d_control(ml::to_native(input));
}

void ATestSpaceShip::set_ship_1d_control_x(float const input) {
    commands().set_ship_1d_control_x(input);
}

void ATestSpaceShip::set_ship_1d_control_y(float const input) {
    commands().set_ship_1d_control_y(input);
}

void ATestSpaceShip::select_next_control_mode() {
    commands().select_next_control_mode();
}

void ATestSpaceShip::select_previous_control_mode() {
    commands().select_previous_control_mode();
}

void ATestSpaceShip::start_sampling() {
    commands().start_sampling();
}

void ATestSpaceShip::stop_sampling() {
    commands().stop_sampling();
}

void ATestSpaceShip::adjust_desired_forward_velocity(float const direction) {
    commands().adjust_desired_forward_velocity(direction);
}

void ATestSpaceShip::turn(FVector2D const direction) {
#if WITH_EDITOR
    if (log_config.can_log(EActorLogVerbosity::VeryVerbose)) {
        UE_LOG(LogSandbox, Verbose, TEXT("Turning: %s"), *direction.ToString());
    }
#endif
    commands().turn(ml::to_native(direction));
}

void ATestSpaceShip::start_boost() {
    commands().start_boost();
}

void ATestSpaceShip::stop_boost() {
    commands().stop_boost();
}

void ATestSpaceShip::start_brake() {
    commands().start_brake();
}

void ATestSpaceShip::stop_brake() {
    commands().stop_brake();
}

auto ATestSpaceShip::get_velocity() const -> FVector {
    return bound_simulation ? ml::to_unreal(bound_simulation->get_movement_state().velocity)
                            : FVector::ZeroVector;
}

auto ATestSpaceShip::GetVelocity() const -> FVector {
    return get_velocity();
}

auto ATestSpaceShip::get_speed() const -> float {
    return simulation().get_speed();
}

void ATestSpaceShip::roll(float const direction) {
    commands().roll(direction);
}

auto ATestSpaceShip::get_target_speed() const -> float {
    return simulation().get_movement_state().target_speed;
}

auto ATestSpaceShip::get_move_input() const -> FVector2D {
    return ml::to_unreal(simulation().planar_movement_direction);
}

auto ATestSpaceShip::get_control_mode() const -> ETestSpaceShipControlMode {
    return ml::to_unreal(simulation().control_mode);
}

auto ATestSpaceShip::get_flight_mode() const -> ETestSpaceShipFlightMode {
    return ml::to_unreal(simulation().flight_mode);
}

void ATestSpaceShip::set_flight_mode(ETestSpaceShipFlightMode const new_flight_mode) noexcept {
    flight_mode = new_flight_mode;
    if (bound_simulation) {
        commands().set_flight_mode(ml::to_native(new_flight_mode));
    }
}

auto ATestSpaceShip::get_target_local_planar_velocity_scale() const -> FVector2D {
    return ml::to_unreal(simulation().target_local_planar_velocity_scale);
}

auto ATestSpaceShip::is_sampling() const -> bool {
    return simulation().sampling;
}

auto ATestSpaceShip::get_target_local_planar_velocity() const -> FVector {
    return ml::to_unreal(simulation().target_local_planar_velocity);
}

auto ATestSpaceShip::get_turn_input() const -> FVector2D {
    return ml::to_unreal(simulation().rotation_input);
}

/* **************************************** */
// Energy and weapons
/* **************************************** */
auto ATestSpaceShip::energy_is_full() const -> bool {
    return simulation().energy_is_full();
}

auto ATestSpaceShip::get_energy() const -> float {
    return simulation().get_energy();
}

auto ATestSpaceShip::get_lock_on_target() const -> ::ioj::sim::EntityUniqueId {
    return simulation().lock_on_target;
}

void ATestSpaceShip::start_fire_laser() {
    commands().start_fire_laser();
}

void ATestSpaceShip::stop_fire_laser() {
    commands().stop_fire_laser();
}

void ATestSpaceShip::upgrade_laser() {
    commands().upgrade_laser();
}

auto ATestSpaceShip::get_laser_fire_rate() const noexcept -> ETestShipFireRate {
    return ml::to_unreal(simulation().laser_fire_rate);
}

auto ATestSpaceShip::get_laser_firing_mode() const noexcept -> ELaserFiringState {
    return ml::to_unreal(simulation().laser_firing_mode);
}

void ATestSpaceShip::select_next_laser_fire_rate() noexcept {
    commands().select_next_laser_fire_rate();
}

void ATestSpaceShip::select_previous_laser_fire_rate() noexcept {
    commands().select_previous_laser_fire_rate();
}

void ATestSpaceShip::set_laser_fire_rate(ETestShipFireRate const value) noexcept {
    laser_fire_rate = value;
    if (bound_simulation) {
        commands().set_laser_fire_rate(ml::to_native(value));
    }
}

/* **************************************** */
// Health and collision
/* **************************************** */
void ATestSpaceShip::add_health(int32 const added_health) {
    commands().add_health(added_health);
    if (commands().consume_death_notification()) {
        handle_simulation_death();
    }
}

auto ATestSpaceShip::get_health_info() const -> FShipHealth {
    return bound_simulation ? ml::to_unreal(bound_simulation->health) : health;
}

auto ATestSpaceShip::is_alive() const noexcept -> bool {
    return get_health_info().is_alive();
}

auto ATestSpaceShip::get_collision_mesh() const -> UStaticMesh const* {
    return ship_mesh ? ship_mesh->GetStaticMesh() : nullptr;
}

auto ATestSpaceShip::get_middle_socket() const -> FTransform {
    return ml::to_unreal(simulation().get_middle_socket());
}

#if WITH_EDITOR
/* **************************************** */
// Diagnostics
/* **************************************** */
auto ATestSpaceShip::get_speed_samples() const noexcept -> std::span<ml::Vector2d const> {
    return simulation().speed_samples;
}

auto ATestSpaceShip::get_speed_sample_index() const noexcept -> int32 {
    return simulation().speed_sample_index;
}
#endif

/* **************************************** */
// Presentation
/* **************************************** */
auto ATestSpaceShip::get_presentation_resources() const -> FPlayerPresentationResources {
    FPlayerPresentationResources resources{
        RootComponent, ship_mesh, boost_pulse, boost_engine_effect};
#if WITH_EDITORONLY_DATA
    resources.debug_forward_socket_direction = debug_forward_socket_direction;
    resources.debug_forward_direction = debug_forward_direction;
    resources.debug_lock_on = debug_lock_on;
    resources.debug_lock_on_sphere_radius = debug_lock_on_sphere_radius;
#endif
    return resources;
}
