#pragma once

#include <ioj/levels/authoring/definition_reader.h>

#include <chrono>
#include <format>
#include <fstream>

namespace ioj::levels::authoring::tests {
class ModuleFixture {
  public:
    ModuleFixture(std::string_view name, std::string_view expression) {
        auto const temp{std::filesystem::temp_directory_path()};
        do {
            directory_ =
                temp / std::format("ioj-module-{}",
                                   std::chrono::steady_clock::now().time_since_epoch().count());
        } while (!std::filesystem::create_directory(directory_));
        path_ = directory_ / "fixture.scm";
        std::ofstream file{path_, std::ios::binary};
        file.exceptions(std::ios::badbit | std::ios::failbit);
        file << std::format("(export {})\n(define {} (let ()\n{}\n))\n", name, name, expression);
    }
    ~ModuleFixture() {
        std::error_code ignored;
        std::filesystem::remove_all(directory_, ignored);
    }
    auto path() const -> std::filesystem::path const& { return path_; }
  private:
    std::filesystem::path directory_;
    std::filesystem::path path_;
};

inline auto read_test_level(DefinitionReader const& reader, std::string_view source) {
    return reader.read_level_file(ModuleFixture{"mission", source}.path());
}
inline auto read_test_settings(DefinitionReader const& reader,
                               std::string_view source,
                               settings::Settings base = {}) {
    return reader.read_settings_file(ModuleFixture{"game-settings", source}.path(),
                                     std::move(base));
}
inline auto read_test_value(DefinitionReader const& reader, std::string_view source) {
    return reader.read_script_file(ModuleFixture{"value", source}.path(), "value");
}
inline auto read_test_campaign(DefinitionReader const& reader, std::string_view source)
    -> CampaignDefinitionReadResult {
    return reader.read_script_file(ModuleFixture{"campaign", source}.path(), "campaign")
        .and_then([](s7::Ast const& ast) { return parse_campaign(ast, ast.root); });
}
}
