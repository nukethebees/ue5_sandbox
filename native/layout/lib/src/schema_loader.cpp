#include <ioj/layout/schema_loader.hpp>

#include "schema_loader_transaction.hpp"

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
        throw std::runtime_error{"Cannot write LispB file: " + path.string()};
    }
}

void validate_new_project_destination(std::filesystem::path const& destination,
                                      std::filesystem::path const& source_directory,
                                      std::filesystem::path const& temporary_project,
                                      std::string const& target_name) {
    if (destination.extension() != ".lispb" || destination.stem().empty()) {
        throw std::invalid_argument{"New project path must end in .lispb"};
    }
    if (target_name.empty() || !std::ranges::all_of(target_name, [](unsigned char const character) {
            return std::isalnum(character) != 0 || character == '_' || character == '-';
        })) {
        throw std::invalid_argument{
            "C++ schema target name must contain only letters, digits, '_' or '-'"};
    }
    if (!std::filesystem::is_directory(destination.parent_path())) {
        throw std::invalid_argument{"New project parent directory does not exist: " +
                                    destination.parent_path().string()};
    }
    for (auto const& path : {destination, source_directory, temporary_project}) {
        std::error_code error;
        auto const status{std::filesystem::symlink_status(path, error)};
        if (error && error != std::errc::no_such_file_or_directory) {
            throw std::filesystem::filesystem_error{
                "Cannot inspect new project destination", path, error};
        }
        if (error != std::errc::no_such_file_or_directory &&
            status.type() != std::filesystem::file_type::not_found) {
            throw std::invalid_argument{"New project destination already exists: " + path.string()};
        }
    }
}

auto publish_project(std::filesystem::path const& temporary_project,
                     std::filesystem::path const& destination,
                     std::string const& target_name,
                     detail::SchemaOpener const& opener) -> SchemaLoadResult {
    auto validated{opener(temporary_project, target_name)};
    if (!validated.loaded) {
        throw std::runtime_error{validated.diagnostics.empty()
                                     ? "LispB project failed validation"
                                     : validated.diagnostics.front().message};
    }

    std::filesystem::rename(temporary_project, destination);
    try {
        auto opened{opener(destination, target_name)};
        if (!opened.loaded) {
            throw std::runtime_error{opened.diagnostics.empty()
                                         ? "Published LispB project could not be reopened"
                                         : opened.diagnostics.front().message};
        }
        return opened;
    } catch (...) {
        std::error_code ignored;
        std::filesystem::remove(destination, ignored);
        throw;
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

auto create_blank_lispb_schema(std::filesystem::path const& destination_project_path,
                               std::string const& target_name) -> SchemaLoadResult {
    auto const destination{normalized_path(destination_project_path)};
    SchemaLoadResult result;
    result.project_path = destination;
    result.target_name = target_name;
    auto const source_directory{destination.parent_path() /
                                (destination.stem().string() + "_schema")};
    auto temporary_project{destination};
    temporary_project += ".layout-planner.tmp";
    bool source_directory_created{};
    bool temporary_project_created{};
    try {
        validate_new_project_destination(
            destination, source_directory, temporary_project, target_name);

        if (!std::filesystem::create_directory(source_directory)) {
            throw std::runtime_error{"New project source directory already exists"};
        }
        source_directory_created = true;
        write_file(source_directory / "types.lispb", "");
        write_file(source_directory / "source.lispb",
                   "(scalar-module starter\n"
                   "  :header \"Starter.h\")\n");

        auto const relative_directory{source_directory.filename()};
        std::ostringstream project;
        project << "(lispb-project\n"
                << "  :language-version 1\n"
                << "  :project-root \".\"\n\n"
                << "  (cpp-schema " << target_name << "\n"
                << "    :types " << quote((relative_directory / "types.lispb").generic_string())
                << "\n"
                << "    :sources (" << quote((relative_directory / "source.lispb").generic_string())
                << ")\n"
                << "    :output-root (project-path \"generated\")))\n";
        temporary_project_created = true;
        write_file(temporary_project, project.str());
        return publish_project(temporary_project, destination, target_name, load_lispb_schema);
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

auto detail::clone_lispb_schema_with_opener(lispb::schema::EditableSchemaDocument const& document,
                                            std::filesystem::path const& destination_project_path,
                                            std::string const& target_name,
                                            SchemaOpener const& opener) -> SchemaLoadResult {
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
        validate_new_project_destination(
            destination, source_directory, temporary_project, target_name);
        auto const sources{document.source_files()};
        if (sources.empty()) {
            throw std::runtime_error{"The editable document has no source files to clone"};
        }
        auto updates{document.preview_source_updates()};
        if (!updates.has_value()) {
            throw std::runtime_error{updates.error().message};
        }

        if (!std::filesystem::create_directory(source_directory)) {
            throw std::runtime_error{"Clone source directory already exists"};
        }
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
        return publish_project(temporary_project, destination, target_name, opener);
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

auto clone_lispb_schema(lispb::schema::EditableSchemaDocument const& document,
                        std::filesystem::path const& destination_project_path,
                        std::string const& target_name) -> SchemaLoadResult {
    return detail::clone_lispb_schema_with_opener(
        document, destination_project_path, target_name, load_lispb_schema);
}

} // namespace ioj::layout
