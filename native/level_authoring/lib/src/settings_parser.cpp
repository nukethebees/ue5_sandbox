#include <ioj/levels/authoring/settings_parser.h>

#include "ast_parser.h"

#include <format>
#include <unordered_set>

namespace ioj::levels::authoring {
namespace settings_parser {
void record(detail::AstParser& parser,
            s7::Ast const& ast,
            s7::NodeIndex const node,
            std::string const& prefix,
            settings::Settings& result) {
    auto const values{parser.list(node, prefix.empty() ? "settings" : prefix)};
    if (values.empty() || ast.node(values[0]).kind != s7::NodeKind::Keyword) {
        parser.error(DiagnosticCode::ExpectedRecord, prefix, "Expected a settings record");
        return;
    }

    auto const kind{ast.text(values[0])};
    if (kind == "settings-layers") {
        for (auto const layer : values.subspan(1)) {
            record(parser, ast, layer, prefix, result);
        }
        return;
    }

    auto const section{kind == "settings" || !prefix.empty() ? prefix : std::string{kind}};
    std::unordered_set<std::string_view> seen;
    auto const count{values.size()};
    for (std::size_t index{1}; index < count; index += 2) {
        auto const key{values[index]};
        if (ast.node(key).kind != s7::NodeKind::Keyword) {
            parser.error(DiagnosticCode::ExpectedKeyword, section, "Expected a settings keyword");
            continue;
        }
        auto const name{ast.text(key)};
        auto const path{section.empty() ? std::string{name} : std::format("{}.{}", section, name)};
        if (!seen.insert(name).second) {
            parser.error(DiagnosticCode::DuplicateProperty, path, "Duplicate settings property");
        }
        if (index + 1 == count) {
            parser.error(DiagnosticCode::MissingValue, path, "Settings property has no value");
            continue;
        }

        auto const value{values[index + 1]};
        switch (ast.node(value).kind) {
            case s7::NodeKind::Boolean:
                result.emplace_back(path, ast.boolean(value));
                break;
            case s7::NodeKind::Integer:
                result.emplace_back(path, ast.integer(value));
                break;
            case s7::NodeKind::Real:
            case s7::NodeKind::Ratio:
                result.emplace_back(path, parser.number(value, path));
                break;
            case s7::NodeKind::String:
            case s7::NodeKind::Symbol:
                result.emplace_back(path, std::string{ast.text(value)});
                break;
            case s7::NodeKind::List: {
                auto const children{ast.children(value)};
                if (!children.empty() && ast.node(children[0]).kind == s7::NodeKind::Keyword) {
                    record(parser, ast, value, path, result);
                } else {
                    std::vector<double> numbers;
                    numbers.reserve(children.size());
                    for (auto const child : children) {
                        numbers.push_back(parser.number(child, path));
                    }
                    result.emplace_back(path, std::move(numbers));
                }
                break;
            }
            default:
                parser.error(DiagnosticCode::InvalidType, path, "Unsupported settings value");
                break;
        }
    }
}
}

auto parse_settings(s7::Ast const& ast, s7::NodeIndex const root) -> SettingsReadResult {
    detail::AstParser parser{ast};
    settings::Settings result;
    settings_parser::record(parser, ast, root, {}, result);
    if (!parser.errors().empty()) {
        return std::unexpected{parser.take_errors()};
    }
    return result;
}
}
