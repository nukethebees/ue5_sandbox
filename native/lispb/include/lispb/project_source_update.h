#pragma once

#include <filesystem>
#include <string>
namespace lispb {

struct ProjectSourceUpdate {
    std::filesystem::path path;
    std::string original;
    std::string updated;
};

}
