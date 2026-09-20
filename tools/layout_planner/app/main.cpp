#include "application.hpp"

#include <CLI/CLI.hpp>

#include <filesystem>
#include <string>
#include <utility>

auto main(int const argument_count, char** arguments) -> int {
    CLI::App app{"Interactive memory layout planner for LispB schemas"};
    std::filesystem::path project_path{"lispb/project.lispb"};
    std::string target_name{"sandbox-code"};
    auto* project_option{app.add_option("--project", project_path, "LispB project manifest path")};
    app.add_option("--target", target_name, "C++ schema target name");
    CLI11_PARSE(app, argument_count, arguments);

    return ioj::layout_planner::run_application(
        std::move(project_path), std::move(target_name), project_option->count() == 0);
}
