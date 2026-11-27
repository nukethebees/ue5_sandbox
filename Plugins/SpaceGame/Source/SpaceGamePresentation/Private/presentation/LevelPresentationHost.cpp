#include <SpaceGamePresentation/presentation/LevelPresentationHost.h>

#include <SpaceGameRendering/SparkRendererComponent.h>
#include <SpaceGameSimulation/support/logging/SandboxLogCategories.h>

#include <Components/InstancedStaticMeshComponent.h>
#include <SandboxISMCComponent.h>

namespace ioj {
auto FLevelPresentationComponents::is_valid() const -> bool {
    return IsValid(lasers) && IsValid(capital_ships) && IsValid(fighters) && IsValid(turrets) &&
           IsValid(spinners) && IsValid(sparks) && IsValid(soft_targets) && IsValid(collision_grid);
}

void FLevelPresentationHost::bind_components(FLevelPresentationComponents const components) {
    check(!presentation_.IsSet());
    check(components.is_valid());
    components_ = components;
}

void FLevelPresentationHost::configure_collision_grid(
    TOptional<FCollisionGridVisualizationSettings> settings) {
    if (!IsValid(components_.collision_grid)) {
        UE_LOG(LogSandbox,
               Error,
               TEXT("Cannot configure collision visualization: component is invalid"));
        return;
    }
    components_.collision_grid->configure(MoveTemp(settings));
}

void FLevelPresentationHost::initialise(FLevelVisualConfig config,
                                        TOptional<FPlayerPresentationResources> player,
                                        FGameAudioFacade audio,
                                        sim::collision::world_aabbs::ConstView static_bounds,
                                        sim::EntityTypeRadii const& radii,
                                        presentation::LevelFrame const& frame) {
    check(!presentation_.IsSet());
    check(components_.is_valid());

    presentation_.Emplace(components_, MoveTemp(config), MoveTemp(player), frame, audio);
    components_.collision_grid->set_static_bounds(static_bounds);
    components_.collision_grid->update_entity_bounds(frame.entity_bounds);
    hud_manager_.initialise(frame, radii, presentation_->config, components_.soft_targets);
}

void FLevelPresentationHost::reset() {
    hud_manager_.deactivate();
    presentation_.Reset();

    check(components_.is_valid());
    components_.collision_grid->clear();
    components_.lasers->clear_instances();
    components_.fighters->clear_instances();
    components_.sparks->clear_sparks();
    for (auto* const component : {components_.capital_ships,
                                  components_.turrets,
                                  components_.spinners,
                                  components_.soft_targets}) {
        component->ClearInstances();
    }
}

void FLevelPresentationHost::tick(double const dt, presentation::LevelFrame const& frame) {
    if (!presentation_.IsSet()) {
        return;
    }

    components_.collision_grid->update_entity_bounds(frame.entity_bounds);
    hud_manager_.advance(dt, frame);
    presentation_->tick(static_cast<float>(dt), frame);
}

} // namespace ioj
