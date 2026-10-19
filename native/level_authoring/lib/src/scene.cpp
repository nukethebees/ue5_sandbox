#include <ioj/levels/authoring/scene_reconciliation.h>

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <numbers>
#include <unordered_map>

namespace ioj::levels::authoring {
namespace scene_comparison {
auto near(double const lhs, double const rhs) -> bool {
    return std::abs(lhs - rhs) <= 0.001;
}
auto same_number(double const lhs, double const rhs) -> bool {
    return lhs == rhs || (std::isnan(lhs) && std::isnan(rhs));
}
auto same_vector(Vector3d const& lhs, Vector3d const& rhs) -> bool {
    return same_number(lhs.x(), rhs.x()) && same_number(lhs.y(), rhs.y()) &&
           same_number(lhs.z(), rhs.z());
}
auto near_vector(Vector3d const& lhs, Vector3d const& rhs) -> bool {
    return near(lhs.x(), rhs.x()) && near(lhs.y(), rhs.y()) && near(lhs.z(), rhs.z());
}
auto quaternion(Rotator3d const r) -> std::array<double, 4> {
    auto const half_radians{std::numbers::pi / 360.0};
    auto const cp{std::cos(r.pitch * half_radians)};
    auto const sp{std::sin(r.pitch * half_radians)};
    auto const cy{std::cos(r.yaw * half_radians)};
    auto const sy{std::sin(r.yaw * half_radians)};
    auto const cr{std::cos(r.roll * half_radians)};
    auto const sr{std::sin(r.roll * half_radians)};
    return {cr * sp * sy - sr * cp * cy,
            -cr * sp * cy - sr * cp * sy,
            cr * cp * sy - sr * sp * cy,
            cr * cp * cy + sr * sp * sy};
}
auto same_rotation(Rotator3d const lhs, Rotator3d const rhs) -> bool {
    auto const a{quaternion(lhs)};
    auto const b{quaternion(rhs)};
    bool equal{true};
    bool negated{true};
    for (std::size_t i{}; i < a.size(); ++i) {
        equal = equal && near(a[i], b[i]);
        negated = negated && near(a[i], -b[i]);
    }
    return equal || negated;
}
auto operator==(LevelMissionDefinition const& lhs, LevelMissionDefinition const& rhs) -> bool {
    auto const same_time{
        lhs.time_limit_seconds.has_value() == rhs.time_limit_seconds.has_value() &&
        (!lhs.time_limit_seconds || same_number(*lhs.time_limit_seconds, *rhs.time_limit_seconds))};
    return lhs.mode == rhs.mode && same_time && lhs.kill_count == rhs.kill_count &&
           std::ranges::is_permutation(lhs.hero_entity_ids, rhs.hero_entity_ids) &&
           std::ranges::is_permutation(lhs.must_survive_entity_ids, rhs.must_survive_entity_ids) &&
           std::ranges::is_permutation(lhs.required_kill_entity_ids, rhs.required_kill_entity_ids);
}

auto operator==(LevelCameraDefinition const& lhs, LevelCameraDefinition const& rhs) -> bool {
    return std::ranges::is_permutation(lhs.target_entity_ids, rhs.target_entity_ids) &&
           same_vector(lhs.offset_direction, rhs.offset_direction) &&
           same_number(lhs.distance, rhs.distance);
}

auto operator==(std::optional<Vector3d> const& lhs, std::optional<Vector3d> const& rhs) -> bool {
    return lhs.has_value() == rhs.has_value() && (!lhs || same_vector(*lhs, *rhs));
}

// Keep observation equality local: std::optional's ordinary equality deliberately
// treats NaN as unequal, whereas an unchanged invalid observation is still current.
template <typename T>
auto operator==(std::optional<T> const& lhs, std::optional<T> const& rhs) -> bool {
    return lhs.has_value() == rhs.has_value() && (!lhs || *lhs == *rhs);
}

auto operator==(LevelCollisionGridDefinition const& lhs, LevelCollisionGridDefinition const& rhs)
    -> bool {
    return lhs.level_size == rhs.level_size && lhs.cell_size == rhs.cell_size;
}

auto same_viewpoint(LevelDefinitionComponents const& lhs, LevelDefinitionComponents const& rhs)
    -> bool {
    return lhs.player_entity_id == rhs.player_entity_id && lhs.camera == rhs.camera;
}

auto operator==(EntitySpawnDefinition const& lhs, EntitySpawnDefinition const& rhs) -> bool {
    return lhs.id == rhs.id && lhs.archetype == rhs.archetype && lhs.team == rhs.team &&
           same_vector(lhs.position, rhs.position) &&
           same_number(lhs.rotation.pitch, rhs.rotation.pitch) &&
           same_number(lhs.rotation.yaw, rhs.rotation.yaw) &&
           same_number(lhs.rotation.roll, rhs.rotation.roll) &&
           same_number(lhs.spawn_time_seconds, rhs.spawn_time_seconds);
}

auto operator==(SceneEntityProperties const& lhs, SceneEntityProperties const& rhs) -> bool {
    return lhs.label == rhs.label && same_vector(lhs.scale, rhs.scale) &&
           lhs.class_matches == rhs.class_matches;
}

auto document_changes(SceneSnapshot const& scene,
                      LevelDefinitionComponents const& incoming,
                      bool const repair_invalid = true) -> SyncPlan {
    auto const& current{scene.definition};
    return {.collision_grid_changed =
                !(current.collision_grid.value_or(LevelCollisionGridDefinition{}) ==
                  incoming.collision_grid.value_or(LevelCollisionGridDefinition{})),
            .metadata_changed = current.metadata.id != incoming.metadata.id ||
                                current.metadata.title != incoming.metadata.title ||
                                current.metadata.description != incoming.metadata.description,
            .viewpoint_changed =
                (repair_invalid && !scene.viewpoint_valid) || !same_viewpoint(current, incoming),
            .mission_changed = (repair_invalid && !scene.mission_references_valid) ||
                               !(current.mission == incoming.mission)};
}
}

auto SyncPlan::count(SyncAction const action) const -> std::size_t {
    return static_cast<std::size_t>(std::ranges::count(changes, action, &SyncChange::action));
}
auto SyncPlan::has_changes() const -> bool {
    return !changes.empty() || collision_grid_changed || metadata_changed || viewpoint_changed ||
           mission_changed;
}

auto reconcile_scene(SceneSnapshot const& scene, LevelDefinition const& level) -> SyncPlan {
    using namespace scene_comparison;
    auto const& incoming{level};
    assert(scene.definition.entities.size() == scene.properties.size());

    auto const& current_document{scene.definition};
    auto result{SyncPlan{
        .collision_grid_changed =
            !(current_document.collision_grid.value_or(LevelCollisionGridDefinition{}) ==
              incoming.collision_grid().value_or(LevelCollisionGridDefinition{})),
        .metadata_changed =
            current_document.metadata.id != incoming.metadata().id ||
            current_document.metadata.title != incoming.metadata().title ||
            current_document.metadata.description != incoming.metadata().description,
        .viewpoint_changed = !scene.viewpoint_valid ||
                             current_document.player_entity_id != incoming.player_entity_id() ||
                             !(current_document.camera == incoming.camera()),
        .mission_changed =
            !scene.mission_references_valid || !(current_document.mission == incoming.mission())}};
    std::unordered_map<EntityId, std::size_t> existing;
    auto const count{scene.definition.entities.size()};
    for (std::size_t i{}; i < count; ++i) {
        [[maybe_unused]] auto const inserted{
            existing.emplace(scene.definition.entities[i].id, i).second};
        assert(inserted);
    }
    auto const incoming_count{incoming.entities().size()};
    for (std::size_t incoming_index{}; incoming_index < incoming_count; ++incoming_index) {
        auto const& entity{incoming.entities()[incoming_index]};
        auto const found{existing.find(entity.id)};
        if (found == existing.end()) {
            result.changes.push_back({entity.id, SyncAction::Add, true, true, incoming_index});
            continue;
        }
        auto const index{found->second};
        auto const& current{scene.definition.entities[index]};
        auto const& properties{scene.properties[index]};
        if (current.archetype != entity.archetype || !properties.class_matches) {
            result.changes.push_back({entity.id, SyncAction::Replace, true, true, incoming_index});
        } else {
            auto const configure{current.team != entity.team ||
                                 !near_vector(current.position, entity.position) ||
                                 !same_rotation(current.rotation, entity.rotation) ||
                                 !near_vector(properties.scale, {1.0, 1.0, 1.0})};
            auto const relabel{properties.label != entity.id.value};
            if (configure || relabel ||
                !near(current.spawn_time_seconds, entity.spawn_time_seconds)) {
                result.changes.push_back(
                    {entity.id, SyncAction::Update, configure, relabel, incoming_index});
            }
        }
        existing.erase(found);
    }
    for (auto const& entity : scene.definition.entities) {
        if (existing.contains(entity.id)) {
            result.changes.push_back({entity.id, SyncAction::Remove});
        }
    }
    return result;
}

auto operator==(SceneSnapshot const& lhs, SceneSnapshot const& rhs) -> bool {
    using namespace scene_comparison;
    assert(lhs.definition.entities.size() == lhs.properties.size());
    assert(rhs.definition.entities.size() == rhs.properties.size());
    if (lhs.viewpoint_valid != rhs.viewpoint_valid ||
        lhs.mission_references_valid != rhs.mission_references_valid ||
        document_changes(lhs, rhs.definition, false).has_changes() ||
        lhs.definition.entities.size() != rhs.definition.entities.size()) {
        return false;
    }
    auto const equal{[](auto const& a, auto const& b) { return a == b; }};
    return std::ranges::equal(lhs.definition.entities, rhs.definition.entities, equal) &&
           std::ranges::equal(lhs.properties, rhs.properties, equal);
}
}
