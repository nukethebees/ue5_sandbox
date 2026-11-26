#include "SpaceGame/ships/player/TestSpaceShip.h"

#include "SpaceGame/UnrealMessages.h"
#include <SpaceGamePresentation/entities/TestTeamVisualData.h>
#include <SpaceGameSimulation/support/logging/SandboxLogCategories.h>

#include <SandboxCoreEngine/TransformConversion.h>
#include <SandboxCoreEngine/uobject_utils.h>
#include <SandboxShaders/SpaceDust/SpaceDustComponent.h>

#include <Camera/CameraComponent.h>
#include <Components/SceneComponent.h>
#include <Components/StaticMeshComponent.h>
#include <DrawDebugHelpers.h>
#include <Engine/StaticMesh.h>
#include <Engine/World.h>
#include <EngineUtils.h>
#include <HAL/IConsoleManager.h>
#include <NiagaraComponent.h>

#include <utility>

namespace ioj {
namespace {
#if !UE_BUILD_SHIPPING
void apply_space_dust_preset_command(TArray<FString> const& arguments, UWorld* const world) {
    if (!IsValid(world) || arguments.Num() != 1) {
        UE_LOG(LogSandbox, Warning, TEXT("Usage: space_dust.preset <off|default|strong>."));
        return;
    }

    for (TActorIterator<ATestSpaceShip> player_ship{world}; player_ship; ++player_ship) {
        player_ship->apply_space_dust_debug_preset(arguments[0]);
    }
}

FAutoConsoleCommandWithWorldAndArgs space_dust_preset_console_command{
    TEXT("space_dust.preset"),
    TEXT("Applies the off, default, or strong space-dust tuning preset to player ships."),
    FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&apply_space_dust_preset_command)};
#endif
}

/* **************************************** */
// Lifecycle and simulation binding
/* **************************************** */
ATestSpaceShip::ATestSpaceShip()
    : camera(CreateDefaultSubobject<UCameraComponent>(TEXT("camera")))
    , space_dust(CreateDefaultSubobject<USpaceDustComponent>(TEXT("space_dust")))
    , ship_mesh(CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ship_mesh")))
    , boost_pulse{CreateDefaultSubobject<UNiagaraComponent>(TEXT("boost_effect"))}
    , boost_engine_effect{CreateDefaultSubobject<UNiagaraComponent>(TEXT("boost_engine_effect"))} {
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("root"));

    camera->SetupAttachment(RootComponent);
    space_dust->SetupAttachment(camera);
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

void ATestSpaceShip::clear_player_frame() {
    reset_level_input();
    player_frame_.reset();
}

auto ATestSpaceShip::input_events() -> sim::player::PlayerInputEvents& {
    checkf(player_frame_, TEXT("Player input requires bound simulation commands"));
    return ship_input();
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
auto ATestSpaceShip::get_unique_id() const noexcept -> sim::EntityId {
    return player_frame_ ? player_frame_->id : sim::EntityId{};
}

auto ATestSpaceShip::get_team() const noexcept -> ETestTeam {
    return player_frame_ ? to_unreal(player_frame_->team) : team;
}

void ATestSpaceShip::set_team(ETestTeam const new_team) noexcept {
    checkf(!player_frame_, TEXT("Set the authored team before binding the ship"));
    team = new_team;
}

/* **************************************** */
// Flight controls
/* **************************************** */

auto ATestSpaceShip::GetVelocity() const -> FVector {
    return player_frame_ ? to_unreal(player_frame_->flight.ship_velocity) : FVector::ZeroVector;
}

/* **************************************** */
// Weapons
/* **************************************** */

void ATestSpaceShip::upgrade_laser() {
    input_events().upgrade_laser();
}

auto ATestSpaceShip::get_laser_trigger_mode() const noexcept -> ETestShipTriggerMode {
    return to_unreal(player_frame_->status.trigger_mode);
}

auto ATestSpaceShip::get_laser_firing_mode() const noexcept -> sim::LaserFiringState {
    return player_frame_->firing_mode;
}

void ATestSpaceShip::select_next_laser_trigger_mode() noexcept {
    input_events().select_next_laser_trigger_mode();
}

void ATestSpaceShip::select_previous_laser_trigger_mode() noexcept {
    input_events().select_previous_laser_trigger_mode();
}

void ATestSpaceShip::set_laser_trigger_mode(ETestShipTriggerMode const value) noexcept {
    if (player_frame_) {
        input_events().set_laser_trigger_mode(to_native(value));
    }
}

/* **************************************** */
// Health and collision
/* **************************************** */
auto ATestSpaceShip::get_health_info() const -> FShipHealth {
    if (!player_frame_) {
        return FShipHealth{initial_health_};
    }

    auto const native_health{player_frame_->status.health};
    return {native_health.health.value(), native_health.max_health.value()};
}

auto ATestSpaceShip::is_alive() const noexcept -> bool {
    return get_health_info().is_alive();
}

auto ATestSpaceShip::get_collision_mesh() const -> UStaticMesh const* {
    return ship_mesh ? ship_mesh->GetStaticMesh() : nullptr;
}

auto ATestSpaceShip::get_middle_socket() const -> FTransform {
    return to_unreal(player_frame_->middle_socket);
}

/* **************************************** */
// Presentation
/* **************************************** */
auto ATestSpaceShip::get_presentation_resources() const -> FPlayerPresentationResources {
    FPlayerPresentationResources resources{
        RootComponent, ship_mesh, boost_pulse, boost_engine_effect, space_dust};
#if WITH_EDITORONLY_DATA
    resources.debug_forward_socket_direction = debug_forward_socket_direction;
    resources.debug_forward_direction = debug_forward_direction;
#endif
    return resources;
}

#if !UE_BUILD_SHIPPING
void ATestSpaceShip::apply_space_dust_debug_preset(FStringView const preset) {
    if (!IsValid(space_dust)) {
        UE_LOG(LogSandbox, Warning, TEXT("Space dust component is unavailable."));
        return;
    }

    auto settings{space_dust->get_settings()};
    if (preset.Equals(FStringView{TEXT("off")}, ESearchCase::IgnoreCase)) {
        settings.enabled = false;
    } else if (preset.Equals(FStringView{TEXT("default")}, ESearchCase::IgnoreCase)) {
        settings = FSpaceDustSettings{};
    } else if (preset.Equals(FStringView{TEXT("strong")}, ESearchCase::IgnoreCase)) {
        settings.enabled = true;
        settings.particle_count = 192;
        settings.brightness = 0.55f;
        settings.minimum_visible_speed = 0.0f;
        settings.full_visible_speed = 1500.0f;
        settings.streak_seconds = 0.02f;
        settings.minimum_motion_pixels = 0.5f;
        settings.full_motion_pixels = 3.0f;
        settings.maximum_streak_pixels = 32.0f;
    } else {
        UE_LOG(LogSandbox,
               Warning,
               TEXT("Unknown space-dust preset '%.*s'. Use off, default, or strong."),
               preset.Len(),
               preset.GetData());
        return;
    }

    space_dust->apply_settings(settings);
}
#endif

} // namespace ioj
