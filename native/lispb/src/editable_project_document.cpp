#include <lispb/editable_project_document.h>

#include <codegen/path_utils.h>
#include <codegen/sexpr/source_span.h>
#include <codegen/sexpr/syntax.h>
#include <codegen/source_loader.h>
#include <lispb/compiler_storage.h>
#include <lispb/indices.h>
#include <lispb/pending_project_source.h>
#include <lispb/project.h>
#include <lispb/project_edit_error.h>
#include <lispb/project_edit_id.h>
#include <lispb/project_edit_kind.h>
#include <lispb/project_edit_request.h>
#include <lispb/project_loader.h>
#include <lispb/project_source_update.h>
#include <lispb/schema/type_graph.h>
#include <lispb/target_id.h>
#include <lispb/target_kind.h>

#include <algorithm>
#include <cassert>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <expected>
#include <filesystem>
#include <fstream>
#include <functional>
#include <ios>
#include <iterator>
#include <map>
#include <memory>
#include <optional>
#include <ranges>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#if defined(_WIN32)
#include <Windows.h>
#endif

namespace lispb {
namespace {

using lispb::codegen::sexpr::SourceSpan;

auto read_file(std::filesystem::path const& path) -> std::string {
    std::ifstream input{path, std::ios::binary};
    if (!input) {
        throw std::runtime_error{"Cannot read Lispb project: " + path.string()};
    }
    return std::string{std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{}};
}

auto quote(std::filesystem::path const& path) -> std::string {
    auto const value{path.generic_string()};
    std::string result{"\""};
    for (auto const character : value) {
        if (character == '\\' || character == '"') {
            result += '\\';
        }
        result += character;
    }
    result += '"';
    return result;
}

auto normalized_source_path(std::filesystem::path const& source) -> std::filesystem::path {
    auto const normalized{source.lexically_normal()};
    if (normalized.empty() || normalized == "." || normalized.is_absolute() ||
        normalized.has_root_path()) {
        throw std::invalid_argument{"C++ schema source path must be a non-empty relative path"};
    }
    for (auto const& component : normalized) {
        if (component == "..") {
            throw std::invalid_argument{"C++ schema source path must stay within the project root"};
        }
    }
    return normalized;
}

auto path_entry_exists(std::filesystem::path const& path) -> bool {
    std::error_code error;
    auto const status{std::filesystem::symlink_status(path, error)};
    if (error) {
        if (error == std::errc::no_such_file_or_directory) {
            return false;
        }
        throw std::filesystem::filesystem_error{"Cannot inspect path", path, error};
    }
    return status.type() != std::filesystem::file_type::not_found;
}

void require_project_source_location(Project const& project, std::filesystem::path const& path) {
    auto const root{std::filesystem::weakly_canonical(project.root)};
    auto const parent{std::filesystem::weakly_canonical(path.parent_path())};
    auto const relative{parent.lexically_relative(root)};
    if (relative.empty() || relative.is_absolute() || relative.has_root_path() ||
        std::ranges::find(relative, std::filesystem::path{".."}) != relative.end()) {
        throw std::invalid_argument{"C++ schema source path escapes the project root: " +
                                    path.string()};
    }
}

void write_file(std::filesystem::path const& path,
                std::string_view const contents,
                std::string_view const purpose) {
    std::ofstream output{path, std::ios::binary | std::ios::trunc};
    output.write(contents.data(), static_cast<std::streamsize>(contents.size()));
    output.close();
    if (!output) {
        throw std::runtime_error{"Cannot write " + std::string{purpose} + ": " + path.string()};
    }
}

struct PendingSourceValidation {
    std::filesystem::path source;
    std::string_view contents;
};

void validate_cpp_schema_target(Project const& project,
                                TargetId const target,
                                std::span<std::filesystem::path const> const candidate_sources,
                                std::span<PendingSourceValidation const> const pending = {}) {
    assert(target.kind() == TargetKind::cpp_schema);
    CompilerStorage storage;
    for (auto const& value : pending) {
        static_cast<void>(storage.sources.overlay(project.root / value.source, value.contents));
    }
    std::vector<std::filesystem::path> sources;
    sources.reserve(candidate_sources.size());
    for (auto const& source : candidate_sources) {
        sources.push_back(project.root / source);
    }
    auto manifest{
        lispb::codegen::load_sources(storage, project.root / project.types(target), sources)};
    static_cast<void>(schema::resolve_type_graph(manifest));
}

void replace_file(std::filesystem::path const& source, std::filesystem::path const& destination) {
#if defined(_WIN32)
    if (!MoveFileExW(source.c_str(),
                     destination.c_str(),
                     MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        throw std::filesystem::filesystem_error{
            "Cannot replace LispB project file",
            source,
            destination,
            std::error_code{static_cast<int>(GetLastError()), std::system_category()}};
    }
#else
    std::filesystem::rename(source, destination);
#endif
}

void publish_new_file(std::filesystem::path const& source,
                      std::filesystem::path const& destination) {
    if (path_entry_exists(destination)) {
        throw std::invalid_argument{"C++ schema source destination already exists: " +
                                    destination.string()};
    }
#if defined(_WIN32)
    if (!MoveFileExW(source.c_str(), destination.c_str(), MOVEFILE_WRITE_THROUGH)) {
        throw std::filesystem::filesystem_error{
            "Cannot publish new LispB schema source",
            source,
            destination,
            std::error_code{static_cast<int>(GetLastError()), std::system_category()}};
    }
#else
    std::filesystem::rename(source, destination);
#endif
}

} // namespace

EditableProjectDocument::EditableProjectDocument()
    : storage_{std::make_unique<CompilerStorage>()}
    , project_{storage_->strings} {}

EditableProjectDocument::~EditableProjectDocument() = default;
EditableProjectDocument::EditableProjectDocument(EditableProjectDocument&&) noexcept = default;
auto EditableProjectDocument::operator=(EditableProjectDocument&&) noexcept
    -> EditableProjectDocument& = default;

auto EditableProjectDocument::source_text() const -> std::string_view {
    return storage_->sources.text(source_);
}

auto EditableProjectDocument::stored_path(StringId const path) const -> std::filesystem::path {
    return std::filesystem::path{storage_->strings.view(path)};
}
auto EditableProjectDocument::source_is_pending(std::filesystem::path const& source) const -> bool {
    return pending_sources_.contains(
        string_id(storage_->strings.find(source.lexically_normal().generic_string())));
}
auto EditableProjectDocument::renamed_source_original(std::filesystem::path const& source) const
    -> std::optional<std::filesystem::path> {
    auto const found{pending_renames_.find(
        string_id(storage_->strings.find(source.lexically_normal().generic_string())))};
    return found == pending_renames_.end() ? std::nullopt
                                           : std::optional{stored_path(found->second)};
}

auto EditableProjectDocument::store_command(ProjectEditKind const kind,
                                            TargetId const target,
                                            StringId const source,
                                            StringId const destination,
                                            StringId const contents) -> ProjectEditId {
    auto index{source_commands_.targets.size()};
    if (kind == ProjectEditKind::rename_source) {
        index = rename_commands_.targets.size();
        rename_commands_.targets.push_back(target);
        rename_commands_.sources.push_back(source);
        rename_commands_.destinations.push_back(destination);
    } else if (kind == ProjectEditKind::create_source) {
        index = create_commands_.targets.size();
        create_commands_.targets.push_back(target);
        create_commands_.sources.push_back(source);
        create_commands_.contents.push_back(contents);
    } else {
        source_commands_.targets.push_back(target);
        source_commands_.sources.push_back(source);
    }
    assert(index < 0x01000000u);
    return {static_cast<std::uint32_t>(kind) << 24 | static_cast<std::uint32_t>(index)};
}

auto EditableProjectDocument::apply_internal(ProjectEditId const id)
    -> std::expected<ProjectEditId, ProjectEditError> {
    try {
        auto const kind{id.kind()};
        auto const index{id.index()};
        auto const target{kind == ProjectEditKind::rename_source ? rename_commands_.targets[index]
                          : kind == ProjectEditKind::create_source
                              ? create_commands_.targets[index]
                              : source_commands_.targets[index]};
        auto const source_id{
            kind == ProjectEditKind::rename_source   ? rename_commands_.sources[index]
            : kind == ProjectEditKind::create_source ? create_commands_.sources[index]
                                                     : source_commands_.sources[index]};
        auto const source{stored_path(source_id)};
        assert(target.kind() == TargetKind::cpp_schema && source_lists_.contains(target));
        auto candidate_sources{project_.sources(target)};
        auto const same_source{[&](std::filesystem::path const& existing) {
            return existing.lexically_normal() == source;
        }};
        auto const validate_candidate{[&](PendingSourceValidation const* const additional = nullptr,
                                          std::map<StringId, StringId> const* const renames =
                                              nullptr) {
            std::vector<PendingSourceValidation> pending;
            for (auto const& [pending_source, pending_value] : pending_sources_) {
                if (pending_value.target == target) {
                    pending.push_back({.source = stored_path(pending_source),
                                       .contents = storage_->strings.view(pending_value.contents)});
                }
            }
            auto const& current_renames{renames == nullptr ? pending_renames_ : *renames};
            std::vector<std::string> rename_contents;
            rename_contents.reserve(current_renames.size());
            for (auto const& [destination, original] : current_renames) {
                rename_contents.push_back(read_file(project_.root / stored_path(original)));
                pending.push_back(
                    {.source = stored_path(destination), .contents = rename_contents.back()});
            }
            if (additional != nullptr) {
                pending.push_back(*additional);
            }
            validate_cpp_schema_target(project_, target, candidate_sources, pending);
        }};
        if (kind == ProjectEditKind::add_source) {
            if (std::ranges::any_of(candidate_sources, same_source)) {
                throw std::invalid_argument{"C++ schema source is already registered: " +
                                            source.generic_string()};
            }
            if (!std::filesystem::is_regular_file(project_.root / source)) {
                throw std::invalid_argument{"C++ schema source does not exist: " +
                                            (project_.root / source).string()};
            }
            if (std::ranges::any_of(pending_renames_, [&](auto const& rename) {
                    return lispb::codegen::output_path_key(stored_path(rename.second)) ==
                           lispb::codegen::output_path_key(source);
                })) {
                throw std::invalid_argument{
                    "Undo the staged rename before registering its original source"};
            }
            auto const original{project_.sources(source_lists_.at(target).original_sources)};
            auto const original_source{std::ranges::find_if(original, same_source)};
            if (original_source == original.end()) {
                candidate_sources.push_back(source);
            } else {
                auto insertion{candidate_sources.begin()};
                for (auto preceding{original.begin()}; preceding != original_source; ++preceding) {
                    auto const present{std::ranges::find_if(
                        candidate_sources, [&](std::filesystem::path const& existing) {
                            return existing.lexically_normal() == preceding->lexically_normal();
                        })};
                    if (present != candidate_sources.end() && present >= insertion) {
                        insertion = std::next(present);
                    }
                }
                candidate_sources.insert(insertion, source);
            }
            validate_candidate();
            project_.replace_sources(target, candidate_sources);
            return store_command(ProjectEditKind::remove_source, target, source_id);
        } else if (kind == ProjectEditKind::remove_source) {
            if (pending_sources_.contains(source_id)) {
                throw std::invalid_argument{"Pending schema sources must be removed "
                                            "through their creation history"};
            }
            if (pending_renames_.contains(source_id)) {
                throw std::invalid_argument{
                    "Undo the staged rename before unregistering this source"};
            }
            auto const existing{std::ranges::find_if(candidate_sources, same_source)};
            if (existing == candidate_sources.end()) {
                throw std::invalid_argument{"C++ schema source is not registered: " +
                                            source.generic_string()};
            }
            candidate_sources.erase(existing);
            validate_candidate();
            project_.replace_sources(target, candidate_sources);
            return store_command(ProjectEditKind::add_source, target, source_id);
        } else if (kind == ProjectEditKind::rename_source) {
            auto const destination_id{rename_commands_.destinations[index]};
            auto const destination{stored_path(destination_id)};
            if (source == destination) {
                throw std::invalid_argument{"New C++ schema source path is unchanged"};
            }
            if (pending_sources_.contains(source_id)) {
                throw std::invalid_argument{"Save the pending source creation before renaming it"};
            }
            auto const existing{std::ranges::find_if(candidate_sources, same_source)};
            if (existing == candidate_sources.end()) {
                throw std::invalid_argument{"C++ schema source is not registered: " +
                                            source.generic_string()};
            }
            auto const renamed{pending_renames_.find(source_id)};
            auto const original{renamed == pending_renames_.end() ? source
                                                                  : stored_path(renamed->second)};
            auto const original_path{project_.root / original};
            if (!std::filesystem::is_regular_file(original_path) ||
                std::filesystem::is_symlink(original_path)) {
                throw std::invalid_argument{"C++ schema source does not exist: " +
                                            original_path.string()};
            }
            require_project_source_location(project_, original_path);
            auto const destination_key{lispb::codegen::output_path_key(destination)};
            auto const original_key{lispb::codegen::output_path_key(original)};
            for (auto const candidate_target : project_.target_ids()) {
                if (candidate_target.kind() == TargetKind::cpp_schema &&
                    lispb::codegen::output_path_key(project_.types(candidate_target)) ==
                        original_key) {
                    throw std::invalid_argument{
                        "A types registry cannot be renamed as a schema source"};
                }
                for (auto const& registered : project_.source_files(candidate_target)) {
                    auto const key{lispb::codegen::output_path_key(registered)};
                    if (candidate_target != target && key == original_key) {
                        throw std::invalid_argument{
                            "Source is shared by another target and cannot be renamed "
                            "here"};
                    }
                    if (key == destination_key && registered != source) {
                        throw std::invalid_argument{
                            "C++ schema source destination is already registered: " +
                            destination.generic_string()};
                    }
                }
            }
            if (pending_sources_.contains(destination_id)) {
                throw std::invalid_argument{"C++ schema source destination is pending creation: " +
                                            destination.generic_string()};
            }
            auto const destination_path{project_.root / destination};
            require_project_source_location(project_, destination_path);
            if (destination != original && path_entry_exists(destination_path)) {
                throw std::invalid_argument{"C++ schema source destination already exists: " +
                                            destination_path.string()};
            }
            if (!std::filesystem::is_directory(destination_path.parent_path())) {
                throw std::invalid_argument{"C++ schema source parent directory does not exist: " +
                                            destination_path.parent_path().string()};
            }

            *existing = destination;
            auto updated_renames{pending_renames_};
            updated_renames.erase(source_id);
            if (destination != original) {
                updated_renames.emplace(destination_id,
                                        renamed == pending_renames_.end() ? source_id
                                                                          : renamed->second);
            }
            validate_candidate(nullptr, &updated_renames);
            project_.replace_sources(target, candidate_sources);
            pending_renames_ = std::move(updated_renames);
            return store_command(ProjectEditKind::rename_source, target, destination_id, source_id);
        } else if (kind == ProjectEditKind::create_source) {
            auto const contents_id{create_commands_.contents[index]};
            if (std::ranges::any_of(candidate_sources, same_source)) {
                throw std::invalid_argument{"C++ schema source is already registered: " +
                                            source.generic_string()};
            }
            if (pending_sources_.contains(source_id)) {
                throw std::invalid_argument{"C++ schema source is already pending: " +
                                            source.generic_string()};
            }
            auto const destination{project_.root / source};
            if (path_entry_exists(destination)) {
                throw std::invalid_argument{"C++ schema source destination already exists: " +
                                            destination.string()};
            }
            if (!std::filesystem::is_directory(destination.parent_path())) {
                throw std::invalid_argument{"C++ schema source parent directory does not exist: " +
                                            destination.parent_path().string()};
            }

            candidate_sources.push_back(source);
            auto const additional{PendingSourceValidation{
                .source = source, .contents = storage_->strings.view(contents_id)}};
            validate_candidate(&additional);
            project_.replace_sources(target, candidate_sources);
            pending_sources_.emplace(
                source_id, PendingProjectSource{.target = target, .contents = contents_id});
            return store_command(ProjectEditKind::delete_pending_source, target, source_id);
        } else {
            auto const pending{pending_sources_.find(source_id)};
            if (pending == pending_sources_.end() || pending->second.target != target) {
                throw std::invalid_argument{"C++ schema source is not pending: " +
                                            source.generic_string()};
            }
            auto const existing{std::ranges::find_if(candidate_sources, same_source)};
            if (existing == candidate_sources.end()) {
                throw std::logic_error{"Pending C++ schema source is not registered: " +
                                       source.generic_string()};
            }
            candidate_sources.erase(existing);
            validate_candidate();
            auto contents{pending->second.contents};
            project_.replace_sources(target, candidate_sources);
            pending_sources_.erase(pending);
            return store_command(ProjectEditKind::create_source, target, source_id, {}, contents);
        }
    } catch (std::exception const& error) {
        return std::unexpected{ProjectEditError{error.what()}};
    }
}

auto EditableProjectDocument::apply(ProjectEditRequest const& request)
    -> std::expected<bool, ProjectEditError> {
    ProjectEditId id;
    try {
        auto const target{project_.find_target(request.target_name)};
        if (!target) {
            throw std::invalid_argument{"Unknown Lispb target '" + request.target_name + "'"};
        }
        if (target->kind() != TargetKind::cpp_schema || !source_lists_.contains(*target)) {
            throw std::invalid_argument{"Lispb target '" + request.target_name +
                                        "' is not an editable C++ schema target"};
        }
        auto const source{normalized_source_path(request.source)};
        auto const destination{request.kind == ProjectEditKind::rename_source
                                   ? normalized_source_path(request.destination)
                                   : std::filesystem::path{}};
        auto const source_id{string_id(storage_->strings.intern(source.generic_string()))};
        auto const destination_id{
            string_id(storage_->strings.intern(destination.generic_string()))};
        auto const contents{request.kind == ProjectEditKind::create_source
                                ? storage_->strings.add(request.contents)
                                : StringId{}};
        id = store_command(request.kind, *target, source_id, destination_id, contents);
    } catch (std::exception const& error) {
        return std::unexpected{ProjectEditError{error.what()}};
    }
    auto inverse{apply_internal(id)};
    if (!inverse.has_value()) {
        return std::unexpected{std::move(inverse.error())};
    }
    history_.resize(history_position_);
    history_.push_back(std::move(*inverse));
    ++history_position_;
    ++revision_;
    return true;
}

auto EditableProjectDocument::undo() -> std::expected<bool, ProjectEditError> {
    if (!can_undo()) {
        return false;
    }
    auto inverse{apply_internal(history_[history_position_ - 1])};
    if (!inverse.has_value()) {
        return std::unexpected{std::move(inverse.error())};
    }
    history_[history_position_ - 1] = std::move(*inverse);
    --history_position_;
    ++revision_;
    return true;
}

auto EditableProjectDocument::redo() -> std::expected<bool, ProjectEditError> {
    if (!can_redo()) {
        return false;
    }
    auto inverse{apply_internal(history_[history_position_])};
    if (!inverse.has_value()) {
        return std::unexpected{std::move(inverse.error())};
    }
    history_[history_position_] = std::move(*inverse);
    ++history_position_;
    ++revision_;
    return true;
}

void EditableProjectDocument::discard_redo_history() {
    history_.resize(history_position_);
}

auto EditableProjectDocument::preview_source_updates() const
    -> std::expected<std::vector<ProjectSourceUpdate>, ProjectEditError> {
    struct Replacement {
        std::size_t begin{};
        std::size_t end{};
        std::string text;
    };
    struct SourceListItem {
        std::filesystem::path source;
        std::uint32_t begin_offset{};
        std::uint32_t end_offset{};
    };
    try {
        std::vector<Replacement> replacements;
        for (auto const& [target, range] : source_lists_) {
            auto const target_sources{project_.sources(target)};
            auto const original_sources{project_.sources(range.original_sources)};
            auto const same_path{
                [](std::filesystem::path const& first, std::filesystem::path const& second) {
                    return first.lexically_normal() == second.lexically_normal();
                }};
            if (std::ranges::equal(original_sources, target_sources, same_path)) {
                continue;
            }
            auto rendered{std::string{
                source_text().substr(range.begin_offset, range.end_offset - range.begin_offset)}};
            auto insertion_offset{range.closing_offset - range.begin_offset};
            auto const multiline{rendered.contains('\n')};
            if (multiline) {
                while (insertion_offset > 1 && std::isspace(static_cast<unsigned char>(
                                                   rendered[insertion_offset - 1])) != 0) {
                    --insertion_offset;
                }
            }
            std::string insertion;
            for (auto const& source : target_sources) {
                if (std::ranges::any_of(
                        original_sources,
                        [&](auto const& original) { return same_path(original, source); }) ||
                    std::ranges::any_of(pending_renames_, [&](auto const& rename) {
                        return same_path(stored_path(rename.first), source) &&
                               std::ranges::any_of(original_sources, [&](auto const& original) {
                                   return same_path(original, stored_path(rename.second));
                               });
                    })) {
                    continue;
                }
                if (multiline) {
                    insertion += "\n" + std::string(range.item_indentation, ' ');
                } else if (!target_sources.empty()) {
                    insertion += ' ';
                }
                insertion += quote(source);
            }
            rendered.insert(insertion_offset, insertion);
            std::vector<SourceListItem> removed_items;
            for (std::uint32_t index{}; index < range.original_items.count; ++index) {
                auto const offset{range.original_items.offset + index};
                SourceListItem const item{original_sources[index],
                                          source_item_begins_[offset],
                                          source_item_ends_[offset]};
                if (!std::ranges::any_of(target_sources, [&](auto const& current) {
                        return same_path(current, item.source);
                    })) {
                    removed_items.push_back(item);
                }
            }
            std::ranges::sort(removed_items, std::greater{}, &SourceListItem::begin_offset);
            for (auto const& item : removed_items) {
                auto const begin{item.begin_offset - range.begin_offset};
                auto const renamed{std::ranges::find_if(pending_renames_, [&](auto const& rename) {
                    return same_path(stored_path(rename.second), item.source) &&
                           std::ranges::any_of(target_sources, [&](auto const& source) {
                               return same_path(source, stored_path(rename.first));
                           });
                })};
                if (renamed == pending_renames_.end()) {
                    rendered.erase(begin, item.end_offset - item.begin_offset);
                } else {
                    rendered.replace(begin,
                                     item.end_offset - item.begin_offset,
                                     quote(stored_path(renamed->first)));
                }
            }
            replacements.push_back({.begin = range.begin_offset,
                                    .end = range.end_offset,
                                    .text = std::move(rendered)});
        }
        std::vector<ProjectSourceUpdate> updates;
        if (!replacements.empty()) {
            std::ranges::sort(replacements, std::greater{}, &Replacement::begin);
            auto updated{std::string{source_text()}};
            for (auto const& replacement : replacements) {
                updated.replace(
                    replacement.begin, replacement.end - replacement.begin, replacement.text);
            }
            updates.push_back({.path = path_,
                               .original = std::string{source_text()},
                               .updated = std::move(updated)});
        }
        for (auto const& [source, pending] : pending_sources_) {
            updates.push_back({.path = project_.root / stored_path(source),
                               .original = {},
                               .updated = std::string{storage_->strings.view(pending.contents)}});
        }
        return updates;
    } catch (std::exception const& error) {
        return std::unexpected{ProjectEditError{error.what()}};
    }
}

auto EditableProjectDocument::save() -> std::expected<bool, ProjectEditError> {
    auto updates{preview_source_updates()};
    if (!updates.has_value()) {
        return std::unexpected{std::move(updates.error())};
    }
    if (updates->empty()) {
        saved_history_position_ = history_position_;
        return false;
    }

    auto const manifest_update{std::ranges::find(*updates, path_, &ProjectSourceUpdate::path)};
    if (manifest_update == updates->end()) {
        return std::unexpected{
            ProjectEditError{"Editable project changes did not produce a manifest update"}};
    }

    struct StagedSource {
        TargetId target;
        std::filesystem::path source;
        std::filesystem::path destination;
        std::filesystem::path temporary;
    };
    struct StagedRename {
        std::filesystem::path original;
        std::filesystem::path destination;
    };
    std::vector<StagedSource> staged_sources;
    staged_sources.reserve(pending_sources_.size());
    std::vector<StagedRename> staged_renames;
    staged_renames.reserve(pending_renames_.size());
    auto temporary_manifest{path_};
    temporary_manifest += ".layout-planner.tmp";
    std::vector<std::filesystem::path> created_temporaries;
    std::vector<std::filesystem::path> published_sources;
    std::vector<StagedRename> published_renames;
    auto manifest_published{false};

    try {
        if (read_file(path_) != source_text()) {
            throw std::invalid_argument{
                "LispB project changed on disk; reload it before saving source-list edits"};
        }
        if (path_entry_exists(temporary_manifest)) {
            throw std::invalid_argument{"Temporary LispB project path already exists: " +
                                        temporary_manifest.string()};
        }

        for (auto const& [source, pending] : pending_sources_) {
            auto const destination{project_.root / stored_path(source)};
            if (path_entry_exists(destination)) {
                throw std::invalid_argument{"C++ schema source destination already exists: " +
                                            destination.string()};
            }
            if (!std::filesystem::is_directory(destination.parent_path())) {
                throw std::invalid_argument{"C++ schema source parent directory does not exist: " +
                                            destination.parent_path().string()};
            }
            auto temporary{destination};
            temporary += ".layout-planner.tmp";
            if (path_entry_exists(temporary)) {
                throw std::invalid_argument{"Temporary schema source path already exists: " +
                                            temporary.string()};
            }
            created_temporaries.push_back(temporary);
            write_file(temporary,
                       storage_->strings.view(pending.contents),
                       "temporary LispB schema source");
            staged_sources.push_back({.target = pending.target,
                                      .source = stored_path(source),
                                      .destination = destination,
                                      .temporary = std::move(temporary)});
        }

        for (auto const& [destination, original] : pending_renames_) {
            auto const original_path{project_.root / stored_path(original)};
            auto const destination_path{project_.root / stored_path(destination)};
            require_project_source_location(project_, original_path);
            require_project_source_location(project_, destination_path);
            if (!std::filesystem::is_regular_file(original_path) ||
                std::filesystem::is_symlink(original_path)) {
                throw std::invalid_argument{"C++ schema source cannot be renamed: " +
                                            original_path.string()};
            }
            if (path_entry_exists(destination_path)) {
                throw std::invalid_argument{"C++ schema source destination already exists: " +
                                            destination_path.string()};
            }
            if (!std::filesystem::is_directory(destination_path.parent_path())) {
                throw std::invalid_argument{"C++ schema source parent directory does not exist: " +
                                            destination_path.parent_path().string()};
            }
            staged_renames.push_back({.original = original_path, .destination = destination_path});
        }

        created_temporaries.push_back(temporary_manifest);
        write_file(
            temporary_manifest, manifest_update->updated, "temporary LispB project manifest");

        for (auto const& [target, range] : source_lists_) {
            auto sources{project_.sources(target)};
            if (sources == project_.sources(range.original_sources)) {
                continue;
            }
            for (auto& source : sources) {
                auto const normalized{source.lexically_normal()};
                auto const staged{
                    std::ranges::find(staged_sources, normalized, &StagedSource::source)};
                if (staged != staged_sources.end()) {
                    source = staged->temporary.lexically_relative(project_.root);
                } else if (auto const renamed{pending_renames_.find(
                               string_id(storage_->strings.find(normalized.generic_string())))};
                           renamed != pending_renames_.end()) {
                    source = stored_path(renamed->second);
                }
            }
            validate_cpp_schema_target(project_, target, sources);
        }

        for (auto const& staged : staged_sources) {
            publish_new_file(staged.temporary, staged.destination);
            published_sources.push_back(staged.destination);
        }
        for (auto const& staged : staged_renames) {
            publish_new_file(staged.original, staged.destination);
            published_renames.push_back(staged);
        }
        replace_file(temporary_manifest, path_);
        manifest_published = true;
        *this = load_editable_project_document(path_);
        return true;
    } catch (std::exception const& error) {
        auto message{std::string{error.what()}};
        for (auto const& temporary : created_temporaries) {
            std::error_code ignored;
            std::filesystem::remove(temporary, ignored);
        }
        if (!manifest_published) {
            for (auto renamed{published_renames.rbegin()}; renamed != published_renames.rend();
                 ++renamed) {
                try {
                    publish_new_file(renamed->destination, renamed->original);
                } catch (std::exception const& rollback_error) {
                    message += "; failed to roll back renamed source '" +
                               renamed->destination.string() + "': " + rollback_error.what();
                }
            }
            for (auto const& published : published_sources) {
                std::error_code rollback_error;
                std::filesystem::remove(published, rollback_error);
                if (rollback_error) {
                    message += "; failed to roll back published source '" + published.string() +
                               "': " + rollback_error.message();
                }
            }
        }
        return std::unexpected{ProjectEditError{std::move(message)}};
    }
}

} // namespace lispb
