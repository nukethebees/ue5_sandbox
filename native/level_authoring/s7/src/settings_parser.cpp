#include <ioj/levels/authoring/settings_parser.h>

#include "reader_diagnostics.h"
#include <ioj/levels/authoring/input_readers.h>
#include <ioj/settings/presentation_reader.h>

#include <format>
#include <unordered_set>

namespace ioj::levels::authoring {
namespace settings_parser {
void flatten_record(s7::Reader const& reader,
                    s7::AstDiagnostics& errors,
                    s7::Ast const& ast,
                    s7::NodeIndex node,
                    std::string const& prefix,
                    settings::Settings& result);

void append_value(s7::Reader const& reader,
                  s7::AstDiagnostics& errors,
                  s7::Ast const& ast,
                  s7::NodeIndex const value,
                  std::string const& path,
                  settings::Settings& result) {
    if (path == "presentation") {
        flatten_record(reader, errors, ast, value, path, result);
        return;
    }

    switch (ast.node(value).kind) {
        case s7::NodeKind::Boolean:
            result.assignments.emplace_back(path, ast.boolean(value));
            break;
        case s7::NodeKind::Integer:
            result.assignments.emplace_back(path, ast.integer(value));
            break;
        case s7::NodeKind::Real:
        case s7::NodeKind::Ratio:
            if (auto number{reader.number(value, path)}) {
                result.assignments.emplace_back(path, *number);
            } else {
                s7::append_errors(errors, std::move(number.error()));
            }
            break;
        case s7::NodeKind::String:
        case s7::NodeKind::Symbol:
            result.assignments.emplace_back(path, std::string{ast.text(value)});
            break;
        case s7::NodeKind::List: {
            auto const children{ast.children(value)};
            if (!children.empty() && ast.node(children[0]).kind == s7::NodeKind::Keyword) {
                flatten_record(reader, errors, ast, value, path, result);
                break;
            }

            if (auto numbers{s7::read<std::vector<double>>(reader, value, path)}) {
                result.assignments.emplace_back(path, std::move(*numbers));
            } else {
                s7::append_errors(errors, std::move(numbers.error()));
            }
            break;
        }
        default:
            errors.emplace_back(s7::AstErrorCode::InvalidType, path, "Unsupported settings value");
            break;
    }
}

void flatten_properties(s7::Reader const& reader,
                        s7::AstDiagnostics& errors,
                        s7::Ast const& ast,
                        std::span<s7::NodeIndex const> const values,
                        std::string const& section,
                        settings::Settings& result) {
    std::unordered_set<std::string_view> seen;
    auto const count{values.size()};
    for (std::size_t index{}; index < count; index += 2) {
        auto const key{values[index]};
        if (ast.node(key).kind != s7::NodeKind::Keyword) {
            errors.emplace_back(
                s7::AstErrorCode::ExpectedKeyword, section, "Expected a settings keyword");
            continue;
        }
        auto const name{ast.text(key)};
        auto const path{section.empty() ? std::string{name} : std::format("{}.{}", section, name)};
        if (!seen.insert(name).second) {
            errors.emplace_back(
                s7::AstErrorCode::DuplicateProperty, path, "Duplicate settings property");
        }
        if (index + 1 == count) {
            errors.emplace_back(
                s7::AstErrorCode::MissingValue, path, "Settings property has no value");
            continue;
        }
        append_value(reader, errors, ast, values[index + 1], path, result);
    }
}

// Flatten nested sections and ordered layers into dotted assignments without inserting defaults.
void flatten_record(s7::Reader const& reader,
                    s7::AstDiagnostics& errors,
                    s7::Ast const& ast,
                    s7::NodeIndex const node,
                    std::string const& prefix,
                    settings::Settings& result) {
    auto const decoded{reader.list(node, prefix.empty() ? "settings" : prefix)};
    if (!decoded) {
        s7::append_errors(errors, decoded.error());
        return;
    }
    auto const values{*decoded};
    if (values.empty() || ast.node(values[0]).kind != s7::NodeKind::Keyword) {
        errors.emplace_back(s7::AstErrorCode::ExpectedRecord, prefix, "Expected a settings record");
        return;
    }

    auto const kind{ast.text(values[0])};
    if (prefix == "presentation" || (prefix.empty() && kind == "presentation")) {
        if (!result.presentation) {
            result.presentation.emplace();
        }
        s7::decode_field(
            reader, node, "presentation", *result.presentation, s7::ReadMode::Override, errors);
        return;
    }
    if (kind == "settings-layers") {
        for (auto const layer : values.subspan(1)) {
            flatten_record(reader, errors, ast, layer, prefix, result);
        }
        return;
    }

    auto const section{kind == "settings" || !prefix.empty() ? prefix : std::string{kind}};
    flatten_properties(reader, errors, ast, values.subspan(1), section, result);
}
}

auto parse_settings(s7::Ast const& ast, s7::NodeIndex root) -> SettingsReadResult {
    return s7::read<settings::Settings>(s7::Reader{ast}, root, "settings")
        .transform_error(detail::diagnostics);
}
}

namespace ioj::s7 {
auto TypeReader<settings::Settings>::read(Reader const& reader, NodeIndex node, std::string_view)
    -> ReadResult<settings::Settings> {
    settings::Settings result;
    AstDiagnostics errors;
    levels::authoring::settings_parser::flatten_record(
        reader, errors, reader.ast(), node, {}, result);
    if (!errors.empty()) {
        return std::unexpected{std::move(errors)};
    }
    return result;
}
}
