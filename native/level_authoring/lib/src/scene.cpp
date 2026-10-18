#include <ioj/levels/authoring/scene.h>

#include <ioj/levels/authoring/level_definition_writer.h>

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <numbers>
#include <unordered_map>

namespace ioj::levels::authoring {
namespace {
auto near(double const lhs, double const rhs) -> bool {
    return std::abs(lhs - rhs) <= 0.001;
}
auto same_number(double const lhs, double const rhs) -> bool {
    return lhs == rhs || (std::isnan(lhs) && std::isnan(rhs));
}
auto same_vector(ml::Vector3d const& lhs, ml::Vector3d const& rhs) -> bool {
    return same_number(lhs.x(), rhs.x()) && same_number(lhs.y(), rhs.y()) &&
           same_number(lhs.z(), rhs.z());
}
auto near_vector(ml::Vector3d const& lhs, ml::Vector3d const& rhs) -> bool {
    return near(lhs.x(), rhs.x()) && near(lhs.y(), rhs.y()) && near(lhs.z(), rhs.z());
}
auto quaternion(ioj::Rotator3d const r) -> std::array<double, 4> {
    auto const half_radians{std::numbers::pi / 360.0};
    auto const cp{std::cos(r.pitch * half_radians)}, sp{std::sin(r.pitch * half_radians)};
    auto const cy{std::cos(r.yaw * half_radians)}, sy{std::sin(r.yaw * half_radians)};
    auto const cr{std::cos(r.roll * half_radians)}, sr{std::sin(r.roll * half_radians)};
    return {cr * sp * sy - sr * cp * cy,
            -cr * sp * cy - sr * cp * sy,
            cr * cp * sy - sr * sp * cy,
            cr * cp * cy + sr * sp * sy};
}
auto same_rotation(ioj::Rotator3d const lhs, ioj::Rotator3d const rhs) -> bool {
    auto const a{quaternion(lhs)}, b{quaternion(rhs)};
    bool equal{true}, negated{true};
    for (std::size_t i{}; i < a.size(); ++i) {
        equal = equal && near(a[i], b[i]);
        negated = negated && near(a[i], -b[i]);
    }
    return equal || negated;
}
auto same_ids(std::vector<EntityId> lhs, std::vector<EntityId> rhs) -> bool {
    std::ranges::sort(lhs);
    std::ranges::sort(rhs);
    return lhs == rhs;
}
auto same_mission(std::optional<LevelMissionDefinition> const& lhs,
                  std::optional<LevelMissionDefinition> const& rhs) -> bool {
    if (!lhs || !rhs) {
        return lhs.has_value() == rhs.has_value();
    }
    auto const same_time{lhs->time_limit_seconds.has_value() ==
                             rhs->time_limit_seconds.has_value() &&
                         (!lhs->time_limit_seconds ||
                          same_number(*lhs->time_limit_seconds, *rhs->time_limit_seconds))};
    return lhs->mode == rhs->mode && same_time && lhs->kill_count == rhs->kill_count &&
           same_ids(lhs->hero_entity_ids, rhs->hero_entity_ids) &&
           same_ids(lhs->must_survive_entity_ids, rhs->must_survive_entity_ids) &&
           same_ids(lhs->required_kill_entity_ids, rhs->required_kill_entity_ids);
}
auto same_viewpoint(LevelDefinition const& lhs, LevelDefinition const& rhs) -> bool {
    if (lhs.player_entity_id != rhs.player_entity_id ||
        lhs.camera.has_value() != rhs.camera.has_value()) {
        return false;
    }
    return !lhs.camera ||
           (same_ids(lhs.camera->target_entity_ids, rhs.camera->target_entity_ids) &&
            same_vector(lhs.camera->offset_direction, rhs.camera->offset_direction) &&
            same_number(lhs.camera->distance, rhs.camera->distance));
}
auto same_grid(std::optional<LevelCollisionGridDefinition> const& lhs,
               std::optional<LevelCollisionGridDefinition> const& rhs) -> bool {
    auto const a{lhs.value_or(LevelCollisionGridDefinition{})};
    auto const b{rhs.value_or(LevelCollisionGridDefinition{})};
    auto const same_optional_vector{[](auto const& x, auto const& y) {
        return x.has_value() == y.has_value() && (!x || same_vector(*x, *y));
    }};
    return same_optional_vector(a.level_size, b.level_size) &&
           same_optional_vector(a.cell_size, b.cell_size);
}
auto document_changes(SceneSnapshot const& scene,
                      LevelDefinition const& incoming,
                      bool const repair_invalid = true) -> SyncPlan {
    auto const& current{scene.definition};
    return {.collision_grid_changed = !same_grid(current.collision_grid, incoming.collision_grid),
            .metadata_changed = current.metadata.id != incoming.metadata.id ||
                                current.metadata.title != incoming.metadata.title ||
                                current.metadata.description != incoming.metadata.description,
            .viewpoint_changed =
                (repair_invalid && !scene.viewpoint_valid) || !same_viewpoint(current, incoming),
            .mission_changed = (repair_invalid && !scene.mission_references_valid) ||
                               !same_mission(current.mission, incoming.mission)};
}
}

auto SyncPlan::count(SyncAction const action) const -> std::size_t {
    return static_cast<std::size_t>(std::ranges::count(changes, action, &SyncChange::action));
}
auto SyncPlan::has_changes() const -> bool {
    return !changes.empty() || collision_grid_changed || metadata_changed || viewpoint_changed ||
           mission_changed;
}

auto reconcile_scene(SceneSnapshot const& scene, LevelDefinition const& incoming)
    -> std::expected<SyncPlan, Diagnostics> {
    assert(scene.definition.entities.size() == scene.properties.size());
    auto validation{validate_editor_level(incoming)};
    if (!validation) {
        return std::unexpected{std::move(validation.error())};
    }

    auto result{document_changes(scene, incoming)};
    std::unordered_map<EntityId, std::size_t> existing;
    auto const count{scene.definition.entities.size()};
    for (std::size_t i{}; i < count; ++i) {
        auto const inserted{existing.emplace(scene.definition.entities[i].id, i).second};
        assert(inserted);
    }
    auto const incoming_count{incoming.entities.size()};
    for (std::size_t incoming_index{}; incoming_index < incoming_count; ++incoming_index) {
        auto const& entity{incoming.entities[incoming_index]};
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

auto same_scene(SceneSnapshot const& lhs, SceneSnapshot const& rhs) -> bool {
    assert(lhs.definition.entities.size() == lhs.properties.size());
    assert(rhs.definition.entities.size() == rhs.properties.size());
    if (lhs.viewpoint_valid != rhs.viewpoint_valid ||
        lhs.mission_references_valid != rhs.mission_references_valid ||
        document_changes(lhs, rhs.definition, false).has_changes() ||
        lhs.definition.entities.size() != rhs.definition.entities.size()) {
        return false;
    }
    auto const count{lhs.definition.entities.size()};
    for (std::size_t i{}; i < count; ++i) {
        auto const& a{lhs.definition.entities[i]};
        auto const& b{rhs.definition.entities[i]};
        auto const& x{lhs.properties[i]};
        auto const& y{rhs.properties[i]};
        if (a.id != b.id || a.archetype != b.archetype || a.team != b.team ||
            !same_vector(a.position, b.position) ||
            !same_number(a.rotation.pitch, b.rotation.pitch) ||
            !same_number(a.rotation.yaw, b.rotation.yaw) ||
            !same_number(a.rotation.roll, b.rotation.roll) ||
            !same_number(a.spawn_time_seconds, b.spawn_time_seconds) || x.label != y.label ||
            !same_vector(x.scale, y.scale) || x.class_matches != y.class_matches) {
            return false;
        }
    }
    return true;
}
}
