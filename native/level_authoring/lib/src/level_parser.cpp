#include <ioj/levels/authoring/level_parser.h>

#include "ast_parser.h"

#include <array>
#include <format>
#include <unordered_set>

namespace ioj::levels::authoring {
namespace {
using detail::AstParser;

auto vector(AstParser& parser, s7::NodeIndex const node, std::string const& node_path)
    -> ml::Vector3d {
    auto const values{parser.list(node, node_path)};
    if (values.size() != 3) {
        parser.error(DiagnosticCode::InvalidType, node_path, "Expected three numeric components");
        return {};
    }
    return {parser.number(values[0], node_path + "[0]"),
            parser.number(values[1], node_path + "[1]"),
            parser.number(values[2], node_path + "[2]")};
}

auto camera(AstParser& parser, s7::NodeIndex const node, std::string const& node_path)
    -> LevelCameraDefinition {
    LevelCameraDefinition result;
    auto const fields{parser.record(node, RecordKind::Camera, node_path)};
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
        parser.require(fields, property, node_path);
    }
    return result;
}

auto grid(AstParser& parser, s7::NodeIndex const node, std::string const& node_path)
    -> LevelCollisionGridDefinition {
    LevelCollisionGridDefinition result;
    auto const fields{parser.record(node, RecordKind::CollisionGrid, node_path)};
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
                     node_path,
                     "Collision grid requires a level-size or cell-size");
    }
    return result;
}

auto mission(AstParser& parser, s7::NodeIndex const node, std::string const& node_path)
    -> LevelMissionDefinition {
    LevelMissionDefinition result;
    auto const fields{parser.record(node, RecordKind::Mission, node_path)};
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
    parser.require(fields, Property::Mode, node_path);
    return result;
}

auto mission_event(AstParser& parser, s7::NodeIndex const node, std::string const& node_path)
    -> LevelMissionObjectiveEvent {
    LevelMissionObjectiveEvent result;
    auto const fields{parser.record(node, RecordKind::MissionEvent, node_path)};
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
    parser.require(fields, Property::At, node_path);
    return result;
}

auto entity(AstParser& parser, s7::NodeIndex const node, std::string const& node_path)
    -> EntitySpawnDefinition {
    EntitySpawnDefinition result;
    auto const fields{parser.record(node, RecordKind::Entity, node_path)};
    for (auto const& field : fields) {
        switch (field.property) {
            case Property::Id:
                result.id = EntityId{parser.symbol(field.value, field.node_path)};
                if (result.id.empty()) {
                    parser.error(DiagnosticCode::InvalidSymbol,
                                 field.node_path,
                                 "Entity id must not be empty");
                }
                break;
            case Property::Archetype:
                result.archetype = parser.archetype(field.value, field.node_path);
                break;
            case Property::Team:
                result.team = parser.team(field.value, field.node_path);
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

    std::array<std::array<std::size_t, ml::EnumTraits<EntityArchetype>::values.size()>,
               ml::EnumTraits<Team>::values.size()>
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
    LevelDefinition result;
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
                result.mission_events.reserve(count);
                for (std::size_t index{}; index < count; ++index) {
                    result.mission_events.push_back(mission_event(
                        parser, values[index], std::format("{}[{}]", field.node_path, index)));
                }
                break;
            }
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
    return result;
}
}
