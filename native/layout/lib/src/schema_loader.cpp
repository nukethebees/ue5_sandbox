#include <ioj/layout/schema_loader.hpp>

#include <codegen/source_loader.h>
#include <lispb/project.h>

#include <exception>
#include <utility>

namespace ioj::layout {

auto load_lispb_schema(std::filesystem::path const& project_path, std::string const& target_name)
    -> SchemaLoadResult {
    SchemaLoadResult result;
    try {
        auto const project{lispb::load_project(project_path)};
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
        auto const manifest{codegen::load_sources(project.root / target->types, sources)};
        result.types = lispb::schema::resolve_type_graph(manifest);
        result.loaded = true;
    } catch (std::exception const& error) {
        result.diagnostics.push_back({DiagnosticSeverity::error, error.what()});
    }
    return result;
}

} // namespace ioj::layout
