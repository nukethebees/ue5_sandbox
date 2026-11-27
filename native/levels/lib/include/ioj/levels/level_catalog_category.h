#pragma once

namespace ioj::levels {
class LevelDefinition;
}

namespace ioj::levels::authoring {
enum class LevelCatalogCategory { Playable, BattleViewer, Benchmark };

[[nodiscard]] auto catalog_category(LevelDefinition const& definition) noexcept
    -> LevelCatalogCategory;
}
