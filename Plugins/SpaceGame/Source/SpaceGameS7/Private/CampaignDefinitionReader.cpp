#include <SpaceGameS7/CampaignDefinitionReader.h>

#include <sandbox/level_authoring/CampaignDefinitionReader.h>
#include <SandboxCoreEngine/strings.h>

#include <Misc/FileHelper.h>
#include <Misc/Paths.h>

namespace ml::s7 {
namespace {
auto to_unreal(level_authoring::CampaignDefinitionReadResult native)
    -> FCampaignDefinitionReadResult {
    FCampaignDefinitionReadResult result;
    result.script_error = ml::to_fstring(native.script_error);
    result.decode_errors.Reserve(static_cast<int32>(native.decode_errors.size()));
    for (auto const& error : native.decode_errors) {
        result.decode_errors.Add(
            {.path = ml::to_fstring(error.path), .message = ml::to_fstring(error.message)});
    }
    if (native.definition) {
        FCampaignDefinition definition;
        definition.id = FCampaignId{FName{ml::to_fstring(native.definition->id)}};
        definition.title = ml::to_fstring(native.definition->title);
        definition.level_ids.Reserve(static_cast<int32>(native.definition->level_ids.size()));
        for (auto const& level_id : native.definition->level_ids) {
            definition.level_ids.Add(FLevelId{FName{ml::to_fstring(level_id)}});
        }
        result.definition.Emplace(MoveTemp(definition));
    }
    return result;
}
} // namespace

FCampaignDefinitionReader::FCampaignDefinitionReader(FString script_library_root)
    : script_library_root_{MoveTemp(script_library_root)} {}

auto FCampaignDefinitionReader::read_source(FStringView const source) const
    -> FCampaignDefinitionReadResult {
    auto const converted_source{FTCHARToUTF8{source.GetData(), source.Len()}};
    auto const utf8_source{std::string_view{converted_source.Get(),
                                            static_cast<std::size_t>(converted_source.Length())}};
    auto const converted_root{FTCHARToUTF8{*script_library_root_}};
    auto const utf8_root{
        std::string{converted_root.Get(), static_cast<std::size_t>(converted_root.Length())}};
    return to_unreal(level_authoring::CampaignDefinitionReader{utf8_root}.read_source(utf8_source));
}

auto FCampaignDefinitionReader::read_file(FStringView const path) const
    -> FCampaignDefinitionReadResult {
    FString source;
    auto const owned_path{FString{path}};
    if (!FFileHelper::LoadFileToString(source, *owned_path)) {
        return {.script_error =
                    FString::Printf(TEXT("Could not read campaign script '%s'."), *owned_path)};
    }
    if (script_library_root_.IsEmpty()) {
        auto const level_script_root{FPaths::GetPath(FPaths::GetPath(owned_path))};
        return FCampaignDefinitionReader{FPaths::Combine(level_script_root, TEXT("Libraries"))}
            .read_source(source);
    }
    return read_source(source);
}
} // namespace ml::s7
