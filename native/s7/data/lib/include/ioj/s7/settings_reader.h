#pragma once

#include <ioj/s7/reader.h>

#include <unordered_set>

namespace ioj::s7 {
template <typename Visit>
void visit_settings_sections(Reader const& reader,
                             NodeIndex node,
                             std::string_view section,
                             Visit const& visit,
                             AstDiagnostics& errors) {
    auto const values{reader.list(node, "settings")};
    if (!values) {
        append_errors(errors, values.error());
        return;
    }
    auto const& ast{reader.ast()};
    if (values->empty() || ast.node(values->front()).kind != NodeKind::Keyword) {
        errors.push_back({AstErrorCode::ExpectedRecord, "settings", "Expected a settings record"});
        return;
    }
    auto const tag{ast.text(values->front())};
    if (tag == "settings-layers") {
        for (auto const layer : values->subspan(1)) {
            visit_settings_sections(reader, layer, section, visit, errors);
        }
    } else if (tag == section) {
        visit(node);
    } else if (tag == "settings") {
        std::unordered_set<std::string_view> seen;
        auto const count{values->size()};
        for (std::size_t i{1}; i < count; i += 2) {
            auto const key{(*values)[i]};
            if (i + 1 == count) {
                errors.push_back(
                    {AstErrorCode::MissingValue, "settings", "Settings property has no value"});
                break;
            }
            if (ast.node(key).kind != NodeKind::Keyword) {
                errors.push_back(
                    {AstErrorCode::ExpectedKeyword, "settings", "Expected a settings keyword"});
                continue;
            }
            auto const name{ast.text(key)};
            if (!seen.insert(name).second) {
                errors.push_back({AstErrorCode::DuplicateProperty,
                                  std::string{name},
                                  "Duplicate settings property"});
            } else if (name == section) {
                visit_settings_sections(reader, (*values)[i + 1], section, visit, errors);
            }
        }
    } else {
        errors.push_back({AstErrorCode::ExpectedRecord,
                          std::string{section},
                          std::format("Expected a '{}' settings section", section)});
    }
}

template <typename T>
auto read_settings(Reader const& reader, NodeIndex node, std::string_view section, T value = {})
    -> ReadResult<T> {
    AstDiagnostics errors;
    visit_settings_sections(
        reader,
        node,
        section,
        [&](NodeIndex layer) {
            decode_field({reader, layer, section, ReadMode::Override, errors}, value);
        },
        errors);
    if (!errors.empty()) {
        return std::unexpected{std::move(errors)};
    }
    return value;
}
}
