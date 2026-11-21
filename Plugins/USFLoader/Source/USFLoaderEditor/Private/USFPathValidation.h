#pragma once

#include "CoreMinimal.h"

#include <expected>

namespace usf_loader {

using ShaderPathResult = std::expected<FString, FString>;

auto resolve_shader_path(FString const& virtual_path) -> ShaderPathResult;
using IncludePathsResult = std::expected<TArray<FString>, FString>;

auto resolve_include_paths(FString const& path_prefix, TConstArrayView<FString> file_paths)
    -> IncludePathsResult;

}
