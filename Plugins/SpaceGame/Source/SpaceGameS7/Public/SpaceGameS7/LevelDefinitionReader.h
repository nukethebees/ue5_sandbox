#pragma once

#include <SpaceGame/levels/LevelDefinition.h>

namespace ml::s7 {
struct SPACEGAMES7_API FLevelDefinitionDecodeError {
    FString path{};
    FString message{};
};

struct SPACEGAMES7_API FLevelDefinitionReadResult {
    TOptional<FLevelDefinition> definition{};
    FString script_error{};
    TArray<FLevelDefinitionDecodeError> decode_errors{};
    TArray<FLevelValidationError> validation_errors{};

    explicit operator bool() const noexcept { return definition.IsSet(); }
};

class SPACEGAMES7_API FLevelDefinitionReader final {
  public:
    [[nodiscard]] auto read_source(FStringView source) const -> FLevelDefinitionReadResult;
    [[nodiscard]] auto read_file(FStringView path) const -> FLevelDefinitionReadResult;
};
}
