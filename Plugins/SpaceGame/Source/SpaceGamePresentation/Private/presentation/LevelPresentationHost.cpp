#include <SpaceGamePresentation/presentation/LevelPresentationHost.h>

#include <ioj/sim/collision/spatial_query_manager.h>
#include <SpaceGameRendering/SparkRendererComponent.h>
#include <SpaceGameSimulation/support/logging/SandboxLogCategories.h>

#include <Components/InstancedStaticMeshComponent.h>
#include <SandboxISMCComponent.h>

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
                                        ioj::sim::LevelReadAccess const& view,
                                        ioj::sim::Mission const& mission,
                                        ioj::sim::SpatialQueryManager const& spatial_queries,
                                        TArray<FTransform> initial_turret_transforms) {
    check(!presentation_.IsSet());
    check(components_.is_valid());

    auto const resources{make_resources(config, MoveTemp(player))};
    presentation_.Emplace(resources, view, MoveTemp(initial_turret_transforms));
    update_collision_bounds(spatial_queries);
    hud_manager_.initialise(mission, view, spatial_queries, config, components_.soft_targets);
}

void FLevelPresentationHost::reset() {
    hud_manager_.deactivate();
    presentation_.Reset();

    check(components_.is_valid());
    components_.collision_grid->clear_collision_bounds();
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

void FLevelPresentationHost::tick(double const dt,
                                  ioj::sim::LevelReadAccess const& view,
                                  ioj::sim::SpatialQueryManager const& spatial_queries) {
    if (!presentation_.IsSet()) {
        return;
    }

    update_collision_bounds(spatial_queries);
    hud_manager_.advance(dt);
    presentation_->tick(static_cast<float>(dt), view);
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

void FLevelPresentationHost::update_collision_bounds(
    ioj::sim::SpatialQueryManager const& spatial_queries) {
    components_.collision_grid->update_collision_bounds(
        spatial_queries.get_entity_collision_bounds(),
        spatial_queries.get_static_collision_bounds());
}
