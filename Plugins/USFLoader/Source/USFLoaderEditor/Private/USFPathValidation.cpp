#include "USFPathValidation.h"

#include "Misc/Paths.h"
#include "ShaderCompilerCore.h"
#include "ShaderCore.h"

namespace usf_loader {
namespace {

auto combine_virtual_path(FString const& path_prefix, FString const& file_path) -> FString {
    if (path_prefix.IsEmpty()) {
        return file_path;
    }

    bool const prefix_has_separator{path_prefix.EndsWith(TEXT("/"))};
    bool const path_has_separator{file_path.StartsWith(TEXT("/"))};
    if (prefix_has_separator == path_has_separator) {
        return prefix_has_separator ? path_prefix + file_path.RightChop(1)
                                    : path_prefix + TEXT("/") + file_path;
    }
    return path_prefix + file_path;
}

}

auto resolve_shader_path(FString const& virtual_path) -> std::expected<FString, FString> {
    TArray<FShaderCompilerError> errors;
    auto resolved_path{GetShaderSourceFilePath(virtual_path, &errors)};
    if (resolved_path.IsEmpty()) {
        auto error{errors.IsEmpty()
                       ? FString::Printf(TEXT("Unable to resolve virtual shader path \"%s\"."),
                                         *virtual_path)
                       : errors[0].StrippedErrorMessage};
        return std::unexpected{MoveTemp(error)};
    }
    if (!FPaths::FileExists(resolved_path)) {
        return std::unexpected{
            FString::Printf(TEXT("Virtual shader path \"%s\" resolves to missing file \"%s\"."),
                            *virtual_path,
                            *resolved_path)};
    }
    return resolved_path;
}

auto resolve_include_paths(FString const& path_prefix, TConstArrayView<FString> const file_paths)
    -> std::expected<TArray<FString>, FString> {
    TArray<FString> include_paths;
    include_paths.Reserve(file_paths.Num());

    int32 const path_count{file_paths.Num()};
    for (int32 path_index{}; path_index < path_count; ++path_index) {
        auto const& file_path{file_paths[path_index]};
        if (file_path.IsEmpty()) {
            return std::unexpected{
                FString::Printf(TEXT("USF include path at index %d is empty."), path_index)};
        }

        auto virtual_path{combine_virtual_path(path_prefix, file_path)};
        auto const resolved_path{resolve_shader_path(virtual_path)};
        if (!resolved_path) {
            return std::unexpected{
                FString::Printf(TEXT("USF include path at index %d is invalid: %s"),
                                path_index,
                                *resolved_path.error())};
        }
        include_paths.AddUnique(MoveTemp(virtual_path));
    }

    return include_paths;
}

}
