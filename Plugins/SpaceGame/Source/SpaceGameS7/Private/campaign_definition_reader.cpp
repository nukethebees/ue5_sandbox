#include <SpaceGameS7/campaign_definition_reader.h>

#include <ioj/levels/authoring/campaign_definition_reader.h>
#include <SpaceGame/levels/native_level_definition_conversion.h>

#include <SandboxCoreEngine/strings.h>

namespace ioj::levels::authoring {
namespace {
auto convert_campaign_result(CampaignDefinitionReadResult native) -> FCampaignDefinitionReadResult {
    if (!native) {
        return FCampaignDefinitionReadResult{std::unexpect, std::move(native.error())};
    }
    ml::FCampaignDefinition result;
    result.id = ml::FCampaignId{FName{ml::to_fstring(native->id.value)}};
    result.title = ml::to_fstring(native->title);
    result.level_ids.Reserve(static_cast<int32>(native->level_ids.size()));
    for (auto const& id : native->level_ids) {
        result.level_ids.Add(ml::FLevelId{FName{ml::to_fstring(id.value)}});
    }
    return result;
}
}
FCampaignDefinitionReader::FCampaignDefinitionReader(FString script_library_root)
    : script_library_root_{MoveTemp(script_library_root)} {}
auto FCampaignDefinitionReader::read_source(FStringView const source) const
    -> FCampaignDefinitionReadResult {
    auto const utf8{FTCHARToUTF8{source.GetData(), source.Len()}};
    return convert_campaign_result(
        CampaignDefinitionReader{std::filesystem::path{*script_library_root_}}.read_source(
            std::string_view{utf8.Get(), static_cast<std::size_t>(utf8.Length())}));
}
auto FCampaignDefinitionReader::read_file(FStringView const path) const
    -> FCampaignDefinitionReadResult {
    return convert_campaign_result(
        CampaignDefinitionReader{std::filesystem::path{*script_library_root_}}.read_file(
            std::filesystem::path{*FString{path}}));
}
}
