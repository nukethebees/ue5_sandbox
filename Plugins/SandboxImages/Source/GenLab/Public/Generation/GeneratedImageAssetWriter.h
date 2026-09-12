#pragma once

#include "CoreMinimal.h"
#include "sandbox/image/image_generation.h"

namespace SandboxImages::GenLab {

inline constexpr TCHAR lab_content_path[]{TEXT("/SandboxImages/Lab/Images")};

[[nodiscard]] GENLAB_API auto get_output_directory(FString const& destination_content_path)
    -> FString;
[[nodiscard]] GENLAB_API auto
generate_and_import(sandbox::image::GenerationRequest const& request,
                    FString const& destination_content_path) -> bool;
[[nodiscard]] GENLAB_API auto regenerate_all() -> bool;

}
