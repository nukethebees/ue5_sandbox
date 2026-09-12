#pragma once

#include "syntax.h"

#include <lispb/compilation.h>

#include <vector>

namespace kernel_codegen::detail {

auto render(KernelModule const& module, Profile profile) -> std::vector<lispb::TextArtifact>;

}
