#pragma once

#include <ioj/presentation/mission_frame.h>
#include <ioj/presentation/mission_init.h>
#include <SpaceGamePresentation/presentation/HUDManager.h>
#include <SpaceGamePresentation/presentation/LevelPresentation.h>
#include <SpaceGamePresentation/simulation/CollisionGridVisualizationComponent.h>

namespace ioj {
struct SPACEGAMEPRESENTATION_API FLevelPresentationComponents {
    auto is_valid() const -> bool;

    USandboxISMCComponent* lasers{};
    UInstancedStaticMeshComponent* capital_ships{};
    USandboxISMCComponent* fighters{};
    UInstancedStaticMeshComponent* turrets{};
    UInstancedStaticMeshComponent* spinners{};
    USparkRendererComponent* sparks{};
    UInstancedStaticMeshComponent* soft_targets{};
    UCollisionGridVisualizationComponent* collision_grid{};
};

// The actor retains reflected component ownership. Initialise after the native level finishes
// initialisation, and reset before destroying that level or the components.
class SPACEGAMEPRESENTATION_API FLevelPresentationHost {
  public:
    FLevelPresentationHost() = default;
    FLevelPresentationHost(FLevelPresentationHost const&) = delete;
    FLevelPresentationHost(FLevelPresentationHost&&) = delete;
    auto operator=(FLevelPresentationHost const&) -> FLevelPresentationHost& = delete;
    auto operator=(FLevelPresentationHost&&) -> FLevelPresentationHost& = delete;

    void bind_components(FLevelPresentationComponents components);
    void configure_collision_grid(TOptional<FCollisionGridVisualizationSettings> settings);
    void initialise(FLevelVisualConfig const& config,
                    TOptional<FPlayerPresentationResources> player,
                    FGameAudioFacade audio,
                    presentation::MissionInit const& mission,
                    presentation::MissionFrame const& frame, sim::LevelReadAccess const& view);
    void reset();
    void tick(double dt, presentation::MissionFrame const& frame, sim::LevelReadAccess const& view);

    auto make_resources(FLevelVisualConfig const& config,
                        TOptional<FPlayerPresentationResources> player) const
        -> FLevelPresentationResources;
    auto get_hud_manager() -> FHUDManager& { return hud_manager_; }
    auto get_presentation() const -> FLevelPresentation const* {
        return presentation_.IsSet() ? &presentation_.GetValue() : nullptr;
    }

  private:
    FLevelPresentationComponents components_;
    FHUDManager hud_manager_;
    TOptional<FLevelPresentation> presentation_;
};

} // namespace ioj
