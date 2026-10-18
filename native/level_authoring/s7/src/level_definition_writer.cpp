#include <ioj/levels/authoring/level_definition_writer.h>

#include <ioj/levels/authoring/editor_support.h>
#include <ioj/levels/authoring/grammar.h>

#include <codegen/sexpr/output.h>

#include <algorithm>
#include <cmath>
#include <format>
#include <numeric>

namespace ioj::levels::authoring {
namespace {
using lispb::codegen::sexpr::Dialect;
using lispb::codegen::sexpr::Expr;
using lispb::codegen::sexpr::SExprIR;

auto editor_number(double const value) -> std::string {
    if (std::abs(value) < 0.0005) {
        return "0";
    }
    auto result{std::format("{:.3f}", value)};
    while (result.ends_with('0')) {
        result.pop_back();
    }
    if (result.ends_with('.')) {
        result.pop_back();
    }
    return result == "-0" ? "0" : result;
}

auto symbol(SExprIR& ir, std::string_view const value) -> Expr {
    return ir.quote(ir.atom(value));
}

auto vector(SExprIR& ir, double const x, double const y, double const z) -> Expr {
    return ir.quote(
        ir.list({ir.atom(editor_number(x)), ir.atom(editor_number(y)), ir.atom(editor_number(z))}));
}

template <typename Id>
auto id_list(SExprIR& ir, std::vector<Id> ids) -> Expr {
    std::ranges::sort(ids);
    std::vector<Expr> values;
    values.reserve(ids.size());
    for (auto const& id : ids) {
        values.push_back(ir.atom(id.value));
    }
    return ir.quote(ir.list(values));
}
}

auto emit_editor_level_source(LevelDefinition const& level)
    -> std::expected<std::string, Diagnostics> {
    auto support{check_editor_support(level)};
    if (!support) {
        return std::unexpected{std::move(support.error())};
    }

    auto const& definition{level};
    SExprIR ir;
    auto form{ir.form(to_serialized_string(RecordKind::Level))};
    form.field(to_serialized_string(Property::Id), symbol(ir, definition.metadata().id.value));
    form.field(to_serialized_string(Property::Title), ir.string(definition.metadata().title));
    if (!definition.metadata().description.empty()) {
        form.field(to_serialized_string(Property::Description),
                   ir.string(definition.metadata().description));
    }
    if (definition.collision_grid() &&
        (definition.collision_grid()->level_size || definition.collision_grid()->cell_size)) {
        auto collision{ir.form(to_serialized_string(RecordKind::CollisionGrid))};
        if (definition.collision_grid()->level_size) {
            auto const size{*definition.collision_grid()->level_size};
            collision.field(to_serialized_string(Property::LevelSize),
                            vector(ir, size.x(), size.y(), size.z()));
        }
        if (definition.collision_grid()->cell_size) {
            auto const size{*definition.collision_grid()->cell_size};
            collision.field(to_serialized_string(Property::CellSize),
                            vector(ir, size.x(), size.y(), size.z()));
        }
        form.field(to_serialized_string(Property::CollisionGrid), collision.build());
    }
    if (!definition.player_entity_id().empty()) {
        form.field(to_serialized_string(Property::Player),
                   symbol(ir, definition.player_entity_id().value));
    } else {
        auto const& camera{*definition.camera()};
        auto camera_form{ir.form(to_serialized_string(RecordKind::Camera))};
        camera_form.field(to_serialized_string(Property::LookAt),
                          id_list(ir, camera.target_entity_ids));
        camera_form.field(to_serialized_string(Property::Distance),
                          ir.atom(editor_number(camera.distance)));
        camera_form.field(to_serialized_string(Property::OffsetDirection),
                          vector(ir,
                                 camera.offset_direction.x(),
                                 camera.offset_direction.y(),
                                 camera.offset_direction.z()));
        form.field(to_serialized_string(Property::Camera), camera_form.build());
    }
    if (definition.mission()) {
        auto const& mission{*definition.mission()};
        auto mission_form{ir.form(to_serialized_string(RecordKind::Mission))};
        mission_form.field(to_serialized_string(Property::Mode),
                           symbol(ir, to_serialized_string(mission.mode)));
        if (mission.time_limit_seconds) {
            mission_form.field(to_serialized_string(Property::TimeLimit),
                               ir.atom(editor_number(*mission.time_limit_seconds)));
        }
        if (mission.kill_count) {
            mission_form.field(to_serialized_string(Property::KillCount),
                               ir.atom(std::to_string(*mission.kill_count)));
        }
        if (!mission.hero_entity_ids.empty()) {
            mission_form.field(to_serialized_string(Property::Heroes),
                               id_list(ir, mission.hero_entity_ids));
        }
        if (!mission.must_survive_entity_ids.empty()) {
            mission_form.field(to_serialized_string(Property::MustSurvive),
                               id_list(ir, mission.must_survive_entity_ids));
        }
        if (!mission.required_kill_entity_ids.empty()) {
            mission_form.field(to_serialized_string(Property::RequiredKills),
                               id_list(ir, mission.required_kill_entity_ids));
        }
        form.field(to_serialized_string(Property::Mission), mission_form.build());
    }

    std::vector<std::size_t> indices(definition.entities().size());
    std::iota(indices.begin(), indices.end(), std::size_t{});
    std::ranges::sort(indices, [&](std::size_t const a, std::size_t const b) {
        auto const& left{definition.entities()[a]};
        auto const& right{definition.entities()[b]};
        if (left.team != right.team) {
            return to_serialized_string(left.team) < to_serialized_string(right.team);
        }
        auto const left_rank{static_cast<int>(left.archetype)};
        auto const right_rank{static_cast<int>(right.archetype)};
        return left_rank != right_rank ? left_rank < right_rank : left.id < right.id;
    });

    auto entities{ir.form("list")};
    auto const entity_count{indices.size()};
    std::size_t group_end{};
    for (std::size_t position{}; position < entity_count; ++position) {
        auto const index{indices[position]};
        auto const& entity{definition.entities()[index]};
        if (position == group_end) {
            group_end = position + 1;
            while (group_end < entity_count) {
                auto const& next{definition.entities()[indices[group_end]]};
                if (next.team != entity.team || next.archetype != entity.archetype) {
                    break;
                }
                ++group_end;
            }
            entities.add(ir.comment(std::format("Team: {} | Archetype: {} | Count: {}",
                                                to_serialized_string(entity.team),
                                                to_serialized_string(entity.archetype),
                                                group_end - position)));
        }
        auto entity_form{ir.form(to_serialized_string(RecordKind::Entity))};
        entity_form.field(to_serialized_string(Property::Id), symbol(ir, entity.id.value));
        entity_form.field(to_serialized_string(Property::Archetype),
                          symbol(ir, to_serialized_string(entity.archetype)));
        entity_form.field(to_serialized_string(Property::Team),
                          symbol(ir, to_serialized_string(entity.team)));
        entity_form.field(
            to_serialized_string(Property::Position),
            vector(ir, entity.position.x(), entity.position.y(), entity.position.z()));
        if (entity.rotation.pitch != 0.0 || entity.rotation.yaw != 0.0 ||
            entity.rotation.roll != 0.0) {
            entity_form.field(
                to_serialized_string(Property::Rotation),
                vector(ir, entity.rotation.pitch, entity.rotation.yaw, entity.rotation.roll));
        }
        if (entity.spawn_time_seconds != 0.0) {
            entity_form.field(to_serialized_string(Property::SpawnAt),
                              ir.atom(editor_number(entity.spawn_time_seconds)));
        }
        entities.add(entity_form.build());
    }
    form.field(to_serialized_string(Property::Entities), entities.build());

    return ir.render_document(
        {ir.comment(
             "Generated by Space Game Level Authoring. Saving rewrites canonical explicit S7."),
         form.build()},
        Dialect::scheme);
}
}
