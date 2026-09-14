#pragma once

#include <ioj/sim/levels/level_definition.h>

#include <filesystem>
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
    std::optional<::ioj::sim::levels::LevelDefinition> definition{};
    std::string script_error{};
    std::vector<LevelDefinitionDecodeError> decode_errors{};
    std::vector<::ioj::sim::levels::LevelValidationError> validation_errors{};

    explicit operator bool() const noexcept { return definition.has_value(); }
};

class LevelDefinitionReader final {
  public:
    LevelDefinitionReader() = default;
    explicit LevelDefinitionReader(std::string script_library_root);

    [[nodiscard]] auto read_file(std::filesystem::path const& path) const
        -> LevelDefinitionReadResult;
    [[nodiscard]] auto read_source(std::string_view source) const -> LevelDefinitionReadResult;
  private:
    std::string script_library_root_{};
};
} // namespace ml::level_authoring
