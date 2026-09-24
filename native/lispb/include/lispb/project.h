#pragma once

#include <cstdint>
#include <expected>
#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace lispb {

enum class PathBase { project, build };

struct RootedPath {
    PathBase base{PathBase::project};
    std::filesystem::path path;
};

struct CppSchemaTarget {
    std::string name;
    std::filesystem::path types;
    std::vector<std::filesystem::path> sources;
    RootedPath output_root;
};

struct SlateTarget {
    std::string name;
    std::vector<std::filesystem::path> sources;
    std::vector<std::filesystem::path> include_directories;
    RootedPath output_root;
};

enum class KernelProfile { unreal, standard, unreal_avx2_lab, native_x86_simd_lab };

struct KernelTarget {
    std::string name;
    std::vector<std::filesystem::path> sources;
    KernelProfile profile{KernelProfile::unreal};
    RootedPath output_root;
};

struct MaterialTarget {
    std::string name;
    std::filesystem::path source;
    RootedPath artifact;
};

using Target = std::variant<CppSchemaTarget, SlateTarget, KernelTarget, MaterialTarget>;

struct Project {
    std::filesystem::path root;
    std::map<std::string, Target, std::less<>> targets;
    std::map<std::string, std::vector<std::string>, std::less<>> groups;
};

struct ProjectEditError {
    std::string message;
};

struct AddCppSchemaSource {
    std::string target_name;
    std::filesystem::path source;
};

struct RemoveCppSchemaSource {
    std::string target_name;
    std::filesystem::path source;
};

struct RenameCppSchemaSource {
    std::string target_name;
    std::filesystem::path source;
    std::filesystem::path destination;
};

struct CreateCppSchemaSource {
    std::string target_name;
    std::filesystem::path source;
    std::string contents;
};

struct DeletePendingCppSchemaSource {
    std::string target_name;
    std::filesystem::path source;
};

using ProjectEditCommand = std::variant<AddCppSchemaSource,
                                        RemoveCppSchemaSource,
                                        RenameCppSchemaSource,
                                        CreateCppSchemaSource,
                                        DeletePendingCppSchemaSource>;

struct ProjectSourceUpdate {
    std::filesystem::path path;
    std::string original;
    std::string updated;
};

class EditableProjectDocument {
  public:
    [[nodiscard]] auto project() const -> Project const& { return project_; }
    [[nodiscard]] auto path() const -> std::filesystem::path const& { return path_; }
    [[nodiscard]] auto source_text() const -> std::string const& { return source_; }
    [[nodiscard]] auto dirty() const -> bool {
        return history_position_ != saved_history_position_;
    }
    [[nodiscard]] auto can_undo() const -> bool { return history_position_ != 0; }
    [[nodiscard]] auto can_redo() const -> bool { return history_position_ < history_.size(); }
    [[nodiscard]] auto revision() const -> std::uint64_t { return revision_; }
    [[nodiscard]] auto source_is_pending(std::filesystem::path const& source) const -> bool {
        return pending_sources_.contains(source.lexically_normal());
    }
    [[nodiscard]] auto renamed_source_original(std::filesystem::path const& source) const
        -> std::optional<std::filesystem::path> {
        auto const found{pending_renames_.find(source.lexically_normal())};
        return found == pending_renames_.end() ? std::nullopt : std::optional{found->second};
    }

    auto apply(ProjectEditCommand const& command) -> std::expected<bool, ProjectEditError>;
    auto undo() -> std::expected<bool, ProjectEditError>;
    auto redo() -> std::expected<bool, ProjectEditError>;
    void discard_redo_history();
    [[nodiscard]] auto preview_source_updates() const
        -> std::expected<std::vector<ProjectSourceUpdate>, ProjectEditError>;
    auto save() -> std::expected<bool, ProjectEditError>;
  private:
    struct SourceListItem {
        std::filesystem::path source;
        std::size_t begin_offset{};
        std::size_t end_offset{};
    };

    struct SourceListRange {
        std::size_t begin_offset{};
        std::size_t end_offset{};
        std::size_t closing_offset{};
        std::size_t item_indentation{};
        std::vector<std::filesystem::path> original_sources;
        std::vector<SourceListItem> original_items;
    };

    struct PendingSource {
        std::string target_name;
        std::string contents;
    };

    friend auto load_editable_project_document(std::filesystem::path const& path)
        -> EditableProjectDocument;
    auto apply_internal(ProjectEditCommand const& command)
        -> std::expected<ProjectEditCommand, ProjectEditError>;

    std::filesystem::path path_;
    std::string source_;
    Project project_;
    std::map<std::string, SourceListRange, std::less<>> source_lists_;
    std::map<std::filesystem::path, PendingSource> pending_sources_;
    std::map<std::filesystem::path, std::filesystem::path> pending_renames_;
    std::vector<ProjectEditCommand> history_;
    std::size_t history_position_{};
    std::size_t saved_history_position_{};
    std::uint64_t revision_{};
};

auto load_project(std::filesystem::path const& path) -> Project;
auto load_editable_project_document(std::filesystem::path const& path) -> EditableProjectDocument;
auto source_files(Target const& target) -> std::vector<std::filesystem::path>;
auto resolve(RootedPath const& path,
             std::filesystem::path const& project_root,
             std::filesystem::path const& build_root) -> std::filesystem::path;
auto expand_target(Project const& project, std::string const& name) -> std::vector<std::string>;

} // namespace lispb
