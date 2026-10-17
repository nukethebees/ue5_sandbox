#pragma once

#include <filesystem>
#include <string>

namespace lispb::schema {
struct SchemaSourceUpdate {
    std::filesystem::path path;
    std::string original;
    std::string updated;
};
}
