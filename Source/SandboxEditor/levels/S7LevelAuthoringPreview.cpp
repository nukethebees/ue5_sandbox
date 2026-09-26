#include "SandboxEditor/levels/S7LevelAuthoringPreview.h"

#include "SandboxEditor/levels/S7LevelAuthoringActors.h"
#include "SandboxEditor/levels/S7LevelAuthoringDocument.h"
#include "SandboxEditor/levels/S7LevelAuthoringSession.h"
#include "SandboxEditor/levels/S7LevelSourceSession.h"
#include <SpaceGame/simulation/SpaceGameLevelConfig.h>
#include <SpaceGameS7/LevelDefinitionWriter.h>

#include <Engine/Level.h>
#include <UObject/UnrealType.h>

namespace ml::editor {
namespace s7_level_authoring_preview_detail {
auto level_config_digest(USpaceGameLevelConfig const* const level_config) -> FString {
    if (!IsValid(level_config)) {
        return {};
    }

    FString signature;
    for (TFieldIterator<FProperty> property{USpaceGameLevelConfig::StaticClass(),
                                            EFieldIteratorFlags::ExcludeSuper};
         property;
         ++property) {
        FString value;
        property->ExportText_InContainer(0,
                                         value,
                                         level_config,
                                         level_config,
                                         const_cast<USpaceGameLevelConfig*>(level_config),
                                         0);
        signature += property->GetName();
        signature.AppendChar(TEXT('='));
        signature += FS7LevelSourceSession::source_digest(value);
        signature.AppendChar(TEXT('|'));
    }
    return FS7LevelSourceSession::source_digest(signature);
}

auto scene_digest(ULevel const& level, AS7LevelAuthoringDocument const& document) -> FString {
    auto const definition{collect_s7_editor_level(level, document)};
    if (!definition) {
        return FS7LevelSourceSession::source_digest(TEXT("invalid:") + definition.error());
    }
    auto const source{s7::emit_editor_level_source(*definition)};
    if (!source) {
        return FS7LevelSourceSession::source_digest(TEXT("invalid:") + source.error());
    }
    return FS7LevelSourceSession::source_digest(*source);
}

auto bindings(AS7LevelAuthoringDocument const& document) -> TArray<FS7LevelPreviewBindingSnapshot> {
    TArray<FS7LevelPreviewBindingSnapshot> result;
    result.Reserve(document.entities.Num());
    for (auto const& binding : document.entities) {
        result.Add({.id = binding.id, .actor = binding.actor});
    }
    return result;
}

auto bindings_match(TArray<FS7LevelPreviewBindingSnapshot> const& expected,
                    AS7LevelAuthoringDocument const& document) -> bool {
    if (expected.Num() != document.entities.Num()) {
        return false;
    }
    auto const binding_count{expected.Num()};
    for (int32 index{}; index < binding_count; ++index) {
        if (expected[index].id != document.entities[index].id ||
            expected[index].actor.Get() != document.entities[index].actor.Get()) {
            return false;
        }
    }
    return true;
}

auto stale_message(bool const source_changed,
                   bool const authoring_state_changed,
                   bool const configuration_changed) -> FString {
    TArray<FString> assumptions;
    if (source_changed) {
        assumptions.Add(TEXT("source or source path"));
    }
    if (authoring_state_changed) {
        assumptions.Add(TEXT("level, document, bindings, or scene"));
    }
    if (configuration_changed) {
        assumptions.Add(TEXT("level configuration"));
    }
    return FString::Printf(
        TEXT("Preview is stale because %s changed. Preview again before applying."),
        *FString::Join(assumptions, TEXT(", ")));
}
}

auto make_s7_level_authoring_preview(ULevel& level,
                                     AS7LevelAuthoringDocument& document,
                                     FS7LevelSourceSession const& source_session,
                                     FS7LevelSyncPlan plan)
    -> std::expected<FS7LevelAuthoringPreview, FString> {
    if (source_session.document() != &document) {
        return std::unexpected{TEXT("The source buffer is not attached to this level document.")};
    }
    auto const captured_scene_digest{
        s7_level_authoring_preview_detail::scene_digest(level, document)};

    auto* const level_config{document.level_config.Get()};
    return FS7LevelAuthoringPreview{
        .plan = MoveTemp(plan),
        .source_revision = source_session.revision(),
        .source_path = source_session.path(),
        .level = &level,
        .document = &document,
        .level_config = level_config,
        .level_config_class = IsValid(level_config) ? level_config->GetClass() : nullptr,
        .level_config_digest = s7_level_authoring_preview_detail::level_config_digest(level_config),
        .bindings = s7_level_authoring_preview_detail::bindings(document),
        .scene_digest = captured_scene_digest};
}

auto validate_s7_level_authoring_preview(ULevel& level,
                                         AS7LevelAuthoringDocument& document,
                                         FS7LevelSourceSession const& source_session,
                                         FS7LevelAuthoringPreview const& preview)
    -> std::expected<void, FString> {
    auto const source_changed{source_session.document() != preview.document.Get() ||
                              source_session.revision() != preview.source_revision ||
                              source_session.path() != preview.source_path};
    auto const level_or_document_changed{preview.level.Get() != &level ||
                                         preview.document.Get() != &document};
    auto const bindings_changed{
        !s7_level_authoring_preview_detail::bindings_match(preview.bindings, document)};

    auto* const level_config{document.level_config.Get()};
    auto const configuration_changed{
        preview.level_config.Get() != level_config ||
        preview.level_config_class.Get() !=
            (IsValid(level_config) ? level_config->GetClass() : nullptr) ||
        preview.level_config_digest !=
            s7_level_authoring_preview_detail::level_config_digest(level_config)};

    auto const current_scene_digest{
        s7_level_authoring_preview_detail::scene_digest(level, document)};
    auto const scene_changed{current_scene_digest != preview.scene_digest};
    auto const authoring_state_changed{level_or_document_changed || bindings_changed ||
                                       scene_changed};
    if (source_changed || authoring_state_changed || configuration_changed) {
        return std::unexpected{s7_level_authoring_preview_detail::stale_message(
            source_changed, authoring_state_changed, configuration_changed)};
    }
    return {};
}

auto apply_s7_level_authoring_preview(ULevel& level,
                                      AS7LevelAuthoringDocument& document,
                                      FS7LevelSourceSession const& source_session,
                                      FS7LevelAuthoringPreview const& preview)
    -> std::expected<void, FString> {
    auto const validated{
        validate_s7_level_authoring_preview(level, document, source_session, preview)};
    if (!validated) {
        return std::unexpected{validated.error()};
    }
    return apply_s7_level_sync_plan(level, document, preview.plan);
}
}
