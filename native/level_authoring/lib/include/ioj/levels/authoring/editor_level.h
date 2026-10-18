#pragma once

#include <ioj/levels/level_definition.h>

#include <utility>

namespace ioj::levels::authoring {
// Own a semantically valid level supported by the authoring editor. Edit a copy and
// check it again at the input boundary; consumers only receive a const definition.
class EditorLevel {
  public:
    [[nodiscard]] static auto make(LevelDefinition definition)
        -> std::expected<EditorLevel, Diagnostics>;
    [[nodiscard]] auto definition() const -> LevelDefinition const& { return definition_; }
  private:
    explicit EditorLevel(LevelDefinition definition)
        : definition_{std::move(definition)} {}
    LevelDefinition definition_;
};
}
