#pragma once

#include <filesystem>

namespace lispb {
struct Compilation;
struct PublicationOptions;

[[nodiscard]] auto publish(Compilation& compilation, PublicationOptions const& options) -> int;
void write_depfile(std::filesystem::path const& path,
                   Compilation const& compilation,
                   PublicationOptions const& options);

} // namespace lispb
