#pragma once

#include "Generation/ImageGenerators.h"

namespace SandboxImages::GenLab {

[[nodiscard]] auto get_output_directory() -> FString;
[[nodiscard]] auto generate_and_import(FGenerationRequest const& request) -> bool;
[[nodiscard]] auto regenerate_all() -> bool;

}
