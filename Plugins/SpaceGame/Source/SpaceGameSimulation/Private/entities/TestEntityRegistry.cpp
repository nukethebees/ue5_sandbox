#include "SpaceGameSimulation/entities/TestEntityRegistry.h"

#include <sandbox/simulation/entity_combat_accounting.h>
#include <sandbox/simulation/entity_death_accounting.h>
#include <sandbox/simulation/entity_registry_history.h>
#include <sandbox/simulation/entity_registry_refresh.h>
#include <sandbox/simulation/entity_registry_spawn.h>
#include <SpaceGameSimulation/entities/DirectDamageEvents.h>
#include <SpaceGameSimulation/entities/EntityDeathInfo.h>
#include <SpaceGameSimulation/entities/NativeEntityRegistryView.h>
#include <SpaceGameSimulation/entities/NativeEntityTypes.h>
#include <SpaceGameSimulation/simulation/NativeVectorTypes.h>
#include <SpaceGameSimulation/support/logging/SandboxLogCategories.h>

#include <SandboxCore/array_checks.h>
#include <SandboxCore/array_utils.h>
#include <SandboxCore/soa_rotator_utils.h>
#include <SandboxCore/soa_vector_utils.h>

#include <utility>

namespace entity_registry_detail {
template <typename Error>
void check_accounting_result(std::expected<void, Error> const& result) {
    if (result) {
        return;
    }

    switch (result.error().code) {
        case ml::simulation::UniqueIdLookupError::InvalidHandle:
            check(false);
            return;
        case ml::simulation::UniqueIdLookupError::MissingStaleHandle:
            checkf(false, TEXT("A missing unique ID should be impossible here."));
            return;
    }
}
} // namespace entity_registry_detail

/* **************************************** */
// Lifecycle
/* **************************************** */
void FTestEntityRegistry::reset() {
    ml::reset(entity_data,
              queued_entity_data,
              unique_entity_history_,
              queued_death_infos,
              queued_direct_damage_events);
    bookkeeping_.reset();
    statistics_.reset();
}
void FTestEntityRegistry::begin_tick() {
    bookkeeping_.begin_tick();
}
void FTestEntityRegistry::commit_updates() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::FTestEntityRegistry::commit_updates);

    validate_unique_queued_entity_update_handles();
    commit_entity_updates();
    commit_death_updates();

    ml::reset(queued_entity_data, queued_death_infos);
    bookkeeping_.clear_queued_updates();

    validate_array_sizes();
}
void FTestEntityRegistry::refresh_free_indices() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::FTestEntityRegistry::refresh_free_indices);

    bookkeeping_.refresh_free_indices(
        std::span{entity_data.alive.GetData(), static_cast<std::size_t>(entity_data.alive.Num())});
}
void FTestEntityRegistry::end_tick() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::FTestEntityRegistry::end_tick);

    refresh_free_indices();

    queued_direct_damage_events.reset();
    bookkeeping_.clear_dead_entities();

    validate_array_sizes();
    validate_unique_ids();
    validate_unique_entity_data();
}

/* **************************************** */
// Entity creation
/* **************************************** */
auto FTestEntityRegistry::add_entities(EntityData::ConstView const view) -> SpawnedEntityHandles {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::FTestEntityRegistry::add_entities);

    view.validate_array_sizes();

    auto const count{view.num()};
    SpawnedEntityHandles new_entities;

    if (count < 1) {
        return new_entities;
    }

    new_entities.first_id = {.id = unique_entity_history_.num()};
    unique_entity_history_.add_defaulted(count);
    new_entities.registry_handles.add_uninitialised(count);
    auto const unique_entities{unique_entity_history_.get_view().columns()};

    auto const reuse_count{FMath::Min(bookkeeping_.available_free_slot_count(), count)};
    for (int32 source_index{}; source_index < reuse_count; ++source_index) {
        auto const slot_index{bookkeeping_.take_free_slot()};
        entity_data.copy_element(slot_index, view, source_index);

        auto const handle{
            ml::simulation::register_spawned_entity(bookkeeping_,
                                                    statistics_,
                                                    unique_entities,
                                                    slot_index,
                                                    new_entities.first_id + source_index,
                                                    ml::to_native(view.teams[source_index]),
                                                    ml::to_native(view.entity_types[source_index]),
                                                    view.alive[source_index])};
        new_entities.registry_handles.set(source_index, handle.index, handle.generation);
    }

    auto const append_count{count - reuse_count};
    auto const first_slot_index{entity_data.num()};
    bookkeeping_.append_slots(append_count);
    entity_data.append_from(view.get_view(reuse_count, append_count));

    for (int32 offset{}; offset < append_count; ++offset) {
        auto const source_index{reuse_count + offset};
        auto const handle{
            ml::simulation::register_spawned_entity(bookkeeping_,
                                                    statistics_,
                                                    unique_entities,
                                                    first_slot_index + offset,
                                                    new_entities.first_id + source_index,
                                                    ml::to_native(view.teams[source_index]),
                                                    ml::to_native(view.entity_types[source_index]),
                                                    view.alive[source_index])};
        new_entities.registry_handles.set(source_index, handle.index, handle.generation);
    }

    validate_array_sizes();
    validate_unique_ids();
    return new_entities;
}

/* **************************************** */
// Queued updates
/* **************************************** */
void FTestEntityRegistry::queue_entity_updates(ConstView const view,
                                               EntityDeathInfo const& death_info) {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::FTestEntityRegistry::queue_entity_updates);

    check(view.indices.Num() == view.data.num());
    queued_entity_data.append_from(view.data);
    bookkeeping_.queue_update_handles(
        std::span{view.indices.GetData(), static_cast<std::size_t>(view.indices.Num())});

    death_info.validate_array_sizes();
    queued_death_infos.append_from(death_info.get_const_view());
}
void FTestEntityRegistry::commit_entity_updates() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::FTestEntityRegistry::commit_entity_updates);

    auto const count{queued_entity_data.num()};
    check(static_cast<int32>(bookkeeping_.queued_update_handles.size()) == count);
    auto const unique_entities{unique_entity_history_.get_view().columns()};
    auto const invalid_update{ml::simulation::apply_entity_updates(
        bookkeeping_,
        statistics_,
        unique_entities,
        ml::make_native_update_view(entity_data.get_view()),
        ml::make_native_update_view(queued_entity_data.get_const_view()))};
    check(invalid_update < 0);
}
void FTestEntityRegistry::commit_death_updates() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::FTestEntityRegistry::commit_death_updates);

    queued_death_infos.validate_array_sizes();
    auto const unique_entities{unique_entity_history_.get_view().columns()};
    auto const result{ml::simulation::record_entity_deaths(
        bookkeeping_, statistics_, unique_entities, queued_death_infos.get_const_view())};
    entity_registry_detail::check_accounting_result(result);
}

/* **************************************** */
// Damage events
/* **************************************** */
void FTestEntityRegistry::queue_direct_damage_events(
    DirectDamageEventsConstView const damage_events) {
    damage_events.validate_array_sizes();

    auto const unique_entities{unique_entity_history_.get_const_view().columns()};
    auto const result{ml::simulation::record_damage_events(statistics_,
                                                           bookkeeping_.generations,
                                                           bookkeeping_.unique_ids,
                                                           unique_entities,
                                                           damage_events)};
    entity_registry_detail::check_accounting_result(result);

    queued_direct_damage_events.append_from(damage_events);
}
void FTestEntityRegistry::record_shots(TConstArrayView<FRegistryEntityHandle> const instigators) {
    auto const unique_entities{unique_entity_history_.get_const_view().columns()};
    auto const result{ml::simulation::record_shots(
        statistics_,
        bookkeeping_.generations,
        bookkeeping_.unique_ids,
        unique_entities,
        {instigators.GetData(), static_cast<std::size_t>(instigators.Num())})};
    entity_registry_detail::check_accounting_result(result);
}
auto FTestEntityRegistry::get_direct_damage_queue_view() const -> DirectDamageEvents const& {
    return queued_direct_damage_events;
}

/* **************************************** */
// Handle queries
/* **************************************** */
auto FTestEntityRegistry::analyse_handle(FRegistryEntityHandle const handle) const
    -> ERegistryHandleState {
    return bookkeeping_.analyse_handle(handle);
}
auto FTestEntityRegistry::is_stale(FRegistryEntityHandle const handle) const -> bool {
    return bookkeeping_.is_stale(handle);
}

/* **************************************** */
// Entity data updates
/* **************************************** */
void FTestEntityRegistry::refresh_handles(TArrayView<FRegistryEntityHandle> const handles) const {
    auto const invalid_index{ml::simulation::refresh_registry_handles(
        ml::make_native_query_view(*this),
        {handles.GetData(), static_cast<std::size_t>(handles.Num())})};
    check(invalid_index < 0);
}
void FTestEntityRegistry::refresh_locations(TConstArrayView<FRegistryEntityHandle> handles,
                                            FVectors3f::View const& locations) {
    auto const n{handles.Num()};
    check(ml::num(locations) == n);

    auto const inactive_index{ml::simulation::copy_registry_entity_data(
        ml::make_native_query_view(*this),
        {handles.GetData(), static_cast<std::size_t>(handles.Num())},
        {.locations = ml::to_native(locations)})};
    check(inactive_index < 0);
}
void FTestEntityRegistry::refresh_entity_data(TArrayView<FRegistryEntityHandle> handles,
                                              FVectors3f::View const& locations,
                                              FVectors3f::View const& velocities,
                                              TArrayView<float> const radii) {
    auto const n_handles{ml::num(handles)};
    if (n_handles == 0) {
        return;
    }

    auto should_update_view{[n_handles](auto const& view) -> bool {
        auto const n_view{ml::num(view)};
        auto const should_update{n_view == n_handles};

        if (!((n_view == 0) || should_update)) {
            UE_LOG(LogSandbox,
                   Fatal,
                   TEXT("View has %d elements but got %d handles"),
                   n_view,
                   n_handles);
        }

        return should_update;
    }};

    refresh_handles(handles);

    auto const update_locations{should_update_view(locations)};
    auto const update_velocities{should_update_view(velocities)};
    auto const update_radii{should_update_view(radii)};
    auto const inactive_index{ml::simulation::copy_registry_entity_data(
        ml::make_native_query_view(*this),
        {handles.GetData(), static_cast<std::size_t>(handles.Num())},
        {.locations = update_locations ? ml::to_native(locations) : ml::simulation::Vectors3fView{},
         .velocities =
             update_velocities ? ml::to_native(velocities) : ml::simulation::Vectors3fView{},
         .radii = update_radii
                    ? std::span<float>{radii.GetData(), static_cast<std::size_t>(radii.Num())}
                    : std::span<float>{}})};
    check(inactive_index < 0);
}

/* **************************************** */
// Entity data queries
/* **************************************** */
auto FTestEntityRegistry::get_location(FRegistryEntityHandle const handle) const -> FVector3f {
    check(is_valid_handle(handle));
    return ml::get_vector3f(entity_data.locations, handle.index);
}
auto FTestEntityRegistry::get_velocity(FRegistryEntityHandle const handle) const -> FVector3f {
    check(is_valid_handle(handle));
    return ml::get_vector3f(entity_data.velocities, handle.index);
}
auto FTestEntityRegistry::get_health(FRegistryEntityHandle const handle) const -> int32 {
    check(is_valid_handle(handle));
    return entity_data.healths[handle.index];
}
auto FTestEntityRegistry::get_team(FRegistryEntityHandle const handle) const -> ETestTeam {
    check(is_valid_handle(handle));
    return entity_data.teams[handle.index];
}
auto FTestEntityRegistry::get_entity_type(FRegistryEntityHandle const handle) const
    -> ETestEntityType {
    check(is_valid_handle(handle));
    return entity_data.entity_types[handle.index];
}
auto FTestEntityRegistry::get_alive(FRegistryEntityHandle const handle) const -> bool {
    check(is_valid_handle(handle));
    return static_cast<bool>(entity_data.alive[handle.index]);
}

/* **************************************** */
// Entity collection queries
/* **************************************** */
auto FTestEntityRegistry::get_moved_entities_this_tick() const
    -> TConstArrayView<FRegistryEntityHandle> {
    return {bookkeeping_.moved_entities.data(),
            static_cast<int32>(bookkeeping_.moved_entities.size())};
}
auto FTestEntityRegistry::get_dead_entities_this_frame() const
    -> TConstArrayView<FRegistryEntityHandle> {
    return {bookkeeping_.dead_entities.data(),
            static_cast<int32>(bookkeeping_.dead_entities.size())};
}
auto FTestEntityRegistry::get_handles_not_in_team(ETestTeam const team) const
    -> TArray<FRegistryEntityHandle> {
    TArray<FRegistryEntityHandle> out;
    get_handles_not_in_team(team, out);
    return out;
}
void FTestEntityRegistry::get_handles_not_in_team(ETestTeam const team,
                                                  TArray<FRegistryEntityHandle>& out) const {
    out.SetNumUninitialized(get_num_elements());
    auto const count{ml::simulation::collect_non_team_alive_entities(
        ml::make_native_query_view(*this),
        ml::to_native(team),
        {out.GetData(), static_cast<std::size_t>(out.Num())})};
    out.SetNum(count, EAllowShrinking::No);
}

/* **************************************** */
// Aggregate queries
/* **************************************** */
auto FTestEntityRegistry::get_num_elements() const noexcept -> int32 {
    return entity_data.num();
}
auto FTestEntityRegistry::get_num_alive_active_entities() const noexcept -> int32 {
    return statistics_.alive_count();
}
auto FTestEntityRegistry::count_kills() const noexcept -> int32 {
    return statistics_.cumulative_kill_count();
}
auto FTestEntityRegistry::count_alive() const noexcept -> int32 {
    return statistics_.alive_count();
}
auto FTestEntityRegistry::count_alive(ETestEntityType const type) const noexcept -> int32 {
    return statistics_.count_alive(ml::to_native(type));
}
auto FTestEntityRegistry::count_alive_per_team() const noexcept -> TeamCounts {
    return statistics_.count_alive_per_team();
}
auto FTestEntityRegistry::count_alive_per_team_and_type() const noexcept -> EntityCounts {
    return statistics_.count_alive_per_team_and_type();
}
auto FTestEntityRegistry::count_alive_not_on_team(ETestTeam const team) const noexcept -> int32 {
    return statistics_.count_alive_not_on_team(ml::to_native(team));
}

/* **************************************** */
// Unique entity queries
/* **************************************** */
auto FTestEntityRegistry::is_valid_unique_id(TestEntityUniqueId const id) const -> bool {
    return ml::simulation::is_valid_unique_id(id, get_num_unique_ids_issued());
}
auto FTestEntityRegistry::find_unique_id(FRegistryEntityHandle const handle) const
    -> TestEntityUniqueId {
    auto const unique_entities{unique_entity_history_.get_const_view().columns()};
    auto const result{ml::simulation::find_entity_unique_id(
        bookkeeping_.generations, bookkeeping_.unique_ids, unique_entities, handle)};
    if (result) {
        return *result;
    }

    switch (result.error()) {
        case ml::simulation::UniqueIdLookupError::InvalidHandle:
            check(false);
            return {};
        case ml::simulation::UniqueIdLookupError::MissingStaleHandle:
            checkf(false, TEXT("A missing unique ID should be impossible here."));
            return {};
    }
    return {};
}
auto FTestEntityRegistry::get_kills(TestEntityUniqueId const id) const -> uint32 {
    check(is_valid_unique_id(id));
    auto const unique_entities{unique_entity_history_.get_const_view().columns()};
    return unique_entities.kills[id.id];
}

/* **************************************** */
// Spatial queries
/* **************************************** */
auto FTestEntityRegistry::collect_entities_in_range(
    FVector3f const& origin,
    float const radius,
    TArrayView<FRegistryEntityHandle> const out_entities) const -> int32 {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::FTestEntityRegistry::collect_entities_in_range);

    return ml::simulation::collect_entities_in_range(
        ml::make_native_query_view(*this),
        ml::to_native(origin),
        radius,
        {out_entities.GetData(), static_cast<std::size_t>(out_entities.Num())});
}

/* **************************************** */
// Validation
/* **************************************** */
void FTestEntityRegistry::validate_unique_queued_entity_update_handles() const {
#if DO_CHECK
    TBitArray<> seen_handles{false, entity_data.num()};
    for (auto const handle : bookkeeping_.queued_update_handles) {
        check(is_valid_handle(handle));
        checkf(!seen_handles[handle.index],
               TEXT("Entity update handle %s was queued more than once in one tick"),
               *LexToString(handle));
        seen_handles[handle.index] = true;
    }
#endif
}
void FTestEntityRegistry::validate_array_sizes() const {
    auto const entity_count{entity_data.num()};
    ml::fatal_if_nums_not_equal({entity_count,
                                 static_cast<int32>(bookkeeping_.generations.size()),
                                 static_cast<int32>(bookkeeping_.unique_ids.size())});

    entity_data.validate_array_sizes();
    queued_direct_damage_events.validate_array_sizes();

#if DO_CHECK
    queued_entity_data.validate_array_sizes();
    unique_entity_history_.get_const_view().columns().validate_array_sizes();
    check(queued_entity_data.num() ==
          static_cast<int32>(bookkeeping_.queued_update_handles.size()));
#endif
}
void FTestEntityRegistry::validate_unique_ids() const {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::FTestEntityRegistry::validate_unique_ids);

    // Development-time validation for the unique id system
    auto const unique_entities{unique_entity_history_.get_const_view().columns()};
    auto const n{static_cast<int32>(bookkeeping_.unique_ids.size())};

    for (int32 i{0}; i < n; ++i) {
        auto const unique_id{bookkeeping_.unique_ids[i]};
        if (!is_valid_unique_id(unique_id)) {
            UE_LOG(LogSandbox,
                   Fatal,
                   TEXT("Invalid unique id detected: (id[%d] = %d)"),
                   i,
                   unique_id.id);
        }
        check(unique_entities.registry_indices[unique_id.id] == i);
        check(unique_entities.registry_generations[unique_id.id] == bookkeeping_.generations[i]);
    }
}
void FTestEntityRegistry::validate_unique_entity_data() const {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::FTestEntityRegistry::validate_unique_entity_data);

    auto const unique_entities{unique_entity_history_.get_const_view().columns()};
    auto const n{unique_entities.num()};

    for (int32 i{0}; i < n; ++i) {
        FRegistryEntityHandle const handle{
            unique_entities.registry_indices[i],
            unique_entities.registry_generations[i],
        };
        auto const handle_status{analyse_handle(handle)};
        check(handle_status != ERegistryHandleState::Invalid);
        check(handle_status != ERegistryHandleState::Null);

        if (unique_entities.alive[i]) {
            continue;
        }
        check(unique_entities.death_reason[i] != ETestDeathReason::Unset);
    }
}
void FTestEntityRegistry::validate_handles(TConstArrayView<FRegistryEntityHandle> const handles) {
    for (auto const handle : handles) {
        if (!is_valid_handle(handle)) {
            UE_LOG(LogSandbox, Fatal, TEXT("Handle is invalid"));
        }
    }
}
