#pragma once

#include <filesystem>
#include <string>

namespace ioj::layout_planner {

auto run_application(std::filesystem::path project_path,
                     std::string target_name,
                     bool reopen_recent_project) -> int;

} // namespace ioj::layout_planner
