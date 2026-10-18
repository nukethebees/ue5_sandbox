#pragma once

#include <ioj/levels/diagnostics.h>
#include <ioj/levels/level_definition_components.h>

#include <optional>
#include <utility>
#include <vector>

namespace ioj::levels {
// Own a validated definition. Construction verifies values and cross-component references.
class LevelDefinition {
  public:
    [[nodiscard]] static auto make(LevelDefinitionComponents components)
        -> std::expected<LevelDefinition, Diagnostics>;

    [[nodiscard]] auto components() const -> LevelDefinitionComponents const& {
        return components_;
    }
  private:
    explicit LevelDefinition(LevelDefinitionComponents components)
        : components_{std::move(components)} {}
    LevelDefinitionComponents components_;
};

// Return distinct teams from initial and scheduled entities, in enum order.
[[nodiscard]] auto participating_teams(LevelDefinition const& definition) -> std::vector<Team>;

} // namespace ioj::levels
