#include <ioj/layout/schema_loader.hpp>

#include <lispb/project.h>
#include <lispb/schema/editable_document.h>

#include <algorithm>
#include <cctype>
#include <exception>
#include <fstream>
#include <sstream>
#include <utility>

namespace ioj::layout {
namespace {

auto normalized_path(std::filesystem::path const& path) -> std::filesystem::path {
    return std::filesystem::absolute(path).lexically_normal();
}

auto safe_filename(std::filesystem::path const& path, std::size_t const index) -> std::string {
    auto stem{path.stem().string()};
    std::ranges::transform(stem, stem.begin(), [](unsigned char const character) {
        return std::isalnum(character) != 0 || character == '-' || character == '_'
                 ? static_cast<char>(character)
                 : '_';
    });
    return "module_" + std::to_string(index) + "_" + stem + ".lispb";
}

auto quote(std::string const& value) -> std::string {
    std::string result{"\""};
    for (auto const character : value) {
        if (character == '\\' || character == '"') {
            result += '\\';
        }
        result += character;
    }
    result += '"';
    return result;
}

void write_file(std::filesystem::path const& path, std::string const& text) {
    std::ofstream output{path, std::ios::binary | std::ios::trunc};
    output.write(text.data(), static_cast<std::streamsize>(text.size()));
    output.close();
    if (!output) {
        throw std::runtime_error{"Cannot write LispB clone file: " + path.string()};
    }
}

} // namespace

auto load_lispb_schema(std::filesystem::path const& project_path, std::string const& target_name)
    -> SchemaLoadResult {
    SchemaLoadResult result;
    result.project_path = normalized_path(project_path);
    result.target_name = target_name;
    try {
        auto project_document{lispb::load_editable_project_document(result.project_path)};
        auto const& project{project_document.project()};
        auto const target_found{project.targets.find(target_name)};
        if (target_found == project.targets.end()) {
            result.diagnostics.push_back(
                {DiagnosticSeverity::error,
                 "LispB project has no target named '" + target_name + "'."});
            return result;
        }
        auto const* target{std::get_if<lispb::CppSchemaTarget>(&target_found->second)};
        if (target == nullptr) {
            result.diagnostics.push_back(
                {DiagnosticSeverity::error,
                 "LispB target '" + target_name + "' is not a C++ schema target."});
            return result;
        }

        std::vector<std::filesystem::path> sources;
        sources.reserve(target->sources.size());
        for (auto const& source : target->sources) {
            sources.push_back(project.root / source);
        }
        result.document =
            lispb::schema::load_editable_schema_document(project.root / target->types, sources);
        result.project_document = std::move(project_document);
        result.loaded = true;
    } catch (std::exception const& error) {
        result.diagnostics.push_back({DiagnosticSeverity::error, error.what()});
    }
    return result;
}

auto clone_lispb_schema(lispb::schema::EditableSchemaDocument const& document,
                        std::filesystem::path const& destination_project_path,
                        std::string const& target_name) -> SchemaLoadResult {
    auto const destination{normalized_path(destination_project_path)};
    SchemaLoadResult result;
    result.project_path = destination;
    result.target_name = target_name;
    auto source_directory{destination.parent_path() / (destination.stem().string() + "_schema")};
    auto temporary_project{destination};
    temporary_project += ".layout-planner.tmp";
    bool source_directory_created{};
    bool temporary_project_created{};
    try {
        if (target_name.empty()) {
            throw std::runtime_error{"Save As requires a LispB target name"};
        }
        if (std::filesystem::exists(destination) || std::filesystem::exists(source_directory) ||
            std::filesystem::exists(temporary_project)) {
            throw std::runtime_error{
                "Save As destination already exists; choose a new project name"};
        }
        auto const sources{document.source_files()};
        if (sources.empty()) {
            throw std::runtime_error{"The editable document has no source files to clone"};
        }
        auto updates{document.preview_source_updates()};
        if (!updates.has_value()) {
            throw std::runtime_error{updates.error().message};
        }

        std::filesystem::create_directories(source_directory);
        source_directory_created = true;
        auto source_text{[&](lispb::schema::SchemaSourceFile const& source) -> std::string const& {
            auto const update{
                std::ranges::find(*updates, source.path, &lispb::schema::SchemaSourceUpdate::path)};
            return update == updates->end() ? source.text : update->updated;
        }};

        auto const types_name{std::filesystem::path{"types.lispb"}};
        write_file(source_directory / types_name, source_text(sources.front()));
        std::vector<std::filesystem::path> module_names;
        module_names.reserve(sources.size() - 1);
        for (std::size_t index{1}; index < sources.size(); ++index) {
            auto const name{std::filesystem::path{safe_filename(sources[index].path, index)}};
            write_file(source_directory / name, source_text(sources[index]));
            module_names.push_back(name);
        }

        auto const relative_source_directory{source_directory.filename()};
        std::ostringstream project;
        project << "(lispb-project\n"
                << "  :language-version 1\n"
                << "  :project-root \".\"\n\n"
                << "  (cpp-schema " << target_name << "\n"
                << "    :types " << quote((relative_source_directory / types_name).generic_string())
                << "\n"
                << "    :sources (";
        for (auto const& module_name : module_names) {
            project << "\n      "
                    << quote((relative_source_directory / module_name).generic_string());
        }
        project << ")\n"
                << "    :output-root (project-path \"generated\")))\n";
        temporary_project_created = true;
        write_file(temporary_project, project.str());

        auto validated{load_lispb_schema(temporary_project, target_name)};
        if (!validated.loaded) {
            auto const message{validated.diagnostics.empty()
                                   ? std::string{"Cloned LispB project failed validation"}
                                   : validated.diagnostics.front().message};
            throw std::runtime_error{message};
        }
        std::filesystem::rename(temporary_project, destination);
        return load_lispb_schema(destination, target_name);
    } catch (std::exception const& error) {
        std::error_code ignored;
        if (temporary_project_created) {
            std::filesystem::remove(temporary_project, ignored);
        }
        if (source_directory_created) {
            std::filesystem::remove_all(source_directory, ignored);
        }
        result.diagnostics.push_back({DiagnosticSeverity::error, error.what()});
        return result;
    }
}

} // namespace ioj::layout
