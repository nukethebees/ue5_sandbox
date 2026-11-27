#pragma once

#include <ioj/presentation/level_init.h>
#include <SpaceGamePresentation/presentation/HUDManager.h>
#include <SpaceGamePresentation/presentation/LevelPresentation.h>
#include <SpaceGamePresentation/simulation/CollisionGridVisualizationComponent.h>

namespace ioj {
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
    void initialise(FLevelVisualConfig config,
                    TOptional<FPlayerPresentationResources> player,
                    FGameAudioFacade audio,
                    sim::collision::world_aabbs::ConstView static_bounds,
                    sim::EntityTypeRadii const& radii,
                    presentation::LevelFrame const& frame);
    void reset();
    void tick(double dt, presentation::LevelFrame const& frame);

    auto components() const -> FLevelPresentationComponents const& { return components_; }
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
