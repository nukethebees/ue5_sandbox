#pragma once

#include <lispb/project_edit_kind.h>

#include <filesystem>
#include <string>
namespace lispb {

struct ProjectEditRequest {
    ProjectEditKind kind;
    std::string target_name;
    std::filesystem::path source;
    std::filesystem::path destination;
    std::string contents;
};

}
