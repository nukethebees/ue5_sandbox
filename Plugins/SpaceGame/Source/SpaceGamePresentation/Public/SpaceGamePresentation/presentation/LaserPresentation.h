#pragma once

#include <sandbox/simulation/laser_source.h>

#include <SpaceGamePresentation/presentation/LevelActorSettings.h>
#include <SpaceGamePresentation/support/DrawDebugConfig.h>
#include <SpaceGameSimulation/simulation/SystemReadViews.h>

#include <CoreMinimal.h>
#include <SpaceGamePresentation/entities/TeamColours.h>

class USandboxISMCComponent;
class FLaserPresentationIndexingTest;
class FSparkEffects;

struct SPACEGAMEPRESENTATION_API FLaserPresentation {
    friend struct FLevelPresentation;
    friend class FLaserPresentationIndexingTest;
  public:
    static constexpr int32 n_custom_ismc_floats{5};

    explicit FLaserPresentation(USandboxISMCComponent& component);

    auto get_config() const noexcept -> FLaserProjectileConfig const* { return actor_config; }
    void set_actor_config(FLaserProjectileConfig const* new_config) noexcept {
        actor_config = new_config;
    }
    void set_spark_effects(FSparkEffects& effects) noexcept { spark_effects_ = &effects; }
  private:
    auto view() const -> FLaserReadView const& { return view_; }

    void clear_runtime_state_presentation();
    void begin_play_presentation();
    void update_visual_data();
    void end_tick_presentation();

    void configure_ismc();
    void synchronize_material_data();
    void update_ismc();
    void queue_hit_sparks();
    void validate_array_sizes() const;

    FLaserProjectileConfig const* actor_config{nullptr};

    struct FMaterialData {
        FVector3f colour;
        float initial_lifetime;
        float spawn_time;
    };

    USandboxISMCComponent* instances{nullptr};
    TArray<FMaterialData> material_data;

    FSparkEffects* spark_effects_{nullptr};

#if WITH_EDITORONLY_DATA
    FDrawDebugConfig debug_drawer;

    bool debugging_shapes_enabled{false};
#endif

    FLaserReadView view_{};
    FTeamColours player_colours_;
    FTeamColours fighter_colours_;
    FTeamColours turret_colours_;
    auto source_colour(ml::simulation::LaserSource source) const -> FLinearColor;
};
