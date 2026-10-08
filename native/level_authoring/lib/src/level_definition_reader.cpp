#include <ioj/levels/authoring/level_definition_reader.h>

#include "ast_parser.h"

#include <array>
#include <format>

namespace ioj::levels::authoring {
namespace {
using detail::AstParser;

auto vector(AstParser& parser, s7::NodeIndex const node, std::string const& path) -> ml::Vector3d {
    auto const values{parser.list(node, path)};
    if (values.size() != 3) {
        parser.error(DiagnosticCode::InvalidType, path, "Expected three numeric components");
        return {};
    }
    return {parser.number(values[0], path + "[0]"),
            parser.number(values[1], path + "[1]"),
            parser.number(values[2], path + "[2]")};
}

auto camera(AstParser& parser, s7::NodeIndex const node, std::string const& path)
    -> LevelCameraDefinition {
    LevelCameraDefinition result;
    auto const fields{parser.record(node, RecordKind::Camera, path)};
    for (auto const& field : fields) {
        switch (field.property) {
            case Property::LookAt:
                result.target_entity_ids = parser.ids<EntityId>(field.value, field.node_path);
                break;
            case Property::Distance:
                result.distance = parser.number(field.value, field.node_path);
                break;
            case Property::OffsetDirection:
                result.offset_direction = vector(parser, field.value, field.node_path);
                break;
            default:
                parser.unknown(field);
                break;
        }
    }
    for (auto const property : {Property::LookAt, Property::Distance, Property::OffsetDirection}) {
        parser.require(fields, property, path);
    }
    return result;
}

auto grid(AstParser& parser, s7::NodeIndex const node, std::string const& path)
    -> LevelCollisionGridDefinition {
    LevelCollisionGridDefinition result;
    auto const fields{parser.record(node, RecordKind::CollisionGrid, path)};
    for (auto const& field : fields) {
        switch (field.property) {
            case Property::LevelSize:
                result.level_size = vector(parser, field.value, field.node_path);
                break;
            case Property::CellSize:
                result.cell_size = vector(parser, field.value, field.node_path);
                break;
            default:
                parser.unknown(field);
                break;
        }
    }
    if (fields.empty()) {
        parser.error(DiagnosticCode::MissingProperty,
                     path,
                     "Collision grid requires a level-size or cell-size");
    }
    return result;
}

auto mission(AstParser& parser, s7::NodeIndex const node, std::string const& path)
    -> LevelMissionDefinition {
    LevelMissionDefinition result;
    auto const fields{parser.record(node, RecordKind::Mission, path)};
    for (auto const& field : fields) {
        switch (field.property) {
            case Property::Mode: {
                auto const mode{parser.symbol(field.value, field.node_path)};
                auto const parsed{try_parse_serialized_level_mission_mode(mode)};
                if (!parsed || *parsed == LevelMissionMode::Unspecified) {
                    parser.error(DiagnosticCode::UnknownSymbol,
                                 field.node_path,
                                 std::format("Unknown mission mode '{}'", mode));
                } else {
                    result.mode = *parsed;
                }
                break;
            }
            case Property::TimeLimit:
                result.time_limit_seconds = parser.float_number(field.value, field.node_path);
                break;
            case Property::KillCount:
                result.kill_count = parser.integer(field.value, field.node_path);
                break;
            case Property::Heroes:
                result.hero_entity_ids = parser.ids<EntityId>(field.value, field.node_path);
                break;
            case Property::MustSurvive:
                result.must_survive_entity_ids = parser.ids<EntityId>(field.value, field.node_path);
                break;
            case Property::RequiredKills:
                result.required_kill_entity_ids =
                    parser.ids<EntityId>(field.value, field.node_path);
                break;
            default:
                parser.unknown(field);
                break;
        }
    }
    parser.require(fields, Property::Mode, path);
    return result;
}

auto mission_event(AstParser& parser, s7::NodeIndex const node, std::string const& path)
    -> LevelMissionObjectiveEvent {
    LevelMissionObjectiveEvent result;
    auto const fields{parser.record(node, RecordKind::MissionEvent, path)};
    for (auto const& field : fields) {
        switch (field.property) {
            case Property::At:
                result.time_seconds = parser.number(field.value, field.node_path);
                break;
            case Property::AddMustSurvive:
                result.must_survive_entity_ids = parser.ids<EntityId>(field.value, field.node_path);
                break;
            case Property::AddRequiredKills:
                result.required_kill_entity_ids =
                    parser.ids<EntityId>(field.value, field.node_path);
                break;
            case Property::IncreaseKillCount:
                result.kill_target_increase = parser.integer(field.value, field.node_path);
                break;
            default:
                parser.unknown(field);
                break;
        }
    }
    parser.require(fields, Property::At, path);
    return result;
}

auto entity(AstParser& parser, s7::NodeIndex const node, std::string const& path)
    -> EntitySpawnDefinition {
    EntitySpawnDefinition result;
    auto const fields{parser.record(node, RecordKind::Entity, path)};
    for (auto const& field : fields) {
        switch (field.property) {
            case Property::Id:
                result.id = EntityId{parser.symbol(field.value, field.node_path)};
                break;
            case Property::Archetype:
                result.archetype = parser.symbol(field.value, field.node_path);
                break;
            case Property::Team:
                result.team = TeamId{parser.symbol(field.value, field.node_path)};
                break;
            case Property::Position:
                result.position = vector(parser, field.value, field.node_path);
                break;
            case Property::Rotation: {
                auto const components{vector(parser, field.value, field.node_path)};
                result.rotation = {components.x, components.y, components.z};
                break;
            }
            case Property::SpawnAt:
                result.spawn_time_seconds = parser.number(field.value, field.node_path);
                break;
            default:
                parser.unknown(field);
                break;
        }
    }
    for (auto const property : {Property::Id,
                                Property::Archetype,
                                Property::Team,
                                Property::Position,
                                Property::Rotation}) {
        parser.require(fields, property, path);
    }
    return result;
}

auto unlocks(AstParser& parser,
             s7::Ast const& ast,
             s7::NodeIndex const node,
             std::string const& path) -> std::vector<LevelId> {
    std::vector<LevelId> result;
    auto const criteria{parser.list(node, path)};
    if (criteria.empty()) {
        parser.error(DiagnosticCode::MissingValue, path, "Unlock requires at least one criterion");
    }
    auto const count{criteria.size()};
    for (std::size_t index{}; index < count; ++index) {
        auto const location{std::format("{}[{}]", path, index)};
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

auto parse_level(s7::Ast const& ast) -> LevelDefinitionReadResult {
    AstParser parser{ast};
    LevelDefinition result;
    auto const fields{parser.record(ast.root, RecordKind::Level, "level")};
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
            case Property::Teams:
                result.teams = parser.ids<TeamId>(field.value, field.node_path);
                break;
            case Property::Player:
                result.player_entity_id = EntityId{parser.symbol(field.value, field.node_path)};
                break;
            case Property::Camera:
                result.camera = camera(parser, field.value, field.node_path);
                break;
            case Property::CollisionGrid:
                result.collision_grid = grid(parser, field.value, field.node_path);
                break;
            case Property::Mission:
                result.mission = mission(parser, field.value, field.node_path);
                break;
            case Property::MissionEvents: {
                auto const values{parser.list(field.value, field.node_path)};
                auto const count{values.size()};
                for (std::size_t index{}; index < count; ++index) {
                    result.mission_events.push_back(mission_event(
                        parser, values[index], std::format("{}[{}]", field.node_path, index)));
                }
                break;
            }
            case Property::Entities: {
                auto const values{parser.list(field.value, field.node_path)};
                auto const count{values.size()};
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
    return result;
}
}
