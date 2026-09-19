#include "application.hpp"

#include <ioj/layout/diagnostic.hpp>
#include <ioj/layout/schema_loader.hpp>

#include <CLI/CLI.hpp>

#include <cstdio>
#include <filesystem>
#include <string>

auto main(int const argument_count, char** arguments) -> int {
    CLI::App app{"Interactive memory layout planner for LispB schemas"};
    std::filesystem::path project_path{"lispb/project.lispb"};
    std::string target_name{"sandbox-code"};
    app.add_option("--project", project_path, "LispB project manifest path");
    app.add_option("--target", target_name, "C++ schema target name");
    CLI11_PARSE(app, argument_count, arguments);

    auto loaded{ioj::layout::load_lispb_schema(project_path, target_name)};
    for (auto const& diagnostic : loaded.diagnostics) {
        auto const* severity{diagnostic.severity == ioj::layout::DiagnosticSeverity::error ? "error"
                             : diagnostic.severity == ioj::layout::DiagnosticSeverity::warning
                                 ? "warning"
                                 : "info"};
        std::fprintf(stderr, "layout-planner: %s: %s\n", severity, diagnostic.message.c_str());
    }
    return ioj::layout_planner::run_application(std::move(loaded));
}
