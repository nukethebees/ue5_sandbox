#include <ioj/levels/authoring/catalog_parser.h>

#include "level_processing.h"
#include "reader_diagnostics.h"
#include <ioj/levels/authoring/generated/level_inputs_reader.h>

namespace ioj::levels::authoring {
namespace {
template <typename Definition, typename Input, typename Process>
auto process_entry(s7::ReadResult<Input> input,
                   std::string const& path,
                   std::filesystem::path const& source_root,
                   Process process) -> DefinitionEntry<Definition> {
    if (!input) {
        return {{}, std::unexpected{detail::diagnostics(std::move(input.error()))}};
    }
    auto const& text{input->path};
    auto const relative{std::filesystem::path{std::u8string{text.begin(), text.end()}}};
    if (relative.empty() || relative.has_root_path() ||
        std::ranges::any_of(relative, [](auto const& part) { return part == ".."; })) {
        return {{},
                std::unexpected{
                    Diagnostics{{DiagnosticCode::InvalidType,
                                 path + ".path",
                                 "Definition paths must be relative to the root directory"}}}};
    }
    auto source_path{source_root / relative};
    std::expected<Definition, Diagnostics> result{std::unexpect};
    if (input->value.has_value() == input->error.has_value()) {
        result = std::unexpected{Diagnostics{
            {DiagnosticCode::MissingValue,
             path,
             "An included definition must contain either a value or an evaluation error"}}};
    } else if (input->error) {
        result = std::unexpected{
            Diagnostics{{DiagnosticCode::ScriptEvaluationFailed, path, std::move(*input->error)}}};
    } else {
        result = std::move(*input->value).transform_error(detail::diagnostics).and_then(process);
    }
    if (!result) {
        for (auto& error : result.error()) {
            error.source_path = source_path;
        }
    }
    return {std::move(source_path), std::move(result)};
}
}

auto parse_catalog(s7::Ast const& ast, std::filesystem::path const& source_root)
    -> std::expected<DefinitionCatalog, Diagnostics> {
    auto input{s7::read<CatalogInput>(s7::Reader{ast}, ast.root, "catalog")};
    if (!input) {
        return std::unexpected{detail::diagnostics(std::move(input.error()))};
    }
    DefinitionCatalog result;
    auto const levels_count{input->levels.size()};
    for (std::size_t i{}; i < levels_count; ++i) {
        result.levels.push_back(process_entry<LevelDefinition>(std::move(input->levels[i]),
                                                               std::format("catalog.levels[{}]", i),
                                                               source_root,
                                                               detail::process_level));
    }
    auto const campaigns_count{input->campaigns.size()};
    for (std::size_t i{}; i < campaigns_count; ++i) {
        result.campaigns.push_back(process_entry<CampaignDefinition>(
            std::move(input->campaigns[i]),
            std::format("catalog.campaigns[{}]", i),
            source_root,
            [](CampaignDefinition definition) -> std::expected<CampaignDefinition, Diagnostics> {
                return definition;
            }));
    }
    return result;
}
}
