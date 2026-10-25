#pragma once

#include <ioj/levels/authoring/definition_reader.h>
#include <ioj/levels/authoring/source_session.h>

#include <CoreMinimal.h>
#include <UObject/WeakObjectPtr.h>

#include <expected>

class AS7LevelAuthoringDocument;

namespace ioj::editor {
using ES7SourceOverwritePolicy = levels::authoring::SourceOverwritePolicy;

struct FS7LevelReadResult {
    levels::LevelDefinition definition;
    FString evaluated_source;
};

class SANDBOXEDITOR_API FS7LevelSourceSession final {
  public:
    [[nodiscard]] auto attach(AS7LevelAuthoringDocument& document) -> std::expected<void, FString>;
    [[nodiscard]] auto load(AS7LevelAuthoringDocument& document, FStringView path)
        -> std::expected<void, FString>;
    [[nodiscard]] auto reload() -> std::expected<void, FString>;
    [[nodiscard]] auto discard_and_attach(AS7LevelAuthoringDocument& document)
        -> std::expected<void, FString>;

    void set_buffer(FString buffer);
    [[nodiscard]] auto save() -> std::expected<void, FString>;
    [[nodiscard]] auto save_as(FStringView path, ES7SourceOverwritePolicy overwrite)
        -> std::expected<void, FString>;
    [[nodiscard]] auto save_replacement(FString source, FStringView synchronized_source)
        -> std::expected<void, FString>;
    [[nodiscard]] auto save_replacement_as(FString source,
                                           FStringView path,
                                           FStringView synchronized_source,
                                           ES7SourceOverwritePolicy overwrite)
        -> std::expected<void, FString>;

    [[nodiscard]] auto read(settings::Settings base = {}) const
        -> std::expected<levels::LevelDefinition, levels::Diagnostics>;
    [[nodiscard]] auto read_replay(settings::Settings base) const
        -> std::expected<FS7LevelReadResult, levels::Diagnostics>;
    [[nodiscard]] auto refresh_external_conflict() -> std::expected<void, FString>;

    [[nodiscard]] auto document() const -> AS7LevelAuthoringDocument*;
    [[nodiscard]] auto path() const -> FString;
    [[nodiscard]] auto buffer() const -> FString;
    [[nodiscard]] auto revision() const -> uint64;
    [[nodiscard]] auto is_attached() const -> bool;
    [[nodiscard]] auto is_dirty() const -> bool;
    [[nodiscard]] auto has_external_conflict() const -> bool;
    [[nodiscard]] auto has_unapplied_buffer(FStringView synchronized_source) const -> bool;
    void detach();
    auto state() -> levels::authoring::SourceSessionState& { return session_.state(); }
    auto state() const -> levels::authoring::SourceSessionState const& { return session_.state(); }
  private:
    TWeakObjectPtr<AS7LevelAuthoringDocument> document_{};
    levels::authoring::SourceSession session_{};
};
}
