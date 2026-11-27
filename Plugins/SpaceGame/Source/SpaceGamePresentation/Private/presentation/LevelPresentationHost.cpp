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

void FLevelPresentationHost::initialise(FLevelVisualConfig const& config,
                                        TOptional<FPlayerPresentationResources> player,
                                        FGameAudioFacade audio,
                                        presentation::LevelInit const& level,
                                        presentation::LevelFrame const& frame) {
    check(!presentation_.IsSet());
    check(components_.is_valid());

    auto const resources{make_resources(config, MoveTemp(player))};
    presentation_.Emplace(resources, frame, audio);
    components_.collision_grid->set_static_bounds(level.static_bounds);
    components_.collision_grid->update_entity_bounds(frame.entity_bounds);
    hud_manager_.initialise(frame, level.radii, config, components_.soft_targets);
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

auto FLevelPresentationHost::make_resources(FLevelVisualConfig const& config,
                                            TOptional<FPlayerPresentationResources> player) const
    -> FLevelPresentationResources {
    return {.lasers = components_.lasers,
            .capital_ships = components_.capital_ships,
            .fighters = components_.fighters,
            .turrets = components_.turrets,
            .spinners = components_.spinners,
            .sparks = components_.sparks,
            .config = config,
            .player = MoveTemp(player)};
}

} // namespace ioj
