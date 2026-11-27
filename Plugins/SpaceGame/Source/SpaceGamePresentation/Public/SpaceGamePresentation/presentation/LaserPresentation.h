#pragma once

#include <ioj/presentation/level_frame.h>
#include <SpaceGamePresentation/entities/TeamColours.h>
#include <SpaceGamePresentation/presentation/LevelActorSettings.h>
#include <SpaceGamePresentation/support/DrawDebugConfig.h>

#include <CoreMinimal.h>

namespace ioj {
class USandboxISMCComponent;
struct FLevelPresentation;
} // namespace ioj
class FSandboxISMCInstanceChunkWriter;
class FLaserPresentationIndexingTest;
class FSparkEffects;

namespace ioj {
struct SPACEGAMEPRESENTATION_API FLaserPresentation {
    friend struct FLevelPresentation;
    friend class ::FLaserPresentationIndexingTest;
  public:
    static constexpr int32 n_custom_ismc_floats{5};

    explicit FLaserPresentation(USandboxISMCComponent& component);

    auto get_config() const noexcept -> FLaserProjectileConfig const* { return actor_config; }
    void set_actor_config(FLaserProjectileConfig const* new_config) noexcept {
        actor_config = new_config;
    }
    void set_spark_effects(FSparkEffects& effects) noexcept { spark_effects_ = &effects; }
  private:
    void clear_runtime_state_presentation();
    void begin_play_presentation(presentation::LaserView const& lasers, int32 reserve);
    void end_tick_presentation();

    void configure_ismc();
    void fill_chunk(presentation::LaserView const& lasers,
                    FSandboxISMCInstanceChunkWriter& chunk) const;
    void update_ismc(presentation::LaserView const& lasers);
    void queue_hit_sparks(presentation::laser_hit_details::ConstView impacts,
                          sim::SimTick tick,
                          std::span<sim::FrameEventCounts const> ticks);

    FLaserProjectileConfig const* actor_config{nullptr};

    USandboxISMCComponent* instances{nullptr};
    TArray<int32> visible_indices_;

    FSparkEffects* spark_effects_{nullptr};

#if WITH_EDITORONLY_DATA
    FDrawDebugConfig debug_drawer;

    bool debugging_shapes_enabled{false};
#endif

    FTeamColours player_colours_;
    FTeamColours fighter_colours_;
    FTeamColours turret_colours_;
    auto source_colour(sim::LaserSource source) const -> FLinearColor;
};

} // namespace ioj
