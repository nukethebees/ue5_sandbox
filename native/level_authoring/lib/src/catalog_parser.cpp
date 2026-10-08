#include <ioj/levels/authoring/catalog_parser.h>

#include "ast_parser.h"
#include <ioj/levels/authoring/campaign_parser.h>
#include <ioj/levels/authoring/level_parser.h>

#include <cassert>
#include <optional>

namespace ioj::levels::authoring {
namespace {
template <typename Definition, typename Parse>
auto parse_entry(s7::Ast const& ast,
                 s7::NodeIndex const node,
                 std::string const& node_path,
                 std::filesystem::path const& source_root,
                 Parse parse) -> DefinitionEntry<Definition> {
    detail::AstParser parser{ast};
    std::filesystem::path source_path;
    std::optional<s7::NodeIndex> value;
    std::optional<std::string> evaluation_error;
    auto const fields{parser.record(node, RecordKind::IncludedDefinition, node_path)};
    for (auto const& field : fields) {
        switch (field.property) {
            case Property::Path: {
                auto const text{parser.string(field.value, field.node_path)};
                auto const relative{std::filesystem::path{std::u8string{text.begin(), text.end()}}};
                if (relative.empty() || relative.has_root_path() ||
                    std::ranges::any_of(relative, [](auto const& part) { return part == ".."; })) {
                    parser.error(DiagnosticCode::InvalidType,
                                 field.node_path,
                                 "Definition paths must be relative to the root directory");
                } else {
                    source_path = source_root / relative;
                }
                break;
            }
            case Property::Value:
                value = field.value;
                break;
            case Property::Error:
                evaluation_error = parser.string(field.value, field.node_path);
                break;
            default:
                parser.unknown(field);
                break;
        }
    }
    parser.require(fields, Property::Path, node_path);
    if (value.has_value() == evaluation_error.has_value()) {
        parser.error(DiagnosticCode::MissingValue,
                     node_path,
                     "An included definition must contain either a value or an evaluation error");
    }

    std::expected<Definition, Diagnostics> result{std::unexpect};
    if (!parser.errors().empty()) {
        result = std::unexpected{parser.take_errors()};
    } else if (evaluation_error) {
        result = std::unexpected{Diagnostics{
            {DiagnosticCode::ScriptEvaluationFailed, node_path, std::move(*evaluation_error)}}};
    } else {
        assert(value.has_value());
        result = parse(ast, *value);
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
    detail::AstParser parser{ast};
    DefinitionCatalog result;
    auto const fields{parser.record(ast.root, RecordKind::Catalog, "catalog")};
    for (auto const& field : fields) {
        if (field.property != Property::Levels && field.property != Property::Campaigns) {
            parser.unknown(field);
            continue;
        }
        auto const entries{parser.list(field.value, field.node_path)};
        auto const count{entries.size()};
        if (field.property == Property::Levels) {
            result.levels.reserve(count);
        } else {
            result.campaigns.reserve(count);
        }
        for (std::size_t index{}; index < count; ++index) {
            auto const node_path{std::format("{}[{}]", field.node_path, index)};
            switch (field.property) {
                case Property::Levels:
                    result.levels.emplace_back(parse_entry<LevelDefinition>(
                        ast, entries[index], node_path, source_root, parse_level));
                    break;
                case Property::Campaigns:
                    result.campaigns.emplace_back(parse_entry<CampaignDefinition>(
                        ast, entries[index], node_path, source_root, parse_campaign));
                    break;
                default:
                    std::unreachable();
            }
        }
    }
    if (!parser.errors().empty()) {
        return std::unexpected{parser.take_errors()};
    }
    return result;
}
}
