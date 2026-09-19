#include <SpaceGameS7/LevelDefinitionReader.h>

#include <sandbox/level_authoring/LevelDefinitionReader.h>
#include <SandboxCoreEngine/strings.h>
#include <SpaceGame/levels/NativeLevelDefinitionConversion.h>

#include <Misc/FileHelper.h>
#include <Misc/Paths.h>

#include <string_view>

namespace ml::s7 {
namespace {
auto to_unreal(level_authoring::LevelDefinitionReadResult native) -> FLevelDefinitionReadResult {
    FLevelDefinitionReadResult result;
    result.script_error = ml::to_fstring(native.script_error);
    result.decode_errors.Reserve(static_cast<int32>(native.decode_errors.size()));
    for (auto& error : native.decode_errors) {
        result.decode_errors.Add(
            {.path = ml::to_fstring(error.path), .message = ml::to_fstring(error.message)});
    }
    result.validation_errors.Reserve(static_cast<int32>(native.validation_errors.size()));
    for (auto& error : native.validation_errors) {
        result.validation_errors.Add(
            {.code = error.code, .message = ml::to_fstring(error.message)});
    }
    if (native.definition) {
        result.definition.Emplace(level_authoring::to_unreal(std::move(*native.definition)));
    }
    return result;
}
} // namespace

FLevelDefinitionReader::FLevelDefinitionReader(FString script_library_root)
    : script_library_root_{MoveTemp(script_library_root)} {}

auto FLevelDefinitionReader::read_source(FStringView const source) const
    -> FLevelDefinitionReadResult {
    auto const converted_source{FTCHARToUTF8{source.GetData(), source.Len()}};
    auto const utf8_source{std::string_view{converted_source.Get(),
                                            static_cast<std::size_t>(converted_source.Length())}};

    auto const converted_root{FTCHARToUTF8{*script_library_root_}};
    auto const utf8_root{
        std::string{converted_root.Get(), static_cast<std::size_t>(converted_root.Length())}};
    return to_unreal(level_authoring::LevelDefinitionReader{utf8_root}.read_source(utf8_source));
}

auto FLevelDefinitionReader::read_file(FStringView const path) const -> FLevelDefinitionReadResult {
    FString source;
    auto const owned_path{FString{path}};
    if (!FFileHelper::LoadFileToString(source, *owned_path)) {
        return {.script_error =
                    FString::Printf(TEXT("Could not read level script '%s'."), *owned_path)};
    }

    if (script_library_root_.IsEmpty()) {
        auto const library_root{FPaths::Combine(FPaths::GetPath(owned_path), TEXT("Libraries"))};
        return FLevelDefinitionReader{library_root}.read_source(source);
    }
    return read_source(source);
}
} // namespace ml::s7
