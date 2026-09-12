#pragma once

#include <lispb/compilation.h>

#include <filesystem>

namespace lispb {

struct PublicationOptions {
    std::filesystem::path path_base;
    std::filesystem::path output_root;
    bool track_outputs{true};
    bool check_only{false};
};

[[nodiscard]] auto publish(Compilation const& compilation, PublicationOptions const& options)
    -> int;
void write_depfile(std::filesystem::path const& path,
                   Compilation const& compilation,
                   PublicationOptions const& options);

} // namespace lispb
