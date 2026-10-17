#pragma once

#include <filesystem>

namespace lispb {
struct PublicationOptions {
    std::filesystem::path path_base;
    std::filesystem::path output_root;
    bool track_outputs{true};
    bool check_only{false};
};
} // namespace lispb
