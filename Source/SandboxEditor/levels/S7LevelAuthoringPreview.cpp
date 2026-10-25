#include "SandboxEditor/levels/S7LevelAuthoringPreview.h"

#include "SandboxEditor/levels/S7LevelAuthoringActors.h"
#include "SandboxEditor/levels/S7LevelAuthoringDocument.h"
#include "SandboxEditor/levels/S7LevelAuthoringSession.h"
#include "SandboxEditor/levels/S7LevelSourceSession.h"
#include <SpaceGame/simulation/SpaceGameLevelConfig.h>

#include <Engine/Level.h>
#include <ScopedTransaction.h>
#include <UObject/UnrealType.h>

namespace ioj::editor {
namespace s7_level_authoring_preview_detail {
auto level_config_snapshot(USpaceGameLevelConfig const* const level_config) -> FString {
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
        signature += value;
        signature.AppendChar(TEXT('|'));
    }
    return signature;
}

auto actor_identities(ULevel const& level, AS7LevelAuthoringDocument const& document)
    -> TArray<TWeakObjectPtr<AActor>> {
    TArray<TWeakObjectPtr<AActor>> result;
    for (auto const& binding : document.entities) {
        result.Add(binding.actor.Get());
    }
    result.Add(nullptr);
    for (auto const actor : document.camera.targets) {
        result.Add(actor.Get());
    }
    result.Add(nullptr);
    for (auto const actor : level.Actors) {
        if (IsValid(actor) && resolve_s7_level_actor(*actor).IsSet()) {
            result.Add(actor.Get());
        }
    }
    return result;
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
    auto* const level_config{document.level_config.Get()};
    return FS7LevelAuthoringPreview{
        .plan = MoveTemp(plan),
        .source_revision = source_session.revision(),
        .level = &level,
        .document = &document,
        .level_config = level_config,
        .level_config_snapshot =
            s7_level_authoring_preview_detail::level_config_snapshot(level_config),
        .actor_identities = s7_level_authoring_preview_detail::actor_identities(level, document)};
}

auto validate_s7_level_authoring_preview(ULevel& level,
                                         AS7LevelAuthoringDocument& document,
                                         FS7LevelSourceSession const& source_session,
                                         FS7LevelAuthoringPreview const& preview)
    -> std::expected<void, FS7LevelPreviewError> {
    auto const source_changed{source_session.document() != preview.document.Get() ||
                              source_session.revision() != preview.source_revision};
    auto const level_or_document_changed{preview.level.Get() != &level ||
                                         preview.document.Get() != &document};
    auto const bindings_changed{s7_level_authoring_preview_detail::actor_identities(
                                    level, document) != preview.actor_identities};

    auto* const level_config{document.level_config.Get()};
    auto const configuration_changed{
        preview.level_config.Get() != level_config ||
        preview.level_config_snapshot !=
            s7_level_authoring_preview_detail::level_config_snapshot(level_config)};

    auto const current_scene{collect_s7_scene(level, document)};
    auto const scene_changed{!current_scene || *current_scene != preview.plan.scene};
    auto const authoring_state_changed{level_or_document_changed || bindings_changed ||
                                       scene_changed};
    if (source_changed || authoring_state_changed || configuration_changed) {
        return std::unexpected{FS7LevelPreviewError{
            ES7LevelPreviewError::Stale,
            s7_level_authoring_preview_detail::stale_message(
                source_changed, authoring_state_changed, configuration_changed)}};
    }
    return {};
}

auto apply_s7_level_authoring_preview(ULevel& level,
                                      AS7LevelAuthoringDocument& document,
                                      FS7LevelSourceSession const& source_session,
                                      FS7LevelAuthoringPreview const& preview)
    -> std::expected<void, FS7LevelPreviewError> {
    auto const validated{
        validate_s7_level_authoring_preview(level, document, source_session, preview)};
    if (!validated) {
        return std::unexpected{validated.error()};
    }
    FScopedTransaction transaction{
        NSLOCTEXT("S7LevelAuthoring", "ApplyPreview", "Apply S7 Preview")};
    auto applied{apply_s7_level_sync_plan(level, document, preview.plan)};
    if (!applied) {
        transaction.Cancel();
        return std::unexpected{
            FS7LevelPreviewError{ES7LevelPreviewError::Execution, applied.error()}};
    }
    document.Modify();
    auto const source{collect_s7_scene_source(level, document)};
    document.synchronized_scene_source = source.value_or(FString{});
    return {};
}
}
