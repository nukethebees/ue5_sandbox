#include <ioj/levels/authoring/level_definition_writer.h>

#include <ioj/levels/authoring/definition_reader.h>
#include <ioj/levels/authoring/editor_support.h>
#include <ioj/levels/authoring/grammar.h>

#include <codegen/sexpr/form_builder.h>
#include <codegen/sexpr/sexpr_renderer.h>
#include <lispb/strings.h>

#include <algorithm>
#include <cmath>
#include <format>
#include <numeric>

namespace ioj::levels::authoring {
namespace {
using lispb::codegen::sexpr::Dialect;
using lispb::codegen::sexpr::Expr;
using lispb::codegen::sexpr::SExprIR;
using lispb::codegen::sexpr::SExprRenderer;

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

auto vector(SExprIR& ir, Vector3d const value) -> Expr {
    return ir.quote(ir.list({ir.atom(editor_number(value.x())),
                             ir.atom(editor_number(value.y())),
                             ir.atom(editor_number(value.z()))}));
}
auto vector(SExprIR& ir, Rotator3d const value) -> Expr {
    return vector(ir, Vector3d{value.pitch, value.yaw, value.roll});
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

auto evaluated_value(SExprIR& ir, s7::Ast const& ast, s7::NodeIndex const node) -> Expr {
    switch (ast.node(node).kind) {
        case s7::NodeKind::List:
            return ir.list(ast.children(node),
                           [&](auto child) { return evaluated_value(ir, ast, child); });
        case s7::NodeKind::Symbol:
            return ir.atom(ast.text(node));
        case s7::NodeKind::Keyword:
            return ir.atom(std::format(":{}", ast.text(node)));
        case s7::NodeKind::String:
            return ir.string(ast.text(node));
        case s7::NodeKind::Boolean:
            return ast.boolean(node) ? ir.atoms.true_value : ir.atoms.false_value;
        case s7::NodeKind::Integer:
            return ir.atom(ast.integer(node));
        case s7::NodeKind::Ratio: {
            auto const ratio{ast.ratio(node)};
            return ir.atom(std::format("{}/{}", ratio.numerator, ratio.denominator));
        }
        case s7::NodeKind::Real:
            return ir.atom(std::format("{}", ast.real(node)));
    }
    std::unreachable();
}

auto level_settings_node(s7::Ast const& ast, s7::NodeIndex const root)
    -> std::optional<s7::NodeIndex> {
    auto const fields{ast.children(root)};
    auto const count{fields.size()};
    for (std::size_t index{1}; index < count; index += 2) {
        if (ast.text(fields[index]) == "settings") {
            return fields[index + 1];
        }
    }
    return std::nullopt;
}
}

auto emit_evaluated_source(s7::Ast const& ast, s7::NodeIndex const node) -> std::string {
    lispb::Strings strings;
    SExprIR ir{strings};
    auto const expression{ir.quote(evaluated_value(ir, ast, node))};
    return SExprRenderer{ir, Dialect::scheme}.render(expression);
}

auto emit_editor_level_source(LevelDefinition const& level,
                              std::optional<std::string_view> const authored_source,
                              std::filesystem::path const& script_library_root)
    -> std::expected<std::string, Diagnostics> {
    auto support{check_editor_support(level)};
    if (!support) {
        return std::unexpected{std::move(support.error())};
    }

    std::optional<s7::Ast> source_ast;
    if (authored_source && !authored_source->empty()) {
        auto source{DefinitionReader{script_library_root}.read_source_ast(*authored_source)};
        if (!source) {
            return std::unexpected{std::move(source.error())};
        }
        auto valid{parse_level(*source, source->root, level.settings())};
        if (!valid) {
            return std::unexpected{std::move(valid.error())};
        }
        auto const fields{source->children(source->root)};
        if (source->text(fields.front()) != "level") {
            return std::unexpected{
                Diagnostics{{DiagnosticCode::UnsupportedEditorFeature,
                             "level",
                             "Canonical saving requires one evaluated level record"}}};
        }
        source_ast = std::move(*source);
    } else if (!authored_source && level.settings() != settings::Settings{}) {
        return std::unexpected{
            Diagnostics{{DiagnosticCode::UnsupportedEditorFeature,
                         "level.settings",
                         "Saving nondefault settings requires the authored level source"}}};
    }

    auto const& definition{level};
    lispb::Strings strings;
    SExprIR ir{strings};
    auto form{ir.form(RecordKind::Level)};
    form.field(Property::Id, ir.quote(definition.metadata().id.value));
    form.field(Property::Title, ir.string(definition.metadata().title));
    if (!definition.metadata().description.empty()) {
        form.field(Property::Description, ir.string(definition.metadata().description));
    }
    if (source_ast) {
        if (auto const node{level_settings_node(*source_ast, source_ast->root)}) {
            form.field(Property::Settings, ir.quote(evaluated_value(ir, *source_ast, *node)));
        }
    }
    if (!definition.player_entity_id().empty()) {
        form.field(Property::Player, ir.quote(definition.player_entity_id().value));
    } else {
        auto const& camera{*definition.camera()};
        auto camera_form{ir.form(RecordKind::Camera)};
        camera_form.field(Property::LookAt, id_list(ir, camera.target_entity_ids));
        camera_form.field(Property::Distance, ir.atom(editor_number(camera.distance)));
        camera_form.field(Property::OffsetDirection, vector(ir, camera.offset_direction));
        form.field(Property::Camera, camera_form.build());
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
            entities.add(ir.comment(std::format("Team: {:s} | Archetype: {:s} | Count: {}",
                                                entity.team,
                                                entity.archetype,
                                                group_end - position)));
        }
        auto entity_form{ir.form(RecordKind::Entity)};
        entity_form.field(Property::Id, ir.quote(entity.id.value));
        entity_form.field(Property::Archetype, ir.quote(entity.archetype));
        entity_form.field(Property::Team, ir.quote(entity.team));
        entity_form.field(Property::Position, vector(ir, entity.position));
        if (entity.rotation.pitch != 0.0 || entity.rotation.yaw != 0.0 ||
            entity.rotation.roll != 0.0) {
            entity_form.field(Property::Rotation, vector(ir, entity.rotation));
        }
        if (entity.spawn_time_seconds != 0.0) {
            entity_form.field(Property::SpawnAt, ir.atom(editor_number(entity.spawn_time_seconds)));
        }
        entities.add(entity_form.build());
    }
    form.field(Property::Entities, entities.build());

    return SExprRenderer{ir, Dialect::scheme}.render_document(
        {ir.comment(
             "Generated by Space Game Level Authoring. Saving rewrites canonical explicit S7."),
         form.build()});
}
}
