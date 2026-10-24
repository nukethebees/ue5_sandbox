#include "SpaceGamePresentation/presentation/CapitalPresentation.h"

#include <ioj/column_math.h>
#include <ioj/sim/entities/entity_ledger.h>
#include <SandboxGameShared/utilities/actor_utils.h>
#include <SpaceGamePresentation/entities/TestBatchActorCore.h>
#include <SpaceGamePresentation/entities/TestTeamConversion.h>
#include <SpaceGamePresentation/entities/TestTeamVisualData.h>
#include <SpaceGamePresentation/presentation/DelayedNiagaraSpawns.h>
#include <SpaceGameSimulation/support/logging/SandboxLogCategories.h>

#include <SandboxCore/array_checks.h>
#include <SandboxCore/array_utils.h>
#include <SandboxCoreEngine/RotatorConversion.h>
#include <SandboxCoreEngine/uobject_utils.h>
#include <SandboxCoreEngine/VectorConversion.h>

#include <Components/InstancedStaticMeshComponent.h>
#include <Components/SceneComponent.h>
#include <Engine/StaticMesh.h>
#include <NiagaraSystem.h>

FCapitalPresentation::FCapitalPresentation(UInstancedStaticMeshComponent& component)
    : instances{&component} {}

void FCapitalPresentation::set_actor_config(FCapitalShipConfig const* const new_config) noexcept {
    actor_config = new_config;
}

void FCapitalPresentation::set_niagara_spawner(FDelayedNiagaraSpawns& spawner) {
    niagara_spawner = &spawner;
}

void FCapitalPresentation::ValidateOptionalAssets() const {
    check(actor_config);

    if (actor_config->presentation.n_small_explosions > 0 &&
        !IsValid(actor_config->small_death_explosion)) {
        UE_LOG(LogSandbox,
               Warning,
               TEXT("FCapitalPresentation optional asset is missing: small_death_explosion"));
    }
    if (!IsValid(actor_config->main_death_explosion)) {
        UE_LOG(LogSandbox,
               Warning,
               TEXT("FCapitalPresentation optional asset is missing: main_death_explosion"));
    }
}

void FCapitalPresentation::clear_runtime_state_presentation() {
    instances->ClearInstances();
}

void FCapitalPresentation::begin_play_presentation() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::FCapitalPresentation::begin_play_presentation);

    auto* const world{instances->GetWorld()};
    if (!actor_config) {
        UE_LOG(LogSandbox, Fatal, TEXT("FCapitalPresentation actor_config is nullptr."));
    }
    ioj::fatal_if_uobject_ptrs_invalid({
        SANDBOX_NAMED_UOBJECT_PTR(world),
        SANDBOX_NAMED_UOBJECT_PTR(actor_config->mesh.Get()),
        SANDBOX_NAMED_UOBJECT_PTR(actor_config->team_visual_data.Get()),
    });

    configure_ismc();
    add_initial_visual_instances();
    validate_array_sizes();
}

void FCapitalPresentation::update_visual_data(ioj::sim::EntityLedger const& ledger) {
    auto const colours{
        UTestTeamVisualData::build_team_colour_cache(actor_config->team_visual_data)};
    for (auto const& change : simulation().get_frame_changes()) {
        if (change.kind == ::ioj::sim::EntityFrameChangeKind::RemoveSwap) {
            instances->RemoveInstance(change.index);
        } else {
            FTransform const transform{FRotator{ioj::to_unreal(change.rotation)},
                                       FVector{ioj::to_unreal(change.location)}};
            auto const index{instances->AddInstance(transform, is_world_space)};
            check(index == static_cast<int32>(change.index));
            auto const colour{colours[ioj::to_unreal(change.team)]};
            TArray<float> data{colour.R, colour.G, colour.B};
            instances->SetCustomData(index, data, false);
        }
    }
    trigger_death_effects(ledger);
}

void FCapitalPresentation::commit_visual_data() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::FCapitalPresentation::commit_visual_data);

    instances->MarkRenderStateDirty();
}

void FCapitalPresentation::end_tick_presentation() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::FCapitalPresentation::end_tick_presentation);
    validate_array_sizes();
}

void FCapitalPresentation::configure_ismc() {
    ioj::batch::configure_ismc(*instances,
                               {
                                   .mesh = actor_config->mesh.Get(),
                                   .material = actor_config->material.Get(),
                                   .num_custom_data_floats = n_custom_ismc_floats,
                               });
}

void FCapitalPresentation::add_initial_visual_instances() {
    auto const entities{simulation().get_entities()};
    auto const n_to_add{static_cast<int32>(entities.num())};
    add_visual_instances(0, n_to_add);
}

void FCapitalPresentation::add_visual_instances(int32 const first_index, int32 const n_to_add) {
    auto const entities{simulation().get_entities()};
    if (n_to_add == 0) {
        return;
    }

    auto const colour_cache{
        UTestTeamVisualData::build_team_colour_cache(actor_config->team_visual_data)};
    TArray<float> custom_data;
    custom_data.SetNumUninitialized(n_to_add * n_custom_ismc_floats, EAllowShrinking::No);
    auto const teams{entities.teams()};

    for (int32 i{0}; i < n_to_add; ++i) {
        auto const base{i * n_custom_ismc_floats};
        auto const& colour{colour_cache[ioj::to_unreal(teams[first_index + i])]};
        custom_data[base + 0] = colour.R;
        custom_data[base + 1] = colour.G;
        custom_data[base + 2] = colour.B;
    }

    auto const locations{entities.view_locations()};
    TArray<FTransform> transforms;
    transforms.Reserve(n_to_add);
    auto const rotations{entities.view_rotations()};
    auto const pitches{rotations.pitches()};
    auto const yaws{rotations.yaws()};
    auto const rolls{rotations.rolls()};
    for (int32 index{first_index}; index < first_index + n_to_add; ++index) {
        transforms.Emplace(
            FRotator{ioj::to_unreal(::ioj::Rotator3f{pitches[index], yaws[index], rolls[index]})},
            FVector{ioj::to_unreal(::ioj::vector_at(locations, index))});
    }
    constexpr bool return_indices{false};
    constexpr bool update_navigation{false};
    instances->AddInstances(transforms, return_indices, is_world_space, update_navigation);
    constexpr bool mark_render_state_dirty{false};
    instances->SetCustomData(
        first_index, first_index + n_to_add - 1, custom_data, mark_render_state_dirty);
}

void FCapitalPresentation::trigger_death_effects(ioj::sim::EntityLedger const& ledger) {
    auto const deaths{ledger.get_frame_deaths(ioj::sim::EntityType::CapitalShip)};
    auto const history{ledger.get_unique_entities()};
    auto const death_ticks{history.death_ticks()};
    auto const death_locations{history.death_locations()};
    auto const n{static_cast<int32>(deaths.size())};
    auto* const small_death_explosion{actor_config->small_death_explosion.Get()};
    auto* const main_death_explosion{actor_config->main_death_explosion.Get()};
    auto const has_small_death_explosion{IsValid(small_death_explosion)};
    auto const has_main_death_explosion{IsValid(main_death_explosion)};

    if ((!has_small_death_explosion || actor_config->presentation.n_small_explosions <= 0) &&
        !has_main_death_explosion) {
        return;
    }
    if (!niagara_spawner) {
        UE_LOG(LogSandbox,
               Warning,
               TEXT("FCapitalPresentation::trigger_death_effects: niagara_spawner is nullptr"));
        return;
    }

    auto const n_small_explosions{actor_config->presentation.n_small_explosions};
    auto const time_between_explosions{actor_config->presentation.time_between_explosions};
    auto const min_range{actor_config->presentation.min_small_explosion_range};
    auto const max_range{actor_config->presentation.max_small_explosion_range};
    auto main_explosion_delay{actor_config->presentation.large_explosion_delay};
    if (actor_config->main_explosion_delay_mode ==
            ECapitalShipMainExplosionDelayMode::AfterSmallExplosions &&
        has_small_death_explosion && n_small_explosions > 1) {
        main_explosion_delay += n_small_explosions * (n_small_explosions - 1);
    }

    TArray<UNiagaraSystem*> spawn_systems;
    TArray<FVector> spawn_locations;
    TArray<float> spawn_delays;
    ioj::reserve(n * (n_small_explosions + 1), spawn_systems, spawn_locations, spawn_delays);

    float current_delay{0.f};
    std::optional<ioj::sim::SimTick> previous_tick;
    for (auto const row : deaths) {
        if (previous_tick != death_ticks[row]) {
            current_delay = 0.f;
            previous_tick = death_ticks[row];
        }
        FVector const base_location{ioj::to_unreal(death_locations[row])};
        if (has_small_death_explosion) {
            for (int32 explosion_index{0}; explosion_index < n_small_explosions;
                 ++explosion_index) {
                if (explosion_index > 0) {
                    current_delay += time_between_explosions;
                }
                auto const offset{FVector{
                    FMath::FRandRange(min_range.x(), max_range.x()),
                    FMath::FRandRange(min_range.y(), max_range.y()),
                    FMath::FRandRange(min_range.z(), max_range.z()),
                }};
                spawn_systems.Add(small_death_explosion);
                spawn_locations.Add(base_location + offset);
                spawn_delays.Add(current_delay);
            }
        }

        if (has_main_death_explosion) {
            spawn_systems.Add(main_death_explosion);
            spawn_locations.Add(base_location);
            spawn_delays.Add(main_explosion_delay);
        }
    }

    niagara_spawner->add_spawns(spawn_systems, spawn_locations, spawn_delays);
}

void FCapitalPresentation::validate_array_sizes() const {
    simulation().get_entities().validate();
    ioj::fatal_if_nums_not_equal({
        SANDBOX_NAMED_NUM(static_cast<int32>(simulation().get_num_instances())),
        SANDBOX_NAMED_NUM(instances->GetNumInstances()),
    });
}
