#include <SpaceGameS7/level_definition_reader.h>

#include <ioj/levels/authoring/level_definition_reader.h>
#include <SpaceGame/levels/native_level_definition_conversion.h>

#include <SandboxCoreEngine/strings.h>

namespace ioj::levels::authoring {
namespace {
auto convert_level_result(LevelDefinitionReadResult native) -> FLevelDefinitionReadResult {
    if (!native) {
        return FLevelDefinitionReadResult{std::unexpect, std::move(native.error())};
    }
    return to_unreal(std::move(*native));
}
}
FLevelDefinitionReader::FLevelDefinitionReader(FString script_library_root)
    : script_library_root_{MoveTemp(script_library_root)} {}
auto FLevelDefinitionReader::read_source(FStringView const source) const
    -> FLevelDefinitionReadResult {
    auto const utf8{FTCHARToUTF8{source.GetData(), source.Len()}};
    return convert_level_result(
        LevelDefinitionReader{std::filesystem::path{*script_library_root_}}.read_source(
            std::string_view{utf8.Get(), static_cast<std::size_t>(utf8.Length())}));
}
auto FLevelDefinitionReader::read_file(FStringView const path) const -> FLevelDefinitionReadResult {
    return convert_level_result(
        LevelDefinitionReader{std::filesystem::path{*script_library_root_}}.read_file(
            std::filesystem::path{*FString{path}}));
}
}
