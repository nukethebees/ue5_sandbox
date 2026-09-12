#include "manifest.h"

#include <codegen/sexpr/reader.h>

#include <charconv>
#include <fstream>
#include <set>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace slate_codegen::detail {
namespace {

using codegen::sexpr::Form;
using codegen::sexpr::SourceError;
using codegen::sexpr::SourceSpan;
using codegen::sexpr::TokenKind;

inline constexpr int manifest_schema_version{1};

[[noreturn]] void fail(SourceSpan const& span, std::string const& message) {
    throw SourceError{span.path, span, message};
}

auto read_document(std::filesystem::path const& path) -> std::vector<Form> {
    return codegen::sexpr::read_forms(path.string(), read_file(path));
}

auto parse_version(Form const& form) -> int {
    if (form.is_list() || form.token.kind != TokenKind::atom) {
        fail(form.token.span, "Slate manifest schema version must be an integer");
    }

    int result{};
    auto const& value{form.token.text};
    auto const [position,
                error]{std::from_chars(value.data(), value.data() + value.size(), result)};
    if (error != std::errc{} || position != value.data() + value.size()) {
        fail(form.token.span, "Slate manifest schema version must be an integer");
    }
    return result;
}

auto string_list(Form const& form, std::string_view const field) -> std::vector<std::string> {
    if (!form.is_list()) {
        fail(form.token.span, "Slate manifest " + std::string{field} + " must be a list");
    }

    std::vector<std::string> result;
    result.reserve(form.children.size());
    for (auto const& child : form.children) {
        if (child.is_list() || child.token.kind != TokenKind::string || child.token.text.empty()) {
            fail(child.token.span,
                 "Slate manifest " + std::string{field} + " must contain nonempty strings");
        }
        result.push_back(child.token.text);
    }
    return result;
}

void validate_relative_path(std::filesystem::path const& path, std::string_view const field) {
    auto const normalized{path.lexically_normal()};
    if (path.empty() || path.is_absolute() || path.has_root_path()) {
        throw std::invalid_argument{std::string{field} + " must be a relative path"};
    }
    for (auto const& component : normalized) {
        if (component == "..") {
            throw std::invalid_argument{std::string{field} +
                                        " must not escape the manifest directory"};
        }
    }
}

}

auto read_file(std::filesystem::path const& path) -> std::string {
    std::ifstream input{path, std::ios::binary};
    if (!input) {
        throw std::runtime_error{"Cannot read file: " + path.string()};
    }
    return std::string{std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{}};
}

auto load_manifest(std::filesystem::path const& path) -> Manifest {
    auto const forms{read_document(path)};
    if (forms.size() != 1 || forms.front().head() != "slate-manifest") {
        auto const span{forms.empty() ? SourceSpan{.path = path.string()}
                                      : forms.front().token.span};
        fail(span, "manifest must contain exactly one 'slate-manifest' form");
    }

    auto const& form{forms.front()};
    Form const* version{};
    Form const* entries{};
    Form const* include_directories{};
    std::set<std::string> properties;
    for (std::size_t index{1}; index < form.children.size();) {
        auto const& property{form.children[index]};
        if (property.token.kind != TokenKind::keyword) {
            fail(property.token.span, "expected a Slate manifest property");
        }
        if (index + 1 >= form.children.size() ||
            form.children[index + 1].token.kind == TokenKind::keyword) {
            fail(property.token.span, "property ':" + property.token.text + "' requires a value");
        }
        if (!properties.insert(property.token.text).second) {
            fail(property.token.span, "duplicate property ':" + property.token.text + "'");
        }

        auto const* value{&form.children[index + 1]};
        if (property.token.text == "schema-version") {
            version = value;
        } else if (property.token.text == "entries") {
            entries = value;
        } else if (property.token.text == "include-directories") {
            include_directories = value;
        } else {
            fail(property.token.span, "unknown property ':" + property.token.text + "'");
        }
        index += 2;
    }

    if (version == nullptr) {
        fail(form.token.span, "Slate manifest requires ':schema-version'");
    }
    auto const parsed_version{parse_version(*version)};
    if (parsed_version != manifest_schema_version) {
        fail(version->token.span,
             "unsupported Slate manifest schema version " + std::to_string(parsed_version) +
                 "; expected " + std::to_string(manifest_schema_version));
    }
    if (entries == nullptr) {
        fail(form.token.span, "Slate manifest requires ':entries'");
    }

    Manifest result;
    if (include_directories != nullptr) {
        for (auto const& directory : string_list(*include_directories, "include-directories")) {
            result.include_directories.push_back(
                (path.parent_path() / directory).lexically_normal());
        }
    }

    std::set<std::string> inputs;
    for (auto const& input : string_list(*entries, "entries")) {
        ManifestEntry entry{input};
        validate_relative_path(entry.input, "Slate manifest input");
        if (entry.input.extension() != ".sbxslate") {
            throw std::invalid_argument{"Slate manifest inputs must use the .sbxslate extension"};
        }
        if (!inputs.insert(entry.input.generic_string()).second) {
            throw std::invalid_argument{"Duplicate Slate manifest input: " +
                                        entry.input.generic_string()};
        }
        result.entries.push_back(std::move(entry));
    }
    if (result.entries.empty()) {
        throw std::invalid_argument{"Slate manifest entries must not be empty"};
    }
    return result;
}

}
