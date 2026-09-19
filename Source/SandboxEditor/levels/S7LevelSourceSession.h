#pragma once

#include <SpaceGameS7/LevelDefinitionReader.h>

#include <CoreMinimal.h>
#include <UObject/WeakObjectPtr.h>

#include <expected>

class AS7LevelAuthoringDocument;

namespace ml::editor {
enum class ES7SourceOverwritePolicy { RefuseExisting, ReplaceExisting };

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
    [[nodiscard]] auto save_replacement(FString source, FStringView synchronized_source_digest)
        -> std::expected<void, FString>;
    [[nodiscard]] auto save_replacement_as(FString source,
                                           FStringView path,
                                           FStringView synchronized_source_digest,
                                           ES7SourceOverwritePolicy overwrite)
        -> std::expected<void, FString>;

    [[nodiscard]] auto read() const -> ml::s7::FLevelDefinitionReadResult;
    [[nodiscard]] auto refresh_external_conflict() -> std::expected<void, FString>;

    [[nodiscard]] auto document() const -> AS7LevelAuthoringDocument*;
    [[nodiscard]] auto path() const -> FString const&;
    [[nodiscard]] auto buffer() const -> FString const&;
    [[nodiscard]] auto revision() const -> uint64;
    [[nodiscard]] auto is_attached() const -> bool;
    [[nodiscard]] auto is_dirty() const -> bool;
    [[nodiscard]] auto has_external_conflict() const -> bool;
    [[nodiscard]] auto has_unapplied_buffer(FStringView synchronized_source_digest) const -> bool;

    [[nodiscard]] static auto source_digest(FStringView source) -> FString;
  private:
    struct FDiskState {
        bool exists{};
        FString digest{};

        auto operator==(FDiskState const&) const -> bool = default;
    };

    [[nodiscard]] static auto normalize_path(FStringView path) -> FString;
    [[nodiscard]] static auto observe_disk(FStringView path) -> std::expected<FDiskState, FString>;
    [[nodiscard]] auto load_attached(AS7LevelAuthoringDocument& document, FStringView path)
        -> std::expected<void, FString>;
    [[nodiscard]] auto verify_disk_baseline() -> std::expected<void, FString>;
    [[nodiscard]] auto write(FString const& source,
                             FStringView target_path,
                             ES7SourceOverwritePolicy overwrite,
                             bool replace_buffer) -> std::expected<void, FString>;

    TWeakObjectPtr<AS7LevelAuthoringDocument> document_{};
    FString path_{};
    FString buffer_{};
    FDiskState disk_baseline_{};
    bool dirty_{};
    bool external_conflict_{};
    uint64 revision_{};
};
}
