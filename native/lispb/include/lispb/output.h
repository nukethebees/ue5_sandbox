#pragma once

#include <lispb/compilation.h>
#include <lispb/publication_options.h>

#include <filesystem>

namespace lispb {

[[nodiscard]] auto publish(Compilation const& compilation, PublicationOptions const& options)
    -> int;
void write_depfile(std::filesystem::path const& path,
                   Compilation const& compilation,
                   PublicationOptions const& options);

} // namespace lispb
