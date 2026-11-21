#pragma once
#include <ioj/levels/authoring/campaign_parser.h>
#include <ioj/levels/authoring/definition_catalog.h>
#include <ioj/levels/authoring/level_parser.h>
#include <ioj/levels/authoring/settings_parser.h>
#include <ioj/s7/ast_result.h>

#include <filesystem>

namespace ioj::s7 {
struct ModuleInput;
}

namespace ioj::levels::authoring {
using CatalogReadResult = std::expected<DefinitionCatalog, Diagnostics>;

// Each read evaluates the selected file and its imports in a fresh interpreter.
class DefinitionReader {
  public:
    DefinitionReader() = default;
    explicit DefinitionReader(std::filesystem::path script_library_root);
    // Files in <script root>/<level>/ import from the shared script root.
    [[nodiscard]] static auto for_level(std::filesystem::path const& file) -> DefinitionReader;
    // Resolve catalog entries relative to the catalog directory.
    [[nodiscard]] auto read_root_file(std::filesystem::path const& path) const -> CatalogReadResult;
    [[nodiscard]] auto read_settings_file(std::filesystem::path const& path,
                                          settings::Settings base = {}) const -> SettingsReadResult;
    [[nodiscard]] auto read_script_file(std::filesystem::path const& path,
                                        char const* export_name,
                                        s7::ModuleInput* module = nullptr) const
        -> std::expected<s7::Ast, Diagnostics>;
    // Standalone reads use this reader's import root, or the file's directory.
    [[nodiscard]] auto read_level_file(std::filesystem::path const& path,
                                       s7::ModuleInput* module = nullptr) const
        -> LevelDefinitionReadResult;
  private:
    std::filesystem::path script_library_root_{};
};

}
