#pragma once
#include <ioj/levels/level_definition.h>
#include <ioj/s7/ast.h>

#include <filesystem>
#include <string_view>

namespace ioj::levels::authoring {
using LevelDefinitionReadResult = std::expected<LevelDefinition, Diagnostics>;
// Parse structure and types only. Call validate_level before publishing to consumers.
[[nodiscard]] auto parse_level(s7::Ast const& ast) -> LevelDefinitionReadResult;
// Source/file entry points evaluate, release the interpreter, parse, then validate.
class LevelDefinitionReader {
  public:
    LevelDefinitionReader() = default;
    explicit LevelDefinitionReader(std::filesystem::path script_library_root);
    [[nodiscard]] auto read_source(std::string_view source) const -> LevelDefinitionReadResult;
    [[nodiscard]] auto read_file(std::filesystem::path const& path) const
        -> LevelDefinitionReadResult;
  private:
    std::filesystem::path script_library_root_{};
};
}
