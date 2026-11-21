#pragma once

#include "SandboxEditor/levels/S7AuthoringResult.h"

#include <CoreMinimal.h>

namespace ioj::editor {
SANDBOXEDITOR_API auto export_entity_geometry(FString const& path) -> S7AuthoringResult<void>;
SANDBOXEDITOR_API auto entity_geometry_path() -> FString;
}
