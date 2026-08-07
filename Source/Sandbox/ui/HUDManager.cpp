#include "Sandbox/ui/HUDManager.h"

#include "Sandbox/batch_game/test_entity_registry/TestEntityRegistry.h"
#include "Sandbox/batch_game/TestBatchGameUiData.h"
#include "Sandbox/batch_game/TestMissionManager.h"
#include "Sandbox/batch_game/TestSpaceShip.h"
#include "Sandbox/batch_game/TestSpaceShipController.h"
#include "Sandbox/batch_game/TestTeamVisualData.h"
#include "Sandbox/logging/SandboxLogCategories.h"
#include "Sandbox/ui/ship_hud/ShipHudWidget.h"

#include <SandboxCore/timing.h>
#include <SandboxCoreEngine/uobject_utils.h>

#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetLayoutLibrary.h"

namespace {
void log_error_once(bool& logged, TCHAR const* context, ml::FErrorMsg const& error) {
    if (logged) {
        return;
    }

    UE_LOG(LogSandboxUI, Warning, TEXT("%s: %s."), context, *error);
    logged = true;
}
}

void FHUDManager::initialise(UShipHudWidget& new_hud_widget,
                             UTestBatchGameUiData const& ui_data,
                             ATestMissionManager const& new_mission_manager,
                             ATestEntityRegistry const& new_entity_registry,
                             SimulationClockInterface const new_simulation_clock,
                             ATestSpaceShipController& new_player_controller,
                             ATestSpaceShip* const new_player_ship,
                             float const new_near_cursor_distance,
                             float const new_far_cursor_distance) {
    deactivate();

    auto* const team_visual_data{ui_data.team_visual_data.Get()};

    if (auto error{ml::report_invalid_uobject_ptrs({
            SANDBOX_NAMED_UOBJECT_PTR(&new_hud_widget),
            SANDBOX_NAMED_UOBJECT_PTR(team_visual_data),
        })}) {
        UE_LOG(LogSandboxUI, Fatal, TEXT("FHUDManager::initialise: %s."), *error);
    }

    auto const update_frequencies{ui_data.update_frequencies.to_array()};
    auto const n_freqs{update_frequencies.Num()};

    for (int32 i{0}; i < n_freqs; ++i) {
        if (!ml::valid_periods(update_frequencies[i])) {
            UE_LOG(LogSandboxUI, Fatal, TEXT("Invalid period."));
        }
        auto const tick_period{new_simulation_clock.duration_to_tick_period(update_frequencies[i])};
        if (!ml::valid_periods(tick_period)) {
            UE_LOG(LogSandboxUI, Fatal, TEXT("Invalid period."));
        }
        update_timers.add_started(tick_period);
    }

    new_hud_widget.AddToViewport();
    hud_widget = &new_hud_widget;
    player_controller = &new_player_controller;
    mission_manager = &new_mission_manager;
    entity_registry = &new_entity_registry;
    simulation_clock = new_simulation_clock;
    near_cursor_distance = new_near_cursor_distance;
    far_cursor_distance = new_far_cursor_distance;

    new_hud_widget.set_entity_colours(team_visual_data->build_team_colour_cache());

    on_mission_started_handle =
        mission_manager->on_mission_started.AddRaw(this, &FHUDManager::on_mission_started);
    on_enemies_killed_handle =
        mission_manager->on_enemies_killed.AddRaw(this, &FHUDManager::on_enemies_killed);
    on_surviving_entity_health_updated_handle =
        mission_manager->on_surviving_entity_health_updated.AddRaw(
            this, &FHUDManager::on_surviving_entity_health_updated);
    on_mission_ended_handle =
        mission_manager->on_mission_ended.AddRaw(this, &FHUDManager::on_mission_ended);

    state = EHUDManagerState::Active;
    set_player_ship(new_player_ship);
    update_player_status();
    update_entity_counts();
    if (mission_manager->is_ready()) {
        on_mission_started(*mission_manager);
    }
}

void FHUDManager::deactivate() {
    remove_player_ship_delegates();
    remove_mission_delegates();
    if (IsValid(hud_widget.Get())) {
        hud_widget->RemoveFromParent();
    }

    update_timers.reset();
    hud_widget.Reset();
    player_controller.Reset();
    player_ship.Reset();
    mission_manager = nullptr;
    entity_registry = nullptr;
    simulation_clock = {};
    near_cursor_distance = 0.f;
    far_cursor_distance = 0.f;
    state = EHUDManagerState::Disabled;
    mission_started_pending = false;
    enemies_killed_pending = false;
    surviving_entity_health_pending = false;
    mission_ended_pending = false;
    has_logged.reset();
}

void FHUDManager::tick() {
    if (state != EHUDManagerState::Active) {
        return;
    }

    if (auto error{ml::report_invalid_uobject_ptrs({
            SANDBOX_NAMED_UOBJECT_PTR(hud_widget.Get()),
            SANDBOX_NAMED_UOBJECT_PTR(mission_manager),
            SANDBOX_NAMED_UOBJECT_PTR(entity_registry),
        })}) {
        log_error_once(has_logged.tick_dependencies_error_logged, TEXT("FHUDManager::tick"), error);
        return;
    }

    update_player_hud();
    update_timers.tick();
    if (mission_started_pending) {
        mission_started_pending = false;
        hud_widget->set_mission_status(*mission_manager);
        hud_widget->set_mission_enemies_remaining(mission_manager->get_kills_remaining());
        update_mission_status();
        hud_widget->update_mission_surviving_entity_health(*mission_manager);
    }
    if (enemies_killed_pending) {
        enemies_killed_pending = false;
        hud_widget->set_mission_enemies_remaining(mission_manager->get_kills_remaining());
    }
    if (surviving_entity_health_pending) {
        surviving_entity_health_pending = false;
        hud_widget->update_mission_surviving_entity_health(*mission_manager);
    }
    if (mission_ended_pending) {
        mission_ended_pending = false;
        hud_widget->set_mission_state(mission_manager->get_mission_state());
        hud_widget->set_mission_enemies_remaining(mission_manager->get_kills_remaining());
        hud_widget->update_mission_surviving_entity_health(*mission_manager);
        update_mission_status();
    }
    if (update_timers.try_consume(FHUDUpdateTimerIndex::player_status)) {
        update_player_status();
    }
    if (update_timers.try_consume(FHUDUpdateTimerIndex::mission_status)) {
        update_mission_status();
    }
    if (update_timers.try_consume(FHUDUpdateTimerIndex::entity_counts)) {
        update_entity_counts();
    }
}

void FHUDManager::set_player_ship(ATestSpaceShip* const new_player_ship) noexcept {
    remove_player_ship_delegates();
    player_ship = new_player_ship;
    bind_player_ship_delegates();

    if (!IsValid(hud_widget.Get())) {
        return;
    }

    if (player_ship.IsValid()) {
        on_health_changed(player_ship->get_health_info());
        on_speed_changed(player_ship->get_speed());
        on_target_speed_changed(player_ship->get_target_speed());
        on_energy_changed(1.f);
        on_bombs_changed(player_ship->get_bombs());
        on_laser_firing_mode_changed(ELaserFiringState::idle);
        on_lock_on_acquired(nullptr);
        on_ship_fire_rate_changed(player_ship->get_laser_fire_rate());
        hud_widget->set_crosshair_widget_visibility(ESlateVisibility::Visible);
    } else {
        hud_widget->set_crosshair_widget_visibility(ESlateVisibility::Collapsed);
        hud_widget->set_lock_on_widget_visibility(false);
    }
}

void FHUDManager::clear_player_ship() noexcept {
    remove_player_ship_delegates();
    player_ship.Reset();
    if (IsValid(hud_widget.Get())) {
        hud_widget->set_crosshair_widget_visibility(ESlateVisibility::Collapsed);
        hud_widget->set_lock_on_widget_visibility(false);
    }
}

void FHUDManager::set_selected_mapping_context(FString const& context_name) {
    if (!IsValid(hud_widget.Get())) {
        UE_LOG(LogSandboxUI,
               Warning,
               TEXT("FHUDManager::set_selected_mapping_context: HUD widget is invalid."));
        return;
    }

    hud_widget->set_selected_imc(FStringView{context_name});
}

bool FHUDManager::validate_player_ship_for_update() {
    auto* const ship{player_ship.Get()};
    if (auto error{ml::report_invalid_uobject_ptrs({
            SANDBOX_NAMED_UOBJECT_PTR(ship),
            SANDBOX_NAMED_UOBJECT_PTR(entity_registry),
        })}) {
        log_error_once(has_logged.player_ship_error_logged,
                       TEXT("FHUDManager: Cannot update player HUD"),
                       error);
        return false;
    }

    auto const unique_id{ship->get_unique_id()};
    if (!unique_id.is_valid() || !entity_registry->is_valid_unique_id(unique_id)) {
        if (!has_logged.player_ship_unique_id_error_logged) {
            UE_LOG(LogSandboxUI,
                   Warning,
                   TEXT("FHUDManager: Player ship unique_id is invalid (%d)."),
                   unique_id.id);
            has_logged.player_ship_unique_id_error_logged = true;
        }
        return false;
    }

    return true;
}

void FHUDManager::update_player_hud() {
    if (!validate_player_ship_for_update()) {
        return;
    }

    if (auto error{ml::report_invalid_uobject_ptrs({
            SANDBOX_NAMED_UOBJECT_PTR(hud_widget.Get()),
            SANDBOX_NAMED_UOBJECT_PTR(player_controller.Get()),
        })}) {
        log_error_once(has_logged.player_hud_dependencies_error_logged,
                       TEXT("FHUDManager::update_player_hud"),
                       error);
        return;
    }

    update_crosshair_positions(*player_ship);
    update_lock_on_widget(*player_ship);
    update_input_widgets(*player_ship);
}

void FHUDManager::update_mission_status() {
    if (auto error{ml::report_invalid_uobject_ptrs({
            SANDBOX_NAMED_UOBJECT_PTR(hud_widget.Get()),
            SANDBOX_NAMED_UOBJECT_PTR(mission_manager),
        })}) {
        log_error_once(has_logged.mission_status_dependencies_error_logged,
                       TEXT("FHUDManager::update_mission_status"),
                       error);
        return;
    }

    hud_widget->set_stopwatch_time(mission_manager->get_mission_stopwatch());
    hud_widget->set_mission_time(mission_manager->get_mission_stopwatch());
    hud_widget->set_mission_time_remaining(mission_manager->get_time_remaining());
}

void FHUDManager::update_player_status() {
    if (!validate_player_ship_for_update()) {
        return;
    }
    if (auto error{ml::report_invalid_uobject_ptrs({
            SANDBOX_NAMED_UOBJECT_PTR(hud_widget.Get()),
        })}) {
        log_error_once(has_logged.player_status_dependencies_error_logged,
                       TEXT("FHUDManager::update_player_status"),
                       error);
        return;
    }

    hud_widget->set_health(player_ship->get_health_info());
    hud_widget->set_speed(player_ship->get_speed());
    hud_widget->set_target_speed(player_ship->get_target_speed());
    hud_widget->set_points(player_ship->get_kills());
}

void FHUDManager::update_entity_counts() {
    if (auto error{ml::report_invalid_uobject_ptrs({
            SANDBOX_NAMED_UOBJECT_PTR(hud_widget.Get()),
            SANDBOX_NAMED_UOBJECT_PTR(entity_registry),
        })}) {
        log_error_once(has_logged.entity_counts_dependencies_error_logged,
                       TEXT("FHUDManager::update_entity_counts"),
                       error);
        return;
    }

    hud_widget->set_entity_counts(entity_registry->count_alive_per_team_and_type());
}

void FHUDManager::update_crosshair_positions(ATestSpaceShip const& ship) {
    auto* const controller{player_controller.Get()};
    if (auto error{ml::report_invalid_uobject_ptrs({
            SANDBOX_NAMED_UOBJECT_PTR(controller),
            SANDBOX_NAMED_UOBJECT_PTR(hud_widget.Get()),
        })}) {
        log_error_once(has_logged.crosshair_dependencies_error_logged,
                       TEXT("FHUDManager::update_crosshair_positions"),
                       error);
        return;
    }

    auto const ship_socket{ship.get_middle_socket()};
    auto const ship_loc{ship_socket.GetLocation()};
    auto const ship_fwd{ship_socket.GetUnitAxis(EAxis::X)};
    auto const near_world_pos{ship_loc + ship_fwd * near_cursor_distance};
    auto const far_world_pos{ship_loc + ship_fwd * far_cursor_distance};
    FVector2d near_screen_pos{};
    FVector2d far_screen_pos{};

    constexpr bool bPlayerViewportRelative{false};
    if (!UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(
            controller, near_world_pos, near_screen_pos, bPlayerViewportRelative)) {
        UE_LOG(
            LogSandboxUI, Warning, TEXT("FHUDManager: Failed to project near crosshair position."));
    }
    if (!UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(
            controller, far_world_pos, far_screen_pos, bPlayerViewportRelative)) {
        UE_LOG(
            LogSandboxUI, Warning, TEXT("FHUDManager: Failed to project far crosshair position."));
    }

    hud_widget->set_crosshair_positions(near_screen_pos, far_screen_pos);
}

void FHUDManager::update_lock_on_widget(ATestSpaceShip const& ship) {
    auto const* const target{ship.get_lock_on_target()};
    if (!target) {
        return;
    }
    auto* const controller{player_controller.Get()};
    if (auto error{ml::report_invalid_uobject_ptrs({
            SANDBOX_NAMED_UOBJECT_PTR(target),
            SANDBOX_NAMED_UOBJECT_PTR(controller),
            SANDBOX_NAMED_UOBJECT_PTR(hud_widget.Get()),
        })}) {
        log_error_once(has_logged.lock_on_dependencies_error_logged,
                       TEXT("FHUDManager::update_lock_on_widget"),
                       error);
        return;
    }

    FVector2d screen_pos{};
    constexpr bool bPlayerViewportRelative{false};
    if (!UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(
            controller, target->GetActorLocation(), screen_pos, bPlayerViewportRelative)) {
        UE_LOG(LogSandboxUI, Warning, TEXT("FHUDManager: Failed to project lock-on position."));
    }

    hud_widget->set_lock_on_widget_position(screen_pos);
}

void FHUDManager::update_input_widgets(ATestSpaceShip const& ship) {
    if (auto error{ml::report_invalid_uobject_ptrs({
            SANDBOX_NAMED_UOBJECT_PTR(hud_widget.Get()),
        })}) {
        log_error_once(has_logged.input_widgets_dependencies_error_logged,
                       TEXT("FHUDManager::update_input_widgets"),
                       error);
        return;
    }

    hud_widget->set_turning(ship.get_turn_input());
    hud_widget->set_moving(ship.get_move_input());
    hud_widget->set_desired_velocity_scale(ship.get_target_local_planar_velocity_scale());
    hud_widget->set_ship_velocity(ship.get_velocity());
    hud_widget->set_target_velocity(ship.get_target_local_planar_velocity());
    hud_widget->set_control_mode(*ml::to_string_without_type_prefix(ship.get_control_mode()));
    hud_widget->set_flight_mode(*ml::to_string_without_type_prefix(ship.get_flight_mode()));
}

void FHUDManager::on_health_changed(FShipHealth const value) {
    if (IsValid(hud_widget.Get())) {
        hud_widget->set_health(value);
    }
}

void FHUDManager::on_speed_changed(float const value) {
    if (IsValid(hud_widget.Get())) {
        hud_widget->set_speed(value);
    }
}

void FHUDManager::on_target_speed_changed(float const value) {
    if (IsValid(hud_widget.Get())) {
        hud_widget->set_target_speed(value);
    }
}

void FHUDManager::on_energy_changed(float const value) {
    if (IsValid(hud_widget.Get())) {
        hud_widget->set_energy(value);
    }
}

void FHUDManager::on_bombs_changed(int32 const value) {
    if (IsValid(hud_widget.Get())) {
        hud_widget->set_bombs(value);
    }
}

void FHUDManager::on_ship_fire_rate_changed(ETestShipFireRate const value) {
    if (IsValid(hud_widget.Get())) {
        hud_widget->set_fire_rate(*ml::to_string_without_type_prefix(value));
    }
}

void FHUDManager::on_laser_firing_mode_changed(ELaserFiringState const mode) {
    if (!IsValid(hud_widget.Get())) {
        return;
    }

    switch (mode) {
        case ELaserFiringState::lock_on_searching: {
            hud_widget->set_crosshair_colours(FLinearColor::Yellow, FLinearColor::Red);
            break;
        }
        case ELaserFiringState::lock_on_acquired: {
            break;
        }
        default: {
            UE_LOG(LogSandboxUI, Warning, TEXT("FHUDManager: Unhandled laser firing mode."));
            [[fallthrough]];
        }
        case ELaserFiringState::idle:
            [[fallthrough]];
        case ELaserFiringState::lock_on_transition:
            [[fallthrough]];
        case ELaserFiringState::burst: {
            hud_widget->set_crosshair_colours(FLinearColor::Green, FLinearColor::Green);
            break;
        }
    }
}

void FHUDManager::on_lock_on_acquired(AActor* const target) {
    if (IsValid(hud_widget.Get())) {
        hud_widget->set_lock_on_widget_visibility(target != nullptr);
    }
}

#if WITH_EDITOR
void FHUDManager::on_speed_sampled(std::span<FVector2d> const samples, int32 const oldest_index) {
    if (IsValid(hud_widget.Get())) {
        hud_widget->update_sampled_speed(samples, oldest_index);
    }
}
#endif

void FHUDManager::bind_player_ship_delegates() {
    if (!player_ship.IsValid()) {
        return;
    }

    player_ship->on_health_changed.BindRaw(this, &FHUDManager::on_health_changed);
    player_ship->on_speed_changed.BindRaw(this, &FHUDManager::on_speed_changed);
    player_ship->on_target_speed_changed.BindRaw(this, &FHUDManager::on_target_speed_changed);
    player_ship->on_energy_changed.BindRaw(this, &FHUDManager::on_energy_changed);
    player_ship->on_bombs_changed.BindRaw(this, &FHUDManager::on_bombs_changed);
    player_ship->on_laser_mode_changed.BindRaw(this, &FHUDManager::on_laser_firing_mode_changed);
    player_ship->on_lock_on_acquired.BindRaw(this, &FHUDManager::on_lock_on_acquired);
    player_ship->on_ship_fire_rate_changed.BindRaw(this, &FHUDManager::on_ship_fire_rate_changed);
#if WITH_EDITOR
    player_ship->on_speed_sampled.BindRaw(this, &FHUDManager::on_speed_sampled);
#endif
}

void FHUDManager::remove_player_ship_delegates() {
    if (!player_ship.IsValid()) {
        return;
    }

    player_ship->on_health_changed.Unbind();
    player_ship->on_speed_changed.Unbind();
    player_ship->on_target_speed_changed.Unbind();
    player_ship->on_energy_changed.Unbind();
    player_ship->on_bombs_changed.Unbind();
    player_ship->on_laser_mode_changed.Unbind();
    player_ship->on_lock_on_acquired.Unbind();
    player_ship->on_ship_fire_rate_changed.Unbind();
#if WITH_EDITOR
    player_ship->on_speed_sampled.Unbind();
#endif
}

void FHUDManager::on_mission_started(ATestMissionManager const& manager) {
    if (&manager != mission_manager || !IsValid(hud_widget.Get())) {
        return;
    }

    mission_started_pending = true;
}

void FHUDManager::on_enemies_killed(ATestMissionManager const& manager) {
    if (&manager != mission_manager || !IsValid(hud_widget.Get())) {
        return;
    }

    enemies_killed_pending = true;
}

void FHUDManager::on_surviving_entity_health_updated(ATestMissionManager const& manager) {
    if (&manager != mission_manager || !IsValid(hud_widget.Get())) {
        return;
    }

    surviving_entity_health_pending = true;
}

void FHUDManager::on_mission_ended(ATestMissionManager const& manager) {
    if (&manager != mission_manager || !IsValid(hud_widget.Get())) {
        return;
    }

    mission_ended_pending = true;
}

void FHUDManager::remove_mission_delegates() {
    if (!mission_manager) {
        return;
    }

    if (on_mission_started_handle.IsValid()) {
        mission_manager->on_mission_started.Remove(on_mission_started_handle);
    }
    if (on_enemies_killed_handle.IsValid()) {
        mission_manager->on_enemies_killed.Remove(on_enemies_killed_handle);
    }
    if (on_surviving_entity_health_updated_handle.IsValid()) {
        mission_manager->on_surviving_entity_health_updated.Remove(
            on_surviving_entity_health_updated_handle);
    }
    if (on_mission_ended_handle.IsValid()) {
        mission_manager->on_mission_ended.Remove(on_mission_ended_handle);
    }
    on_mission_started_handle.Reset();
    on_enemies_killed_handle.Reset();
    on_surviving_entity_health_updated_handle.Reset();
    on_mission_ended_handle.Reset();
}
