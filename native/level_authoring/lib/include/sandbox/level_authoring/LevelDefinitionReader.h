#pragma once

#include <sandbox/simulation/levels/LevelDefinition.h>

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace ml::level_authoring {
struct LevelDefinitionDecodeError {
    std::string path{};
    std::string message{};
};

struct LevelDefinitionReadResult {
    std::optional<LevelDefinition> definition{};
    std::string script_error{};
    std::vector<LevelDefinitionDecodeError> decode_errors{};
    std::vector<LevelValidationError> validation_errors{};

    explicit operator bool() const noexcept { return definition.has_value(); }
};

class LevelDefinitionReader final {
  public:
    LevelDefinitionReader() = default;
    explicit LevelDefinitionReader(std::string script_library_root);

    [[nodiscard]] auto read_source(std::string_view source) const -> LevelDefinitionReadResult;
  private:
    std::string script_library_root_{};
};
} // namespace ml::level_authoring
