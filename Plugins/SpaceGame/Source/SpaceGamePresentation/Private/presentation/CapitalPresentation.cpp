#include "SpaceGamePresentation/presentation/CapitalPresentation.h"

#include <ioj/column_math.h>
#include <ioj/presentation/capital_explosion_timing.h>
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

namespace ioj {
FCapitalPresentation::FCapitalPresentation(UInstancedStaticMeshComponent& component)
    : instances{&component} {}

void
    FCapitalPresentation::set_actor_config(FCapitalShipConfig const* const new_config,
                                           settings::CapitalPresentation const& settings) noexcept {
    actor_config = new_config;
    settings_ = &settings;
}

void FCapitalPresentation::set_niagara_spawner(FDelayedNiagaraSpawns& spawner) {
    niagara_spawner = &spawner;
}

void FCapitalPresentation::ValidateOptionalAssets() const {
    check(actor_config);

    if (settings_->n_small_explosions > 0 && !IsValid(actor_config->small_death_explosion)) {
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

void FCapitalPresentation::begin_play_presentation(presentation::AgentDisplayBatch const& entities,
                                                   presentation::rotators3f::ConstView rotations) {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::FCapitalPresentation::begin_play_presentation);

    auto* const world{instances->GetWorld()};
    if (!actor_config) {
        UE_LOG(LogSandbox, Fatal, TEXT("FCapitalPresentation actor_config is nullptr."));
    }
    fatal_if_uobject_ptrs_invalid({
        SANDBOX_NAMED_UOBJECT_PTR(world),
        SANDBOX_NAMED_UOBJECT_PTR(actor_config->mesh.Get()),
        SANDBOX_NAMED_UOBJECT_PTR(actor_config->team_visual_data.Get()),
    });

    configure_ismc();
    add_initial_visual_instances(entities, rotations);
    validate_array_sizes(static_cast<int32>(entities.num()));
}

void FCapitalPresentation::update_visual_data(presentation::InstanceChanges const& changes) {
    auto const colours{
        UTestTeamVisualData::build_team_colour_cache(actor_config->team_visual_data)};
    auto const count{changes.kinds.size()};
    for (std::uint32_t row{}; row < count; ++row) {
        if (changes.kinds[row] == sim::EntityFrameChangeKind::RemoveSwap) {
            instances->RemoveInstance(changes.indices[row]);
        } else {
            FTransform const transform{FRotator{to_unreal(changes.rotations[row])},
                                       FVector{to_unreal(changes.locations[row])}};
            auto const index{instances->AddInstance(transform, is_world_space)};
            check(index == static_cast<int32>(changes.indices[row]));
            auto const colour{colours[to_unreal(changes.teams[row])]};
            float const rgb[]{colour.R, colour.G, colour.B};
            instances->SetCustomData(index, rgb, false);
        }
    }
}

void FCapitalPresentation::commit_visual_data() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::FCapitalPresentation::commit_visual_data);

    instances->MarkRenderStateDirty();
}

void FCapitalPresentation::configure_ismc() {
    batch::configure_ismc(*instances,
                          {
                              .mesh = actor_config->mesh.Get(),
                              .material = actor_config->material.Get(),
                              .num_custom_data_floats = n_custom_ismc_floats,
                          });
}

void FCapitalPresentation::add_initial_visual_instances(
    presentation::AgentDisplayBatch const& entities,
    presentation::rotators3f::ConstView rotations) {
    auto const n_to_add{static_cast<int32>(entities.num())};

    if (n_to_add == 0) {
        return;
    }

    auto const colour_cache{
        UTestTeamVisualData::build_team_colour_cache(actor_config->team_visual_data)};
    TArray<float> custom_data;
    custom_data.SetNumUninitialized(n_to_add * n_custom_ismc_floats, EAllowShrinking::No);
    auto const teams{entities.teams};

    for (int32 i{0}; i < n_to_add; ++i) {
        auto const base{i * n_custom_ismc_floats};
        auto const& colour{colour_cache[to_unreal(teams[i])]};
        custom_data[base + 0] = colour.R;
        custom_data[base + 1] = colour.G;
        custom_data[base + 2] = colour.B;
    }

    auto const locations{entities.locations};
    TArray<FTransform> transforms;
    transforms.Reserve(n_to_add);
    auto const pitches{rotations.pitches};
    auto const yaws{rotations.yaws};
    auto const rolls{rotations.rolls};
    for (int32 index{}; index < n_to_add; ++index) {
        transforms.Emplace(
            FRotator{to_unreal(Rotator3f{pitches[index], yaws[index], rolls[index]})},
            FVector{to_unreal(locations[index])});
    }
    constexpr bool return_indices{false};
    constexpr bool update_navigation{false};
    instances->AddInstances(transforms, return_indices, is_world_space, update_navigation);
    constexpr bool mark_render_state_dirty{false};
    instances->SetCustomData(0, n_to_add - 1, custom_data, mark_render_state_dirty);
}

void FCapitalPresentation::trigger_death_effects(std::span<sim::FrameEventCounts const> ticks,
                                                 std::span<sim::EntityId const> victims,
                                                 std::span<Vector3f const> death_locations) {
    auto* const small_death_explosion{actor_config->small_death_explosion.Get()};
    auto* const main_death_explosion{actor_config->main_death_explosion.Get()};
    auto const has_small_death_explosion{IsValid(small_death_explosion)};
    auto const has_main_death_explosion{IsValid(main_death_explosion)};

    if ((!has_small_death_explosion || settings_->n_small_explosions <= 0) &&
        !has_main_death_explosion) {
        return;
    }

    auto const n_small_explosions{settings_->n_small_explosions};
    auto const time_between_explosions{settings_->time_between_explosions};
    auto const min_range{settings_->min_small_explosion_range};
    auto const max_range{settings_->max_small_explosion_range};
    auto const after_small_explosions{actor_config->main_explosion_delay_mode ==
                                      ECapitalShipMainExplosionDelayMode::AfterSmallExplosions};

    std::uint32_t first{};
    for (auto const counts : ticks) {
        float current_delay{};
        for (auto row{first}; row < counts.deaths; ++row) {
            if (victims[row].entity_type() != sim::EntityType::CapitalShip) {
                continue;
            }
            presentation::CapitalExplosionTiming const timing{
                current_delay,
                time_between_explosions,
                has_small_death_explosion ? n_small_explosions : 0};
            FVector const base_location{to_unreal(death_locations[row])};
            if (has_small_death_explosion) {
                for (int32 explosion_index{0}; explosion_index < n_small_explosions;
                     ++explosion_index) {
                    auto const offset{FVector{
                        FMath::FRandRange(min_range.x(), max_range.x()),
                        FMath::FRandRange(min_range.y(), max_range.y()),
                        FMath::FRandRange(min_range.z(), max_range.z()),
                    }};
                    niagara_spawner->add_spawn(small_death_explosion,
                                               base_location + offset,
                                               FRotator::ZeroRotator,
                                               FVector::OneVector,
                                               timing.small_delay(explosion_index));
                }
            }

            current_delay = timing.last_small_delay();
            if (has_main_death_explosion) {
                niagara_spawner->add_spawn(
                    main_death_explosion,
                    base_location,
                    FRotator::ZeroRotator,
                    FVector::OneVector,
                    timing.main_delay(settings_->large_explosion_delay, after_small_explosions));
            }
        }

        first = counts.deaths;
    }
}

void FCapitalPresentation::validate_array_sizes(int32 count) const {
    fatal_if_nums_not_equal({
        SANDBOX_NAMED_NUM(count),
        SANDBOX_NAMED_NUM(instances->GetNumInstances()),
    });
}

} // namespace ioj
