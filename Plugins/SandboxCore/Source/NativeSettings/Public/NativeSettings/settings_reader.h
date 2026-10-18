#pragma once

#include <ioj/settings/settings.h>

#include <CoreMinimal.h>

#include <expected>

namespace ioj::settings {
// Resolve paths relative to the project directory and imports relative to LevelScripts.
NATIVESETTINGS_API auto read_file(FStringView path) -> std::expected<Settings, FString>;
// Include evaluated imports and overrides in a source identity without relying on file timestamps.
NATIVESETTINGS_API auto source_hash(FStringView previous_hash, Settings const& settings) -> FString;
}
