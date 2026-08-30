#pragma once

#include "CoreMinimal.h"

class UStaticMesh;

namespace SandboxMesh {

[[nodiscard]] auto get_generated_cube_filename() -> FString;
[[nodiscard]] auto generate_cube_asset() -> UStaticMesh*;

}
