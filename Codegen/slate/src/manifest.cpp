#include "manifest.h"

#include <nlohmann/json.hpp>

#include <fstream>
#include <set>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace slate_codegen::detail {
namespace {

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

auto load_manifest(std::filesystem::path const& path) -> std::vector<ManifestEntry> {
    auto const document = nlohmann::json::parse(read_file(path));
    if (!document.is_object()) {
        throw std::invalid_argument{"Slate manifest root must be an object"};
    }
    if (!document.contains("entries")) {
        throw std::invalid_argument{"Slate manifest must contain an entries field"};
    }
    if (!document.at("entries").is_array()) {
        throw std::invalid_argument{"Slate manifest entries field must be an array"};
    }

    std::vector<ManifestEntry> result;
    std::set<std::string> inputs;
    for (auto const& item : document.at("entries")) {
        if (!item.is_object() || !item.contains("input") || !item.at("input").is_string()) {
            throw std::invalid_argument{"Each Slate manifest entry requires a string input field"};
        }
        ManifestEntry entry{item.at("input").get<std::string>()};
        validate_relative_path(entry.input, "Slate manifest input");
        if (entry.input.extension() != ".sbxslate") {
            throw std::invalid_argument{"Slate manifest inputs must use the .sbxslate extension"};
        }
        if (!inputs.insert(entry.input.generic_string()).second) {
            throw std::invalid_argument{"Duplicate Slate manifest input: " +
                                        entry.input.generic_string()};
        }
        result.push_back(std::move(entry));
    }
    if (result.empty()) {
        throw std::invalid_argument{"Slate manifest entries must not be empty"};
    }
    return result;
}

}
