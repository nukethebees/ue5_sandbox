#include "SpaceGamePresentation/presentation/TurretPresentation.h"

#include <ioj/column_math.h>
#include <SpaceGamePresentation/entities/TestBatchActorCore.h>
#include <SpaceGamePresentation/entities/TestTeamConversion.h>
#include <SpaceGamePresentation/entities/TestTeamVisualData.h>
#include <SpaceGameSimulation/support/logging/SandboxLogCategories.h>

#include <SandboxCore/array_checks.h>
#include <SandboxCore/array_utils.h>
#include <SandboxCoreEngine/RotatorConversion.h>
#include <SandboxCoreEngine/uobject_utils.h>
#include <SandboxCoreEngine/VectorConversion.h>

#include <Components/InstancedStaticMeshComponent.h>
#include <Components/SceneComponent.h>
#include <Engine/StaticMesh.h>
#include <NiagaraFunctionLibrary.h>
#include <NiagaraSystem.h>

namespace ioj {
FTurretPresentation::FTurretPresentation(UInstancedStaticMeshComponent& component)
    : instances{&component} {}

void FTurretPresentation::set_actor_config(FTurretConfig const* const new_config,
                                           settings::TurretPresentation const& settings) noexcept {
    actor_config = new_config;
    settings_ = &settings;
}

void FTurretPresentation::ValidateOptionalAssets() const {
    check(actor_config);

    if (!IsValid(actor_config->death_effect)) {
        UE_LOG(LogSandbox,
               Warning,
               TEXT("FTurretPresentation optional asset is missing: death_effect"));
    }
}

void FTurretPresentation::clear_runtime_state_presentation() {
    instances->ClearInstances();
    ismc_transforms.Reset();
}

void FTurretPresentation::begin_play_presentation(presentation::AgentDisplayBatch const& entities,
                                                  presentation::rotators3f::ConstView rotations) {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::FTurretPresentation::begin_play_presentation);
    if (!actor_config) {
        UE_LOG(LogSandbox, Fatal, TEXT("FTurretPresentation actor_config is nullptr."));
    }
    fatal_if_uobject_ptrs_invalid({
        SANDBOX_NAMED_UOBJECT_PTR(instances),
        SANDBOX_NAMED_UOBJECT_PTR(actor_config->mesh.Get()),
        SANDBOX_NAMED_UOBJECT_PTR(actor_config->team_visual_data.Get()),
    });
    debug_drawer = actor_config->debug_drawer;
    debug_drawer.world = instances->GetWorld();

    configure_ismc();
    auto const count{static_cast<int32>(entities.num())};
    ismc_transforms.Reserve(count);
    auto const locations{entities.locations};
    auto const pitches{rotations.pitches};
    auto const yaws{rotations.yaws};
    auto const rolls{rotations.rolls};
    for (int32 i{}; i < count; ++i) {
        ismc_transforms.Emplace(FRotator{to_unreal(Rotator3f{pitches[i], yaws[i], rolls[i]})},
                                FVector{to_unreal(locations[i])});
    }
    add_initial_visual_instances(entities);
    validate_array_sizes(static_cast<int32>(entities.num()));
}

void FTurretPresentation::update_visual_data(presentation::InstanceChanges const& changes) {
    auto const colours{
        UTestTeamVisualData::build_team_colour_cache(actor_config->team_visual_data)};
    auto const count{changes.kinds.size()};
    for (std::uint32_t row{}; row < count; ++row) {
        if (changes.kinds[row] == sim::EntityFrameChangeKind::RemoveSwap) {
            ismc_transforms.RemoveAtSwap(changes.indices[row], EAllowShrinking::No);
            instances->RemoveInstance(changes.indices[row]);
        } else {
            FTransform const transform{FRotator{to_unreal(changes.rotations[row])},
                                       FVector{to_unreal(changes.locations[row])}};
            auto const index{instances->AddInstance(transform, is_world_space)};
            check(index == static_cast<int32>(changes.indices[row]));
            ismc_transforms.Add(transform);
            auto const colour{colours[to_unreal(changes.teams[row])]};
            float const rgb[]{colour.R, colour.G, colour.B};
            instances->SetCustomData(index, rgb, false);
        }
    }
}

void FTurretPresentation::commit_visual_data(presentation::AgentDisplayBatch const& entities,
                                             presentation::TargetView const& target_view) {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::FTurretPresentation::commit_visual_data);

    instances->MarkRenderStateDirty();
    if (draw_target_arrows_enabled || draw_debug_entity_info_enabled) {
        draw_debugging_shapes(entities, target_view);
    }
}

void FTurretPresentation::configure_ismc() {
    instances->SetMobility(EComponentMobility::Static);
    batch::configure_ismc(*instances,
                          {
                              .mesh = actor_config->mesh.Get(),
                              .num_custom_data_floats = n_custom_ismc_floats,
                          });
}

void FTurretPresentation::add_initial_visual_instances(
    presentation::AgentDisplayBatch const& entities) {
    auto const n_to_add{ismc_transforms.Num()};
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

    instances->AddInstances(ismc_transforms, false);
    instances->SetCustomData(0, n_to_add - 1, custom_data, false);
}

void FTurretPresentation::trigger_death_effects(std::span<sim::EntityId const> victims,
                                                std::span<Vector3f const> death_locations) {
    auto const n{static_cast<int32>(death_locations.size())};
    auto* world{instances->GetWorld()};
    auto* explosion_system{actor_config->death_effect.Get()};

    if (!IsValid(explosion_system)) {
        return;
    }

    FVector const scale{settings_->death_effect_scale};
    FVector const location_offset{actor_config->death_effect_offset};

    for (int32 i{0}; i < n; ++i) {

        if (victims[i].entity_type() != sim::EntityType::Turret) {
            continue;
        }
        constexpr bool auto_destroy{true};
        constexpr bool auto_activate{true};

        UNiagaraFunctionLibrary::SpawnSystemAtLocation(world,
                                                       explosion_system,
                                                       FVector{to_unreal(death_locations[i])} +
                                                           location_offset,
                                                       FRotator::ZeroRotator,
                                                       scale,
                                                       auto_destroy,
                                                       auto_activate,
                                                       ENCPoolMethod::AutoRelease);
    }
}

void FTurretPresentation::validate_array_sizes(int32 count) const {
    fatal_if_nums_not_equal({
        SANDBOX_NAMED_NUM(count),
        SANDBOX_NAMED_NUM(ismc_transforms),
        SANDBOX_NAMED_NUM(instances->GetNumInstances()),
    });
}

void FTurretPresentation::draw_debugging_shapes(presentation::AgentDisplayBatch const& entities,
                                                presentation::TargetView const& target_view) const {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::FTurretPresentation::draw_debugging_shapes);

    auto const n{static_cast<int32>(entities.num())};
    auto const text_offset{actor_config->debug_status_text_offset};

    auto& drawer{debug_drawer};
    auto const locations{entities.locations};
    auto const targets{target_view.ids};
    auto const target_locations{target_view.locations};
    auto const ids{entities.ids};

    for (int32 i{0}; i < n; ++i) {
        auto const turret_location{FVector{to_unreal(locations[i])}};

        if (draw_target_arrows_enabled) {
            auto const target_id{targets[i]};

            if (target_id.is_valid()) {
                auto const target_location{FVector{to_unreal(target_locations[i])}};
                drawer.draw_line(turret_location, target_location);
            }
        }

        if (draw_debug_entity_info_enabled) {
            auto const entity_id{ids[i]};

            auto const msg{FString::Printf(TEXT("[%u] HP=%d"),
                                           entity_id.raw_value(),
                                           entities.healths.health(entity_id).value())};
            auto const msg_location{turret_location + text_offset};
            drawer.draw_string(msg_location, msg);
        }
    }
}

} // namespace ioj
