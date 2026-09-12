#pragma once

#include "CoreMinimal.h"

#include <expected>

namespace usf_loader {

auto resolve_shader_path(FString const& virtual_path) -> std::expected<FString, FString>;
auto resolve_include_paths(FString const& path_prefix,
                           TConstArrayView<FString> file_paths)
    -> std::expected<TArray<FString>, FString>;

}
