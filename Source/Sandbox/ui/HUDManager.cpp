#include "Sandbox/ui/HUDManager.h"

#include "Sandbox/batch_game/TestMissionManager.h"
#include "Sandbox/batch_game/TestSpaceShip.h"
#include "Sandbox/logging/SandboxLogCategories.h"
#include "Sandbox/ui/ship_hud/ShipHudWidget.h"

#include <SandboxCore/timing.h>

#include <Algo/Sort.h>
#include <Blueprint/WidgetLayoutLibrary.h>
#include <GameFramework/PlayerController.h>

#include <utility>

void FHUDManager::initialise(FTestBatchGameUiUpdateFrequencies const& update_frequencies,
                             FTestMissionManager const& new_mission_manager,
                             FTestEntityRegistry const& new_entity_registry,
                             double const update_tick_rate,
                             ATestSpaceShip const* const new_player_ship) {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::FHUDManager::initialise);
    update_timers.reset();
    mission_data_buffers = {};
    entity_count_data_buffers = {};
    kill_data_buffers = {};
    player_status_data_buffers = {};
    player_flight_data_buffers = {};
    selected_mapping_context.Reset();
    has_mission_data = false;
#if WITH_EDITOR
    sampled_speed_data_buffers = {};
    has_sampled_speed_data = false;
#endif

    check(update_tick_rate > 0.0);

    auto const periods{update_frequencies.to_array()};
    auto const n_periods{periods.Num()};
    for (int32 i{0}; i < n_periods; ++i) {
        if (!ml::valid_periods(periods[i])) {
            UE_LOG(LogSandboxUI, Fatal, TEXT("FHUDManager::initialise: Invalid update period."));
        }

        auto const tick_period{static_cast<FPeriodicTickCountdown8::counter_type>(
            FMath::CeilToInt64(periods[i] * update_tick_rate))};
        if (!ml::valid_periods(tick_period)) {
            UE_LOG(LogSandboxUI, Fatal, TEXT("FHUDManager::initialise: Invalid tick period."));
        }
        update_timers.add_started(tick_period);
    }

    mission_manager = &new_mission_manager;
    entity_registry = &new_entity_registry;
    player_ship = new_player_ship;
    top_killer_ids_buffer.Reset();
    top_killer_ids_buffer.Reserve(entity_registry->get_num_unique_ids_issued());
    check(mission_manager);
    check(entity_registry);
    state = EHUDManagerState::Active;

    collect_mission_data();
    collect_entity_count_data();
    collect_kill_data();
    collect_player_status_data();
    collect_player_flight_data();
#if WITH_EDITOR
    collect_sampled_speed_data();
#endif

    for (auto const hud_ptr : registered_huds) {
        auto* const hud{hud_ptr.Get()};
        check(IsValid(hud));
        synchronise_hud(*hud);
    }
}
void FHUDManager::deactivate() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::FHUDManager::deactivate);
    update_timers.reset();
    registered_huds.Reset();
    player_ship.Reset();
    mission_manager = nullptr;
    entity_registry = nullptr;
    mission_data_buffers = {};
    entity_count_data_buffers = {};
    kill_data_buffers = {};
    player_status_data_buffers = {};
    player_flight_data_buffers = {};
    top_killer_ids_buffer.Reset();
    has_mission_data = false;
    state = EHUDManagerState::Disabled;
#if WITH_EDITOR
    sampled_speed_data_buffers = {};
    has_sampled_speed_data = false;
#endif
}

void FHUDManager::tick(FPeriodicTickCountdown8::counter_type const num_ticks) {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::FHUDManager::tick);
    check(num_ticks >= 0);
    if (state != EHUDManagerState::Active) {
        return;
    }

    check(mission_manager);
    check(entity_registry);

    auto const changes{collect_data(num_ticks)};
    update_huds(changes);
}

void FHUDManager::force_sample() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::FHUDManager::force_sample);
    if (state != EHUDManagerState::Active) {
        return;
    }

    check(mission_manager);
    check(entity_registry);

    ml::hud_manager::FDataChanges changes;
    changes.mission = collect_mission_data();
    changes.entity_counts = collect_entity_count_data();
    changes.kill_data = collect_kill_data();
    changes.player_status = collect_player_status_data();
    changes.player_flight = collect_player_flight_data();
#if WITH_EDITOR
    changes.sampled_speed = collect_sampled_speed_data();
#endif
    update_huds(changes);
}

void FHUDManager::register_hud(UShipHudWidget& hud) {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::FHUDManager::register_hud);
    check(IsValid(&hud));
    check(!registered_huds.ContainsByPredicate(
        [&hud](TWeakObjectPtr<UShipHudWidget> const existing) { return existing.Get() == &hud; }));

    registered_huds.Emplace(&hud);
    if (state == EHUDManagerState::Active) {
        synchronise_hud(hud);
    }
}
void FHUDManager::unregister_hud(UShipHudWidget& hud) {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::FHUDManager::unregister_hud);
    check(IsValid(&hud));

    auto const index{registered_huds.IndexOfByPredicate(
        [&hud](TWeakObjectPtr<UShipHudWidget> const existing) { return existing.Get() == &hud; })};
    check(index != INDEX_NONE);
    registered_huds.RemoveAt(index);
}

void FHUDManager::set_selected_mapping_context(FString const& context_name) {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::FHUDManager::set_selected_mapping_context);
    if (selected_mapping_context == context_name) {
        return;
    }

    selected_mapping_context = context_name;
}

auto FHUDManager::collect_data(FPeriodicTickCountdown8::counter_type const num_ticks)
    -> ml::hud_manager::FDataChanges {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::FHUDManager::collect_data);
    ml::hud_manager::FDataChanges changes;
    changes.player_flight = collect_player_flight_data();
#if WITH_EDITOR
    changes.sampled_speed = collect_sampled_speed_data();
#endif

    update_timers.tick(num_ticks);
    if (update_timers.try_consume(FHUDUpdateTimerIndex::player_status)) {
        changes.player_status = collect_player_status_data();
    }
    if (update_timers.try_consume(FHUDUpdateTimerIndex::mission_status)) {
        changes.mission = collect_mission_data();
    }
    if (update_timers.try_consume(FHUDUpdateTimerIndex::entity_counts)) {
        changes.entity_counts = collect_entity_count_data();
        changes.kill_data = collect_kill_data();
    }
    return changes;
}
bool FHUDManager::collect_mission_data() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::FHUDManager::collect_mission_data);
    check(mission_manager);
    if (!mission_manager->is_ready()) {
        return false;
    }

    auto& next_data{mission_data_buffers.next()};
    read_mission_data(next_data);
    mission_data_buffers.cycle();
    has_mission_data = true;
    return mission_data_buffers.current() != mission_data_buffers.previous();
}
void FHUDManager::read_mission_data(ml::hud_manager::FMissionDataCache& out) const {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::FHUDManager::read_mission_data);
    check(mission_manager);

    auto& static_data{out.static_data};
    static_data.mission_mode = mission_manager->get_mission_mode();
    static_data.surviving_entity_ids = mission_manager->get_entity_ids_that_must_survive();
    static_data.surviving_entity_types = mission_manager->get_entity_types_that_must_survive();

    auto& status_data{out.status_data};
    status_data.mission_state = mission_manager->get_mission_state();
    status_data.mission_stopwatch = mission_manager->get_mission_stopwatch();
    status_data.time_remaining = mission_manager->get_time_remaining();
    status_data.enemies_remaining = mission_manager->get_kills_remaining();
    status_data.surviving_entity_health = mission_manager->get_entity_health_that_must_survive();
}
bool FHUDManager::collect_entity_count_data() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::FHUDManager::collect_entity_count_data);
    check(entity_registry);

    auto& next_data{entity_count_data_buffers.next()};
    next_data.alive_per_team_and_type = entity_registry->count_alive_per_team_and_type();
    entity_count_data_buffers.cycle();
    return entity_count_data_buffers.current() != entity_count_data_buffers.previous();
}
bool FHUDManager::collect_kill_data() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::FHUDManager::collect_kill_data);
    check(entity_registry);

    auto& next_data{kill_data_buffers.next()};
    auto const& unique_entities{entity_registry->get_unique_entities()};
    auto const n_unique_entities{unique_entities.num()};

    top_killer_ids_buffer.Reset();
    for (int32 entity_index{0}; entity_index < n_unique_entities; ++entity_index) {
        if (unique_entities.kills[entity_index] == 0) {
            continue;
        }
        top_killer_ids_buffer.Add({.id = entity_index});
    }
    Algo::Sort(top_killer_ids_buffer,
               [&unique_entities](TestEntityUniqueId const lhs, TestEntityUniqueId const rhs) {
                   auto const lhs_kills{unique_entities.kills[lhs.id]};
                   auto const rhs_kills{unique_entities.kills[rhs.id]};
                   return lhs_kills != rhs_kills ? lhs_kills > rhs_kills : lhs.id < rhs.id;
               });

    next_data.top_killers.reset();
    next_data.top_killers.add_defaulted(top_killer_ids_buffer.Num());
    auto const n_top_killers{top_killer_ids_buffer.Num()};
    for (int32 top_killer_index{0}; top_killer_index < n_top_killers; ++top_killer_index) {
        auto const entity_id{top_killer_ids_buffer[top_killer_index]};
        next_data.top_killers.entity_ids[top_killer_index] = entity_id;
        next_data.top_killers.entity_types[top_killer_index] =
            unique_entities.entity_types[entity_id.id];
        next_data.top_killers.teams[top_killer_index] = unique_entities.teams[entity_id.id];
        next_data.top_killers.kills[top_killer_index] =
            static_cast<int32>(unique_entities.kills[entity_id.id]);
    }

    next_data.team_kill_matrix = {};
    for (int32 victim_index{0}; victim_index < n_unique_entities; ++victim_index) {
        if (!unique_entities.killed_by[victim_index].is_valid()) {
            continue;
        }

        auto const killer_id{unique_entities.killed_by[victim_index]};
        check(killer_id.is_valid());
        auto const team_index{std::to_underlying(unique_entities.teams[killer_id.id])};
        auto const type_index{std::to_underlying(unique_entities.entity_types[victim_index])};
        if (team_index >= ml::ship_hud::FTeamKillMatrix::team_count ||
            type_index >= ml::ship_hud::FTeamKillMatrix::entity_type_count) {
            continue;
        }
        next_data.team_kill_matrix.add(unique_entities.teams[killer_id.id],
                                       unique_entities.entity_types[victim_index]);
    }
    kill_data_buffers.cycle();
    return kill_data_buffers.current() != kill_data_buffers.previous();
}
bool FHUDManager::collect_player_status_data() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::FHUDManager::collect_player_status_data);
    auto& next_data{player_status_data_buffers.next()};
    next_data = {};

    if (validate_player_ship_for_collection()) {
        next_data.has_player_ship = true;
        next_data.health = player_ship->get_health_info();
        next_data.speed = player_ship->get_speed();
        next_data.target_speed = player_ship->get_target_speed();
        next_data.energy = player_ship->get_energy();
        next_data.points = player_ship->get_kills();
        next_data.fire_rate = player_ship->get_laser_fire_rate();

        auto const firing_mode{player_ship->get_laser_firing_mode()};
        if (firing_mode == ELaserFiringState::lock_on_searching ||
            firing_mode == ELaserFiringState::lock_on_acquired) {
            next_data.near_crosshair_colour = FLinearColor::Yellow;
            next_data.far_crosshair_colour = FLinearColor::Red;
        }
    }

    player_status_data_buffers.cycle();
    return player_status_data_buffers.current() != player_status_data_buffers.previous();
}
bool FHUDManager::collect_player_flight_data() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::FHUDManager::collect_player_flight_data);
    auto& next_data{player_flight_data_buffers.next()};
    next_data = {};
    next_data.selected_mapping_context = selected_mapping_context;

    if (validate_player_ship_for_collection()) {
        auto const ship_socket{player_ship->get_middle_socket()};
        auto const* const lock_on_target{player_ship->get_lock_on_target()};

        next_data.has_player_ship = true;
        next_data.turning = player_ship->get_turn_input();
        next_data.moving = player_ship->get_move_input();
        next_data.desired_velocity_scale = player_ship->get_target_local_planar_velocity_scale();
        next_data.ship_velocity = player_ship->get_velocity();
        next_data.target_velocity = player_ship->get_target_local_planar_velocity();
        next_data.control_mode = player_ship->get_control_mode();
        next_data.flight_mode = player_ship->get_flight_mode();
        next_data.crosshair_origin = ship_socket.GetLocation();
        next_data.crosshair_direction = ship_socket.GetUnitAxis(EAxis::X);
        next_data.has_lock_on_target = IsValid(lock_on_target);
        if (next_data.has_lock_on_target) {
            next_data.lock_on_target_position = lock_on_target->GetActorLocation();
        }
    }

    player_flight_data_buffers.cycle();
    return player_flight_data_buffers.current() != player_flight_data_buffers.previous();
}
#if WITH_EDITOR
bool FHUDManager::collect_sampled_speed_data() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::FHUDManager::collect_sampled_speed_data);
    auto& next_data{sampled_speed_data_buffers.next()};
    next_data = {};
    if (player_ship.IsValid()) {
        auto const samples{player_ship->get_speed_samples()};
        next_data.samples.Append(samples.GetData(), samples.Num());
        next_data.oldest_index = player_ship->get_speed_sample_index();
        has_sampled_speed_data = true;
    }

    sampled_speed_data_buffers.cycle();
    return sampled_speed_data_buffers.current() != sampled_speed_data_buffers.previous();
}
#endif

void FHUDManager::update_huds(ml::hud_manager::FDataChanges const& changes) {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::FHUDManager::update_huds);
    if (registered_huds.IsEmpty()) {
        return;
    }

    for (auto const hud_ptr : registered_huds) {
        auto* const hud{hud_ptr.Get()};
        check(IsValid(hud));

        if (changes.mission) {
            update_mission_hud(*hud);
        }
        if (changes.entity_counts) {
            update_entity_count_hud(*hud);
        }
        if (changes.kill_data) {
            update_kill_data_hud(*hud);
        }
        if (changes.player_status) {
            update_player_status_hud(*hud);
        }
        if (changes.player_flight) {
            update_player_flight_hud(*hud);
        }
#if WITH_EDITOR
        if (changes.sampled_speed) {
            update_sampled_speed_hud(*hud);
        }
#endif
    }
}
void FHUDManager::synchronise_hud(UShipHudWidget& hud) const {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::FHUDManager::synchronise_hud);
    check(IsValid(&hud));

    if (has_mission_data) {
        update_mission_hud(hud);
    }
    update_entity_count_hud(hud);
    update_kill_data_hud(hud);
    update_player_status_hud(hud);
    update_player_flight_hud(hud);
#if WITH_EDITOR
    if (has_sampled_speed_data) {
        update_sampled_speed_hud(hud);
    }
#endif
}

void FHUDManager::update_mission_hud(UShipHudWidget& hud) const {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::FHUDManager::update_mission_hud);
    check(has_mission_data);
    auto const& data{mission_data_buffers.current()};
    hud.set_mission_data(data);
    hud.set_stopwatch_time(data.status_data.mission_stopwatch);
}
void FHUDManager::update_entity_count_hud(UShipHudWidget& hud) const {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::FHUDManager::update_entity_count_hud);
    hud.set_entity_counts(entity_count_data_buffers.current().alive_per_team_and_type);
}
void FHUDManager::update_kill_data_hud(UShipHudWidget& hud) const {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::FHUDManager::update_kill_data_hud);
    auto const& data{kill_data_buffers.current()};
    hud.set_top_killers(data.top_killers);
    hud.set_team_kill_matrix(data.team_kill_matrix);
}
void FHUDManager::update_player_status_hud(UShipHudWidget& hud) const {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::FHUDManager::update_player_status_hud);
    auto const& data{player_status_data_buffers.current()};
    if (!data.has_player_ship) {
        return;
    }

    hud.set_health(data.health);
    hud.set_speed(data.speed);
    hud.set_target_speed(data.target_speed);
    hud.set_energy(data.energy);
    hud.set_points(data.points);
    hud.set_fire_rate(*ml::to_string_without_type_prefix(data.fire_rate));
    hud.set_crosshair_colours(data.near_crosshair_colour, data.far_crosshair_colour);
}
void FHUDManager::update_player_flight_hud(UShipHudWidget& hud) const {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::FHUDManager::update_player_flight_hud);
    auto const& data{player_flight_data_buffers.current()};
    if (!data.has_player_ship) {
        hud.set_crosshair_widget_visibility(ESlateVisibility::Collapsed);
        hud.set_lock_on_widget_visibility(false);
        return;
    }

    hud.set_crosshair_widget_visibility(ESlateVisibility::Visible);
    hud.set_lock_on_widget_visibility(data.has_lock_on_target);
    hud.set_selected_imc(FStringView{data.selected_mapping_context});
    hud.set_turning(data.turning);
    hud.set_moving(data.moving);
    hud.set_desired_velocity_scale(data.desired_velocity_scale);
    hud.set_ship_velocity(data.ship_velocity);
    hud.set_target_velocity(data.target_velocity);
    hud.set_control_mode(*ml::to_string_without_type_prefix(data.control_mode));
    hud.set_flight_mode(*ml::to_string_without_type_prefix(data.flight_mode));

    auto* const controller{hud.GetOwningPlayer()};
    check(IsValid(controller));

    auto const& distances{hud.get_crosshair_distances()};
    auto const near_world_position{data.crosshair_origin +
                                   data.crosshair_direction * distances.near};
    auto const far_world_position{data.crosshair_origin + data.crosshair_direction * distances.far};
    FVector2d near_screen_position{};
    FVector2d far_screen_position{};
    constexpr bool player_viewport_relative{false};
    if (!UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(
            controller, near_world_position, near_screen_position, player_viewport_relative)) {
        UE_LOG(
            LogSandboxUI, Warning, TEXT("FHUDManager: Failed to project near crosshair position."));
    }
    if (!UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(
            controller, far_world_position, far_screen_position, player_viewport_relative)) {
        UE_LOG(
            LogSandboxUI, Warning, TEXT("FHUDManager: Failed to project far crosshair position."));
    }
    hud.set_crosshair_positions(near_screen_position, far_screen_position);

    if (data.has_lock_on_target) {
        FVector2d lock_on_screen_position{};
        if (!UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(
                controller,
                data.lock_on_target_position,
                lock_on_screen_position,
                player_viewport_relative)) {
            UE_LOG(LogSandboxUI, Warning, TEXT("FHUDManager: Failed to project lock-on position."));
        }
        hud.set_lock_on_widget_position(lock_on_screen_position);
    }
}
#if WITH_EDITOR
void FHUDManager::update_sampled_speed_hud(UShipHudWidget& hud) const {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::FHUDManager::update_sampled_speed_hud);
    check(has_sampled_speed_data);
    auto const& data{sampled_speed_data_buffers.current()};
    hud.update_sampled_speed(TConstArrayView<FVector2d>{data.samples}, data.oldest_index);
}
#endif

bool FHUDManager::validate_player_ship_for_collection() const {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::FHUDManager::validate_player_ship_for_collection);
    auto const* const ship{player_ship.Get()};
    if (!IsValid(ship)) {
        return false;
    }

    check(entity_registry);
    auto const unique_id{ship->get_unique_id()};
    return unique_id.is_valid() && entity_registry->is_valid_unique_id(unique_id);
}
