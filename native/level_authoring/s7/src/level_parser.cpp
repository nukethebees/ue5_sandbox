#include <ioj/levels/authoring/level_parser.h>

#include "ast_parser.h"
#include "mission_parser.h"
#include <ioj/levels/authoring/settings_parser.h>
#include <ioj/levels/level_camera_definition_reader.h>
#include <ioj/levels/level_collision_grid_definition_reader.h>

#include <array>
#include <format>
#include <unordered_set>

namespace ioj::levels::authoring {
namespace {
using detail::AstParser;

auto grid(AstParser& parser, s7::NodeIndex const node, std::string const& node_path)
    -> LevelCollisionGridDefinition {
    auto result{parser.read<LevelCollisionGridDefinition>(node, node_path)};
    if (!result.level_size && !result.cell_size) {
        parser.error(DiagnosticCode::MissingProperty,
                     node_path,
                     "Collision grid requires a level-size or cell-size");
    }
    return result;
}

auto entity(AstParser& parser, s7::NodeIndex const node, std::string const& node_path)
    -> EntitySpawnDefinition {
    EntitySpawnDefinition result;
    auto const fields{parser.record(node, RecordKind::Entity, node_path)};
    for (auto const& field : fields) {
        switch (field.property) {
            case Property::Id:
                result.id = parser.identifier<EntityId>(field.value, field.node_path);
                break;
            case Property::Archetype:
                result.archetype = parser.archetype(field.value, field.node_path);
                break;
            case Property::Team:
                result.team = parser.team(field.value, field.node_path);
                break;
            case Property::Position:
                result.position = parser.read<Vector3d>(field.value, field.node_path);
                break;
            case Property::Rotation: {
                auto const components{parser.read<Vector3d>(field.value, field.node_path)};
                result.rotation = {components.x(), components.y(), components.z()};
                break;
            }
            case Property::SpawnAt:
                result.spawn_time_seconds = parser.number(field.value, field.node_path);
                break;
            case Property::Health:
                result.overrides.health = parser.integer(field.value, field.node_path);
                break;
            case Property::LaserDamage:
                result.overrides.laser_damage = parser.integer(field.value, field.node_path);
                break;
            case Property::InitialSpawnDelay:
                result.overrides.initial_spawn_delay =
                    parser.float_number(field.value, field.node_path);
                break;
            case Property::SpawnCooldown:
                result.overrides.spawn_cooldown = parser.float_number(field.value, field.node_path);
                break;
            case Property::Target:
                result.overrides.target_entity_id =
                    parser.identifier<EntityId>(field.value, field.node_path);
                break;
            default:
                parser.unknown(field);
                break;
        }
    }
    for (auto const property : {Property::Archetype, Property::Team, Property::Position}) {
        parser.require(fields, property, node_path);
    }
    return result;
}

void assign_generated_entity_ids(std::vector<EntitySpawnDefinition>& entities) {
    std::unordered_set<EntityId> used_ids;
    used_ids.reserve(entities.size());
    for (auto const& entity : entities) {
        if (!entity.id.empty()) {
            used_ids.insert(entity.id);
        }
    }

    std::array<std::array<std::size_t, EnumTraits<EntityArchetype>::values.size()>,
               EnumTraits<Team>::values.size()>
        next_indices{};
    for (auto& entity : entities) {
        if (!entity.id.empty()) {
            continue;
        }

        auto& index{next_indices[static_cast<std::size_t>(entity.team)]
                                [static_cast<std::size_t>(entity.archetype)]};
        // Reserve explicit IDs before generating names, including IDs on later entities.
        do {
            entity.id = EntityId{std::format("{}-{}-{}",
                                             to_serialized_string(entity.team),
                                             to_serialized_string(entity.archetype),
                                             index++)};
        } while (!used_ids.insert(entity.id).second);
    }
}

auto unlocks(AstParser& parser,
             s7::Ast const& ast,
             s7::NodeIndex const node,
             std::string const& node_path) -> std::vector<LevelId> {
    std::vector<LevelId> result;
    auto const criteria{parser.list(node, node_path)};
    if (criteria.empty()) {
        parser.error(
            DiagnosticCode::MissingValue, node_path, "Unlock requires at least one criterion");
    }
    auto const count{criteria.size()};
    for (std::size_t index{}; index < count; ++index) {
        auto const location{std::format("{}[{}]", node_path, index)};
        auto const values{parser.list(criteria[index], location)};
        if (values.size() != 2 || ast.node(values[0]).kind != s7::NodeKind::Keyword ||
            ast.text(values[0]) != to_serialized_string(RecordKind::LevelCompleted)) {
            parser.error(DiagnosticCode::ExpectedRecord,
                         location,
                         "Expected a level-completed criterion with one level id");
            continue;
        }
        result.emplace_back(parser.symbol(values[1], location + ".level-id"));
    }
    return result;
}
}

auto parse_level(s7::Ast const& ast, s7::NodeIndex const root) -> LevelDefinitionReadResult {
    AstParser parser{ast};
    LevelDefinitionComponents result;
    auto const fields{parser.record(root, RecordKind::Level, "level")};
    for (auto const& field : fields) {
        switch (field.property) {
            case Property::Id:
                result.metadata.id = LevelId{parser.symbol(field.value, field.node_path)};
                break;
            case Property::Title:
                result.metadata.title = parser.string(field.value, field.node_path);
                break;
            case Property::Description:
                result.metadata.description = parser.string(field.value, field.node_path);
                break;
            case Property::ParTime:
                result.metadata.par_time_seconds =
                    parser.float_number(field.value, field.node_path);
                break;
            case Property::Unlock:
                result.unlock_level_ids = unlocks(parser, ast, field.value, field.node_path);
                break;
            case Property::Player:
                result.player_entity_id = EntityId{parser.symbol(field.value, field.node_path)};
                break;
            case Property::Camera:
                result.camera = parser.read<LevelCameraDefinition>(field.value, field.node_path);
                break;
            case Property::CollisionGrid:
                result.collision_grid = grid(parser, field.value, field.node_path);
                break;
            case Property::Settings: {
                auto settings{parse_settings(ast, field.value)};
                if (settings) {
                    result.settings = std::move(*settings);
                } else {
                    for (auto& error : settings.error()) {
                        parser.error(error.code, error.node_path, std::move(error.message));
                    }
                }
                break;
            }
            case Property::Mission:
                result.mission = detail::parse_mission(parser, ast, field.value, field.node_path);
                break;
            case Property::Entities: {
                auto const values{parser.list(field.value, field.node_path)};
                auto const count{values.size()};
                result.entities.reserve(count);
                for (std::size_t index{}; index < count; ++index) {
                    result.entities.push_back(entity(
                        parser, values[index], std::format("{}[{}]", field.node_path, index)));
                }
                break;
            }
            default:
                parser.unknown(field);
                break;
        }
    }
    parser.require(fields, Property::Id, "level");
    parser.require(fields, Property::Title, "level");
    if (!parser.errors().empty()) {
        return std::unexpected{parser.take_errors()};
    }
    assign_generated_entity_ids(result.entities);
    return LevelDefinition::make(std::move(result));
}
}
