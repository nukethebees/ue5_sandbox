#include <ioj/levels/authoring/level_definition_writer.h>

#include <ioj/levels/authoring/level_inputs_writer.h>
#include <ioj/s7/ast.h>

namespace ioj::levels::authoring {
auto make_entity_input(EntitySpawnDefinition const& entity) -> EntityInput {
    return {.id = entity.id,
            .archetype = entity.archetype,
            .team = entity.team,
            .position = entity.position,
            .rotation = {entity.rotation.pitch, entity.rotation.yaw, entity.rotation.roll},
            .spawn_at = entity.spawn_time_seconds,
            .health = entity.overrides.health,
            .laser_damage = entity.overrides.laser_damage,
            .initial_spawn_delay = entity.overrides.initial_spawn_delay,
            .spawn_cooldown = entity.overrides.spawn_cooldown,
            .initial_fire_point = entity.overrides.initial_fire_point};
}

namespace {
auto settings_form(s7::SExprIR& ir, s7::Ast const& ast, s7::NodeIndex node) -> s7::Expr {
    switch (ast.node(node).kind) {
        case s7::NodeKind::List: {
            auto children{ast.children(node)};
            if (!children.empty() && ast.node(children.front()).kind == s7::NodeKind::Keyword) {
                auto form{ir.form(ast.text(children.front()))};
                for (auto const child : children.subspan(1)) {
                    form.add(settings_form(ir, ast, child));
                }
                return form.build();
            }
            auto form{ir.form("list")};
            for (auto const child : children) {
                form.add(settings_form(ir, ast, child));
            }
            return form.build();
        }
        case s7::NodeKind::Keyword:
            return ir.atom(std::format(":{}", ast.text(node)));
        case s7::NodeKind::Symbol:
            return ir.form("string->symbol", {ir.string(ast.text(node))}).build();
        case s7::NodeKind::String:
            return ir.string(ast.text(node));
        case s7::NodeKind::Boolean:
            return s7::write(ir, ast.boolean(node));
        case s7::NodeKind::Integer:
            return ir.atom(ast.integer(node));
        case s7::NodeKind::Real:
            return s7::write(ir, ast.real(node));
        case s7::NodeKind::Ratio: {
            auto const ratio{ast.ratio(node)};
            return ir.atom(std::format("{}/{}", ratio.numerator, ratio.denominator));
        }
    }
    std::unreachable();
}
}

auto emit_editor_level_source(LevelInput const& level, s7::Ast const* saved_scene) -> std::string {
    lispb::Strings strings;
    s7::SExprIR ir{strings};
    auto form{ir.form("level")};
    s7::PropertyWriter<LevelInput> writer{ir, form, level, nullptr};
    writer("id", &LevelInput::id);
    writer("title", &LevelInput::title);
    writer("description", &LevelInput::description);
    writer("par-time", &LevelInput::par_time);
    writer("unlock", &LevelInput::unlock);
    writer("player", &LevelInput::player);
    writer("camera", &LevelInput::camera);
    writer("entities", &LevelInput::entities);
    auto settings{ir.form("settings").build()};
    if (saved_scene) {
        auto const fields{saved_scene->children(saved_scene->root)};
        for (std::size_t i{1}; i < fields.size(); i += 2) {
            if (saved_scene->text(fields[i]) == "settings") {
                settings = settings_form(ir, *saved_scene, fields[i + 1]);
                break;
            }
        }
    }
    form.field("settings", settings);
    return "(export scene)\n(define scene " +
           lispb::codegen::sexpr::SExprRenderer{ir, lispb::codegen::sexpr::Dialect::scheme}.render(
               form.build()) +
           ")\n";
}
}
