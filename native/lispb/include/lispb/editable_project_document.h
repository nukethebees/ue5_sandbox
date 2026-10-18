#pragma once

#include <codegen/sexpr/indices.h>
#include <lispb/indices.h>
#include <lispb/pending_project_source.h>
#include <lispb/project.h>
#include <lispb/project_create_commands.h>
#include <lispb/project_edit_error.h>
#include <lispb/project_edit_id.h>
#include <lispb/project_rename_commands.h>
#include <lispb/project_source_commands.h>
#include <lispb/project_source_list_range.h>
#include <lispb/project_source_update.h>
#include <lispb/target_id.h>

#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <map>
#include <memory>
#include <optional>
#include <string_view>
#include <vector>
namespace lispb {
struct ProjectEditRequest;

struct CompilerStorage;

class EditableProjectDocument {
  public:
    EditableProjectDocument();
    ~EditableProjectDocument();
    EditableProjectDocument(EditableProjectDocument&&) noexcept;
    auto operator=(EditableProjectDocument&&) noexcept -> EditableProjectDocument&;
    EditableProjectDocument(EditableProjectDocument const&) = delete;
    auto operator=(EditableProjectDocument const&) -> EditableProjectDocument& = delete;

    [[nodiscard]] auto project() const -> Project const& { return project_; }
    [[nodiscard]] auto path() const -> std::filesystem::path const& { return path_; }
    [[nodiscard]] auto source_text() const -> std::string_view;
    [[nodiscard]] auto dirty() const -> bool {
        return history_position_ != saved_history_position_;
    }
    [[nodiscard]] auto can_undo() const -> bool { return history_position_ != 0; }
    [[nodiscard]] auto can_redo() const -> bool { return history_position_ < history_.size(); }
    [[nodiscard]] auto revision() const -> std::uint64_t { return revision_; }
    [[nodiscard]] auto source_is_pending(std::filesystem::path const& source) const -> bool;
    [[nodiscard]] auto renamed_source_original(std::filesystem::path const& source) const
        -> std::optional<std::filesystem::path>;

    auto apply(ProjectEditRequest const& command) -> std::expected<bool, ProjectEditError>;
    auto undo() -> std::expected<bool, ProjectEditError>;
    auto redo() -> std::expected<bool, ProjectEditError>;
    void discard_redo_history();
    [[nodiscard]] auto preview_source_updates() const
        -> std::expected<std::vector<ProjectSourceUpdate>, ProjectEditError>;
    auto save() -> std::expected<bool, ProjectEditError>;
  private:
    ProjectSourceCommands source_commands_;
    ProjectRenameCommands rename_commands_;
    ProjectCreateCommands create_commands_;

    friend auto load_editable_project_document(std::filesystem::path const& path)
        -> EditableProjectDocument;
    auto store_command(ProjectEditKind kind,
                       TargetId target,
                       StringId source,
                       StringId destination = {},
                       StringId contents = {}) -> ProjectEditId;
    auto apply_internal(ProjectEditId id) -> std::expected<ProjectEditId, ProjectEditError>;
    auto stored_path(StringId path) const -> std::filesystem::path;

    std::filesystem::path path_;
    std::unique_ptr<CompilerStorage> storage_;
    codegen::sexpr::SourceFileId source_;
    Project project_;
    std::map<TargetId, ProjectSourceListRange> source_lists_;
    std::map<StringId, PendingProjectSource> pending_sources_;
    std::map<StringId, StringId> pending_renames_;
    std::vector<std::uint32_t> source_item_begins_;
    std::vector<std::uint32_t> source_item_ends_;
    std::vector<ProjectEditId> history_;
    std::size_t history_position_{};
    std::size_t saved_history_position_{};
    std::uint64_t revision_{};
};

}
