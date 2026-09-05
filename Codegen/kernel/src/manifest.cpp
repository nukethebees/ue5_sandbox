#include "manifest.h"

#include <nlohmann/json.hpp>

#include <fstream>
#include <set>
#include <stdexcept>
#include <string_view>
#include <utility>
#include <vector>

namespace kernel_codegen::detail {
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

auto parse_json(std::filesystem::path const& path) -> nlohmann::json {
    try {
        std::vector<std::set<std::string>> object_keys;
        auto reject_duplicate = [&](int, nlohmann::json::parse_event_t const event,
                                    nlohmann::json& parsed) {
            if (event == nlohmann::json::parse_event_t::object_start) {
                object_keys.emplace_back();
            } else if (event == nlohmann::json::parse_event_t::key) {
                auto const key{parsed.get<std::string>()};
                if (!object_keys.back().insert(key).second) {
                    throw std::invalid_argument{path.string() + ": duplicate field '" + key + "'"};
                }
            } else if (event == nlohmann::json::parse_event_t::object_end) {
                object_keys.pop_back();
            }
            return true;
        };
        return nlohmann::json::parse(read_file(path), reject_duplicate);
    } catch (nlohmann::json::exception const& error) {
        throw std::invalid_argument{path.string() + ": " + error.what()};
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
    auto const document = parse_json(path);
    if (!document.is_object()) {
        throw std::invalid_argument{"Kernel manifest root must be an object"};
    }
    for (auto const& [key, unused] : document.items()) {
        static_cast<void>(unused);
        if (key != "entries") {
            throw std::invalid_argument{"Unknown Kernel manifest field: " + key};
        }
    }
    if (!document.contains("entries") || !document.at("entries").is_array()) {
        throw std::invalid_argument{"Kernel manifest requires an entries array"};
    }

    Manifest result;
    std::set<std::string> inputs;
    for (auto const& item : document.at("entries")) {
        if (!item.is_object() || item.size() != 1 || !item.contains("input") ||
            !item.at("input").is_string()) {
            throw std::invalid_argument{
                "Each Kernel manifest entry requires only a string input field"};
        }
        ManifestEntry entry{item.at("input").get<std::string>()};
        validate_relative_path(entry.input, "Kernel manifest input");
        if (entry.input.extension() != ".sbxkernel") {
            throw std::invalid_argument{
                "Kernel manifest inputs must use the .sbxkernel extension"};
        }
        if (!inputs.insert(entry.input.generic_string()).second) {
            throw std::invalid_argument{"Duplicate Kernel manifest input: " +
                                        entry.input.generic_string()};
        }
        result.entries.push_back(std::move(entry));
    }
    if (result.entries.empty()) {
        throw std::invalid_argument{"Kernel manifest entries must not be empty"};
    }
    return result;
}

}
