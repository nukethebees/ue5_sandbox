#pragma once
#include <ioj/levels/authoring/level_inputs.h>
#include <ioj/levels/level_definition.h>

#include <string>

namespace ioj::s7 {
struct Ast;
}

namespace ioj::levels::authoring {
[[nodiscard]] auto make_entity_input(EntitySpawnDefinition const& entity) -> EntityInput;
// Keep the explicit settings fields from the decoded editor scene across exports.
[[nodiscard]] auto emit_editor_level_source(LevelInput const& level,
                                            s7::Ast const* saved_scene = nullptr) -> std::string;
}
