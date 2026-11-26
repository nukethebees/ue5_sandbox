#include <SpaceGameS7/level_script_catalog.h>

#include <Misc/Paths.h>

namespace ioj::levels::authoring {
auto default_level_script_directory() -> FString {
    return FPaths::ConvertRelativePathToFull(
        FPaths::Combine(FPaths::ProjectDir(), TEXT("LevelScripts")));
}

} // namespace ioj::levels::authoring
