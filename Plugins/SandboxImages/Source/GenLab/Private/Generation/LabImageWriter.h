#pragma once

#include "CoreMinimal.h"
#include "sandbox/image/image_generation.h"

namespace SandboxImages::GenLab {

[[nodiscard]] auto get_output_directory() -> FString;
[[nodiscard]] auto generate_and_import(sandbox::image::GenerationRequest const& request) -> bool;
[[nodiscard]] auto regenerate_all() -> bool;

}
