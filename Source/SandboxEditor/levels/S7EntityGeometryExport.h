#pragma once

#include <CoreMinimal.h>

#include <expected>

namespace ioj::editor {
SANDBOXEDITOR_API auto export_entity_geometry(FString const& path) -> std::expected<void, FString>;
SANDBOXEDITOR_API auto entity_geometry_path() -> FString;
}
