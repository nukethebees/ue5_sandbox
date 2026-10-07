#pragma once
#include <ioj/levels/diagnostics.h>
#include <SpaceGame/levels/LevelDefinition.h>

#include <expected>

namespace ioj::levels::authoring {
using FLevelDefinitionReadResult = std::expected<ml::FLevelDefinition, Diagnostics>;
class SPACEGAMES7_API FLevelDefinitionReader {
  public:
    FLevelDefinitionReader() = default;
    explicit FLevelDefinitionReader(FString script_library_root);
    [[nodiscard]] auto read_source(FStringView source) const -> FLevelDefinitionReadResult;
    [[nodiscard]] auto read_file(FStringView path) const -> FLevelDefinitionReadResult;
  private:
    FString script_library_root_{};
};
}
