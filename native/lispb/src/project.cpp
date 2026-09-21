#include <lispb/project.h>
#include <lispb/schema/type_graph.h>

#include <codegen/path_utils.h>
#include <codegen/sexpr/fields.h>
#include <codegen/source_loader.h>

#include <algorithm>
#include <cctype>
#include <fstream>
#include <ranges>
#include <set>
#include <span>
#include <stdexcept>
#include <string_view>
#include <type_traits>

#if defined(_WIN32)
#include <Windows.h>
#endif

namespace lispb {
namespace {

using codegen::sexpr::Fields;
using codegen::sexpr::Form;
using codegen::sexpr::SourceError;
using codegen::sexpr::SourceSpan;

[[noreturn]] void fail(SourceSpan const& span, std::string const& message) {
    throw SourceError{span.path, span, message};
}

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

auto token_end_offset(std::string_view const source, Form const& form) -> std::size_t {
    auto const offset{form.token.span.offset};
    if (form.token.kind != codegen::sexpr::TokenKind::string) {
        return offset + form.token.text.size();
    }
    auto escaped{false};
    for (auto index{offset + 1}; index < source.size(); ++index) {
        auto const character{source[index]};
        if (!escaped && character == '"') {
            return index + 1;
        }
        if (!escaped && character == '\\') {
            escaped = true;
        } else {
            escaped = false;
        }
    }
    throw std::logic_error{"Project source string has no closing quote"};
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

auto validation_source_path(Project const& project, std::uint64_t const revision)
    -> std::filesystem::path {
    for (std::uint32_t suffix{}; suffix != 1'000; ++suffix) {
        auto const candidate{project.root /
                             (".layout-planner-source-validation-" + std::to_string(revision) +
                              "-" + std::to_string(suffix) + ".lispb")};
        if (!path_entry_exists(candidate)) {
            return candidate;
        }
    }
    throw std::runtime_error{"Cannot allocate a temporary schema source in project root: " +
                             project.root.string()};
}

void validate_cpp_schema_target(Project const& project, std::string const& target_name) {
    auto const found{project.targets.find(target_name)};
    if (found == project.targets.end()) {
        throw std::invalid_argument{"Unknown Lispb target '" + target_name + "'"};
    }
    auto const* target{std::get_if<CppSchemaTarget>(&found->second)};
    if (target == nullptr) {
        throw std::invalid_argument{"Lispb target '" + target_name +
                                    "' is not a C++ schema target"};
    }
    std::vector<std::filesystem::path> sources;
    sources.reserve(target->sources.size());
    for (auto const& source : target->sources) {
        sources.push_back(project.root / source);
    }
    auto const manifest{codegen::load_sources(project.root / target->types, sources)};
    static_cast<void>(schema::resolve_type_graph(manifest));
}

struct PendingSourceValidation {
    std::filesystem::path source;
    std::string_view contents;
};

void validate_cpp_schema_target_with_pending_sources(
    Project candidate,
    std::string const& target_name,
    std::span<PendingSourceValidation const> const pending_sources,
    std::uint64_t const revision) {
    std::vector<std::filesystem::path> temporaries;
    try {
        auto found{candidate.targets.find(target_name)};
        auto* target{found == candidate.targets.end()
                         ? nullptr
                         : std::get_if<CppSchemaTarget>(&found->second)};
        if (target == nullptr) {
            throw std::logic_error{"Pending source target disappeared from candidate project: " +
                                   target_name};
        }
        for (auto const& pending : pending_sources) {
            auto const registered{
                std::ranges::find_if(target->sources, [&](std::filesystem::path const& source) {
                    return source.lexically_normal() == pending.source;
                })};
            if (registered == target->sources.end()) {
                continue;
            }
            auto const temporary{validation_source_path(candidate, revision)};
            temporaries.push_back(temporary);
            write_file(temporary, pending.contents, "temporary LispB schema source");
            auto const temporary_source{temporary.lexically_relative(candidate.root)};
            if (temporary_source.empty() || temporary_source.has_root_path()) {
                throw std::logic_error{"Temporary schema source is outside the project root"};
            }
            *registered = temporary_source;
        }
        validate_cpp_schema_target(candidate, target_name);
    } catch (...) {
        for (auto const& temporary : temporaries) {
            std::error_code ignored;
            std::filesystem::remove(temporary, ignored);
        }
        throw;
    }
    for (auto const& temporary : temporaries) {
        std::error_code ignored;
        std::filesystem::remove(temporary, ignored);
    }
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

auto text(Form const& form, std::string_view const purpose) -> std::string {
    return codegen::sexpr::text(form, purpose, codegen::sexpr::throw_source_error);
}

auto paths(Form const& form, std::string_view const purpose) -> std::vector<std::filesystem::path> {
    auto const values{codegen::sexpr::text_list(form, purpose, codegen::sexpr::throw_source_error)};
    std::vector<std::filesystem::path> result;
    result.reserve(values.size());
    for (auto const& value : values) {
        result.emplace_back(value);
    }
    return result;
}

auto names(Form const& form, std::string_view const purpose) -> std::vector<std::string> {
    return codegen::sexpr::text_list(form, purpose, codegen::sexpr::throw_source_error);
}

auto rooted_path(Form const& form, std::string_view const purpose) -> RootedPath {
    if (!form.is_list() || form.children.size() != 2 ||
        (form.head() != "project-path" && form.head() != "build-path")) {
        fail(form.token.span,
             std::string{purpose} + " must be (project-path ...) or (build-path ...)");
    }
    auto const value{std::filesystem::path{text(form.children[1], purpose)}};
    if (value.is_absolute() || value.has_root_path()) {
        fail(form.children[1].token.span, std::string{purpose} + " must be relative");
    }
    return {form.head() == "project-path" ? PathBase::project : PathBase::build, value};
}

auto parse_profile(Form const& form) -> KernelProfile {
    auto const value{text(form, "kernel profile")};
    if (value == "unreal") {
        return KernelProfile::unreal;
    }
    if (value == "standard") {
        return KernelProfile::standard;
    }
    if (value == "unreal-avx2-lab") {
        return KernelProfile::unreal_avx2_lab;
    }
    if (value == "native-x86-simd-lab") {
        return KernelProfile::native_x86_simd_lab;
    }
    fail(form.token.span, "unknown kernel profile '" + value + "'");
}

void insert_target(Project& project,
                   std::string const& name,
                   Target target,
                   SourceSpan const& span) {
    if (project.groups.contains(name) || !project.targets.emplace(name, std::move(target)).second) {
        fail(span, "duplicate Lispb target or group '" + name + "'");
    }
}

void expand(Project const& project,
            std::string const& name,
            std::set<std::string>& active,
            std::set<std::string>& emitted,
            std::vector<std::string>& result) {
    if (project.targets.contains(name)) {
        if (emitted.insert(name).second) {
            result.push_back(name);
        }
        return;
    }
    auto const group{project.groups.find(name)};
    if (group == project.groups.end()) {
        throw std::invalid_argument{"Unknown Lispb target or group: " + name};
    }
    if (!active.insert(name).second) {
        throw std::invalid_argument{"Lispb target group cycle at: " + name};
    }
    for (auto const& member : group->second) {
        expand(project, member, active, emitted, result);
    }
    active.erase(name);
}

} // namespace

auto load_project(std::filesystem::path const& path) -> Project {
    auto const project_path{std::filesystem::absolute(path).lexically_normal()};
    auto const forms{codegen::sexpr::read_forms(project_path.string(), read_file(project_path))};
    if (forms.size() != 1 || forms.front().head() != "lispb-project") {
        auto const span{forms.empty() ? SourceSpan{.path = project_path.string()}
                                      : forms.front().token.span};
        fail(span, "project must contain exactly one 'lispb-project' form");
    }

    Fields const project_fields{forms.front(), "lispb-project", 0};
    project_fields.validate({"language-version", "project-root"},
                            {"cpp-schema", "slate", "kernel", "material", "group"});
    if (text(project_fields.required("language-version"), "language version") != "1") {
        fail(project_fields.required("language-version").token.span,
             "unsupported Lispb language version; expected 1");
    }

    Project project{.root = (project_path.parent_path() /
                             text(project_fields.required("project-root"), "project root"))
                                .lexically_normal()};
    for (auto const* declaration : project_fields.declarations()) {
        auto const head{declaration->head()};
        if (head == "cpp-schema") {
            Fields const fields{*declaration, head, 1};
            fields.validate({"types", "sources", "output-root"});
            auto const name{text(fields.positional(0), "target name")};
            insert_target(project,
                          name,
                          CppSchemaTarget{.name = name,
                                          .types = text(fields.required("types"), "types path"),
                                          .sources = paths(fields.required("sources"), "sources"),
                                          .output_root = rooted_path(fields.required("output-root"),
                                                                     "output root")},
                          declaration->token.span);
        } else if (head == "slate") {
            Fields const fields{*declaration, head, 1};
            fields.validate({"sources", "include-directories", "output-root"});
            auto const name{text(fields.positional(0), "target name")};
            auto const* includes{fields.optional("include-directories")};
            insert_target(
                project,
                name,
                SlateTarget{.name = name,
                            .sources = paths(fields.required("sources"), "sources"),
                            .include_directories = includes == nullptr
                                                     ? std::vector<std::filesystem::path>{}
                                                     : paths(*includes, "include directories"),
                            .output_root =
                                rooted_path(fields.required("output-root"), "output root")},
                declaration->token.span);
        } else if (head == "kernel") {
            Fields const fields{*declaration, head, 1};
            fields.validate({"sources"}, {"emission"});
            auto const sources{paths(fields.required("sources"), "sources")};
            for (auto const* emission : fields.declarations()) {
                Fields const emission_fields{*emission, "emission", 1};
                emission_fields.validate({"profile", "output-root"});
                auto const name{text(emission_fields.positional(0), "target name")};
                insert_target(
                    project,
                    name,
                    KernelTarget{.name = name,
                                 .sources = sources,
                                 .profile = parse_profile(emission_fields.required("profile")),
                                 .output_root = rooted_path(emission_fields.required("output-root"),
                                                            "output root")},
                    emission->token.span);
            }
        } else if (head == "material") {
            Fields const fields{*declaration, head, 1};
            fields.validate({"source", "artifact"});
            auto const name{text(fields.positional(0), "target name")};
            insert_target(project,
                          name,
                          MaterialTarget{.name = name,
                                         .source = text(fields.required("source"), "source path"),
                                         .artifact = rooted_path(fields.required("artifact"),
                                                                 "artifact path")},
                          declaration->token.span);
        } else if (head == "group") {
            Fields const fields{*declaration, head, 1};
            fields.validate({"targets"});
            auto const name{text(fields.positional(0), "group name")};
            if (project.targets.contains(name) ||
                !project.groups.emplace(name, names(fields.required("targets"), "targets"))
                     .second) {
                fail(declaration->token.span, "duplicate Lispb target or group '" + name + "'");
            }
        }
    }

    for (auto const& [name, unused] : project.groups) {
        static_cast<void>(unused);
        static_cast<void>(expand_target(project, name));
    }
    return project;
}

auto load_editable_project_document(std::filesystem::path const& path) -> EditableProjectDocument {
    EditableProjectDocument result;
    result.path_ = std::filesystem::absolute(path).lexically_normal();
    result.source_ = read_file(result.path_);
    result.project_ = load_project(result.path_);

    auto const forms{codegen::sexpr::read_forms(result.path_.string(), result.source_)};
    Fields const project_fields{forms.front(), "lispb-project", 0};
    project_fields.validate({"language-version", "project-root"},
                            {"cpp-schema", "slate", "kernel", "material", "group"});
    for (auto const* declaration : project_fields.declarations()) {
        if (declaration->head() != "cpp-schema") {
            continue;
        }
        Fields const fields{*declaration, "cpp-schema", 1};
        fields.validate({"types", "sources", "output-root"});
        auto const target_name{text(fields.positional(0), "target name")};
        auto const& sources_form{fields.required("sources")};
        auto const target{result.project_.targets.find(target_name)};
        if (target == result.project_.targets.end()) {
            throw std::logic_error{"Parsed C++ schema target is missing from the project"};
        }
        auto const* schema_target{std::get_if<CppSchemaTarget>(&target->second)};
        if (schema_target == nullptr) {
            throw std::logic_error{"Parsed C++ schema target has the wrong kind"};
        }
        auto const indentation{sources_form.children.empty()
                                   ? declaration->token.span.column + 3
                                   : sources_form.children.front().token.span.column - 1};
        std::vector<EditableProjectDocument::SourceListItem> original_items;
        original_items.reserve(sources_form.children.size());
        if (sources_form.children.size() != schema_target->sources.size()) {
            throw std::logic_error{"Parsed C++ schema source list lost its syntax items"};
        }
        for (std::size_t index{}; index < sources_form.children.size(); ++index) {
            auto const& source_form{sources_form.children[index]};
            original_items.push_back({.source = schema_target->sources[index].lexically_normal(),
                                      .begin_offset = source_form.token.span.offset,
                                      .end_offset = token_end_offset(result.source_, source_form)});
        }
        result.source_lists_.emplace(target_name,
                                     EditableProjectDocument::SourceListRange{
                                         .begin_offset = sources_form.token.span.offset,
                                         .end_offset = sources_form.closing.span.offset + 1,
                                         .closing_offset = sources_form.closing.span.offset,
                                         .item_indentation = indentation,
                                         .original_sources = schema_target->sources,
                                         .original_items = std::move(original_items)});
    }
    return result;
}

auto EditableProjectDocument::apply_internal(ProjectEditCommand const& command)
    -> std::expected<ProjectEditCommand, ProjectEditError> {
    try {
        return std::visit(
            [&](auto const& edit) -> ProjectEditCommand {
                using Edit = std::decay_t<decltype(edit)>;
                auto const source{normalized_source_path(edit.source)};
                auto const found{project_.targets.find(edit.target_name)};
                if (found == project_.targets.end()) {
                    throw std::invalid_argument{"Unknown Lispb target '" + edit.target_name + "'"};
                }
                auto* target{std::get_if<CppSchemaTarget>(&found->second)};
                if (target == nullptr || !source_lists_.contains(edit.target_name)) {
                    throw std::invalid_argument{"Lispb target '" + edit.target_name +
                                                "' is not an editable C++ schema target"};
                }
                auto candidate{project_};
                auto& candidate_sources{
                    std::get<CppSchemaTarget>(candidate.targets.at(edit.target_name)).sources};
                auto const same_source{[&](std::filesystem::path const& existing) {
                    return existing.lexically_normal() == source;
                }};
                auto const validate_candidate{
                    [&](Project const& project_candidate,
                        PendingSourceValidation const* const additional = nullptr,
                        std::map<std::filesystem::path, std::filesystem::path> const* const
                            renames = nullptr) {
                        std::vector<PendingSourceValidation> pending;
                        for (auto const& [pending_source, pending_value] : pending_sources_) {
                            if (pending_value.target_name == edit.target_name) {
                                pending.push_back(
                                    {.source = pending_source, .contents = pending_value.contents});
                            }
                        }
                        auto const& current_renames{renames == nullptr ? pending_renames_
                                                                       : *renames};
                        std::vector<std::string> rename_contents;
                        rename_contents.reserve(current_renames.size());
                        for (auto const& [destination, original] : current_renames) {
                            rename_contents.push_back(read_file(project_.root / original));
                            pending.push_back(
                                {.source = destination, .contents = rename_contents.back()});
                        }
                        if (additional != nullptr) {
                            pending.push_back(*additional);
                        }
                        validate_cpp_schema_target_with_pending_sources(
                            project_candidate, edit.target_name, pending, revision_);
                    }};
                if constexpr (std::is_same_v<Edit, AddCppSchemaSource>) {
                    if (std::ranges::any_of(target->sources, same_source)) {
                        throw std::invalid_argument{"C++ schema source is already registered: " +
                                                    source.generic_string()};
                    }
                    if (!std::filesystem::is_regular_file(project_.root / source)) {
                        throw std::invalid_argument{"C++ schema source does not exist: " +
                                                    (project_.root / source).string()};
                    }
                    if (std::ranges::any_of(pending_renames_, [&](auto const& rename) {
                            return codegen::output_path_key(rename.second) ==
                                   codegen::output_path_key(source);
                        })) {
                        throw std::invalid_argument{
                            "Undo the staged rename before registering its original source"};
                    }
                    auto const& original{source_lists_.at(edit.target_name).original_sources};
                    auto const original_source{std::ranges::find_if(original, same_source)};
                    if (original_source == original.end()) {
                        candidate_sources.push_back(source);
                    } else {
                        auto insertion{candidate_sources.begin()};
                        for (auto preceding{original.begin()}; preceding != original_source;
                             ++preceding) {
                            auto const present{std::ranges::find_if(
                                candidate_sources, [&](std::filesystem::path const& existing) {
                                    return existing.lexically_normal() ==
                                           preceding->lexically_normal();
                                })};
                            if (present != candidate_sources.end() && present >= insertion) {
                                insertion = std::next(present);
                            }
                        }
                        candidate_sources.insert(insertion, source);
                    }
                    validate_candidate(candidate);
                    project_ = std::move(candidate);
                    return RemoveCppSchemaSource{.target_name = edit.target_name, .source = source};
                } else if constexpr (std::is_same_v<Edit, RemoveCppSchemaSource>) {
                    if (pending_sources_.contains(source)) {
                        throw std::invalid_argument{"Pending schema sources must be removed "
                                                    "through their creation history"};
                    }
                    if (pending_renames_.contains(source)) {
                        throw std::invalid_argument{
                            "Undo the staged rename before unregistering this source"};
                    }
                    auto const existing{std::ranges::find_if(candidate_sources, same_source)};
                    if (existing == candidate_sources.end()) {
                        throw std::invalid_argument{"C++ schema source is not registered: " +
                                                    source.generic_string()};
                    }
                    candidate_sources.erase(existing);
                    validate_candidate(candidate);
                    project_ = std::move(candidate);
                    return AddCppSchemaSource{.target_name = edit.target_name, .source = source};
                } else if constexpr (std::is_same_v<Edit, RenameCppSchemaSource>) {
                    auto const destination{normalized_source_path(edit.destination)};
                    if (source == destination) {
                        throw std::invalid_argument{"New C++ schema source path is unchanged"};
                    }
                    if (pending_sources_.contains(source)) {
                        throw std::invalid_argument{
                            "Save the pending source creation before renaming it"};
                    }
                    auto const existing{std::ranges::find_if(candidate_sources, same_source)};
                    if (existing == candidate_sources.end()) {
                        throw std::invalid_argument{"C++ schema source is not registered: " +
                                                    source.generic_string()};
                    }
                    auto const renamed{pending_renames_.find(source)};
                    auto const original{renamed == pending_renames_.end() ? source
                                                                          : renamed->second};
                    auto const original_path{project_.root / original};
                    if (!std::filesystem::is_regular_file(original_path) ||
                        std::filesystem::is_symlink(original_path)) {
                        throw std::invalid_argument{"C++ schema source does not exist: " +
                                                    original_path.string()};
                    }
                    require_project_source_location(project_, original_path);
                    auto const destination_key{codegen::output_path_key(destination)};
                    auto const original_key{codegen::output_path_key(original)};
                    for (auto const& [name, candidate_target] : project_.targets) {
                        if (auto const* schema{std::get_if<CppSchemaTarget>(&candidate_target)}) {
                            if (codegen::output_path_key(schema->types) ==
                                codegen::output_path_key(original)) {
                                throw std::invalid_argument{
                                    "A types registry cannot be renamed as a schema source"};
                            }
                            for (auto const& registered : schema->sources) {
                                if (name != edit.target_name &&
                                    codegen::output_path_key(registered) == original_key) {
                                    throw std::invalid_argument{
                                        "Source is shared by another target and cannot be renamed "
                                        "here"};
                                }
                                if (registered != source &&
                                    codegen::output_path_key(registered) == destination_key) {
                                    throw std::invalid_argument{
                                        "C++ schema source destination is already registered: " +
                                        destination.generic_string()};
                                }
                            }
                        } else if (auto const* slate{std::get_if<SlateTarget>(&candidate_target)}) {
                            if (std::ranges::any_of(slate->sources, [&](auto const& registered) {
                                    return codegen::output_path_key(registered) == original_key;
                                })) {
                                throw std::invalid_argument{
                                    "Source is shared by another target and cannot be renamed "
                                    "here"};
                            }
                        } else if (auto const* kernel{
                                       std::get_if<KernelTarget>(&candidate_target)}) {
                            if (std::ranges::any_of(kernel->sources, [&](auto const& registered) {
                                    return codegen::output_path_key(registered) == original_key;
                                })) {
                                throw std::invalid_argument{
                                    "Source is shared by another target and cannot be renamed "
                                    "here"};
                            }
                        } else if (auto const* material{
                                       std::get_if<MaterialTarget>(&candidate_target)}) {
                            if (codegen::output_path_key(material->source) == original_key) {
                                throw std::invalid_argument{
                                    "Source is shared by another target and cannot be renamed "
                                    "here"};
                            }
                        }
                    }
                    if (pending_sources_.contains(destination)) {
                        throw std::invalid_argument{
                            "C++ schema source destination is pending creation: " +
                            destination.generic_string()};
                    }
                    auto const destination_path{project_.root / destination};
                    require_project_source_location(project_, destination_path);
                    if (destination != original && path_entry_exists(destination_path)) {
                        throw std::invalid_argument{
                            "C++ schema source destination already exists: " +
                            destination_path.string()};
                    }
                    if (!std::filesystem::is_directory(destination_path.parent_path())) {
                        throw std::invalid_argument{
                            "C++ schema source parent directory does not exist: " +
                            destination_path.parent_path().string()};
                    }

                    *existing = destination;
                    auto updated_renames{pending_renames_};
                    updated_renames.erase(source);
                    if (destination != original) {
                        updated_renames.emplace(destination, original);
                    }
                    validate_candidate(candidate, nullptr, &updated_renames);
                    project_ = std::move(candidate);
                    pending_renames_ = std::move(updated_renames);
                    return RenameCppSchemaSource{.target_name = edit.target_name,
                                                 .source = destination,
                                                 .destination = source};
                } else if constexpr (std::is_same_v<Edit, CreateCppSchemaSource>) {
                    if (std::ranges::any_of(target->sources, same_source)) {
                        throw std::invalid_argument{"C++ schema source is already registered: " +
                                                    source.generic_string()};
                    }
                    if (pending_sources_.contains(source)) {
                        throw std::invalid_argument{"C++ schema source is already pending: " +
                                                    source.generic_string()};
                    }
                    auto const destination{project_.root / source};
                    if (path_entry_exists(destination)) {
                        throw std::invalid_argument{
                            "C++ schema source destination already exists: " +
                            destination.string()};
                    }
                    if (!std::filesystem::is_directory(destination.parent_path())) {
                        throw std::invalid_argument{
                            "C++ schema source parent directory does not exist: " +
                            destination.parent_path().string()};
                    }

                    candidate_sources.push_back(source);
                    auto const additional{
                        PendingSourceValidation{.source = source, .contents = edit.contents}};
                    validate_candidate(candidate, &additional);
                    project_ = std::move(candidate);
                    pending_sources_.emplace(
                        source,
                        PendingSource{.target_name = edit.target_name, .contents = edit.contents});
                    return DeletePendingCppSchemaSource{.target_name = edit.target_name,
                                                        .source = source};
                } else {
                    auto const pending{pending_sources_.find(source)};
                    if (pending == pending_sources_.end() ||
                        pending->second.target_name != edit.target_name) {
                        throw std::invalid_argument{"C++ schema source is not pending: " +
                                                    source.generic_string()};
                    }
                    auto const existing{std::ranges::find_if(candidate_sources, same_source)};
                    if (existing == candidate_sources.end()) {
                        throw std::logic_error{"Pending C++ schema source is not registered: " +
                                               source.generic_string()};
                    }
                    candidate_sources.erase(existing);
                    validate_candidate(candidate);
                    auto contents{pending->second.contents};
                    project_ = std::move(candidate);
                    pending_sources_.erase(pending);
                    return CreateCppSchemaSource{.target_name = edit.target_name,
                                                 .source = source,
                                                 .contents = std::move(contents)};
                }
            },
            command);
    } catch (std::exception const& error) {
        return std::unexpected{ProjectEditError{error.what()}};
    }
}

auto EditableProjectDocument::apply(ProjectEditCommand command)
    -> std::expected<bool, ProjectEditError> {
    auto inverse{apply_internal(command)};
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

auto EditableProjectDocument::preview_source_updates() const
    -> std::expected<std::vector<ProjectSourceUpdate>, ProjectEditError> {
    struct Replacement {
        std::size_t begin{};
        std::size_t end{};
        std::string text;
    };
    try {
        std::vector<Replacement> replacements;
        for (auto const& [target_name, range] : source_lists_) {
            auto const found{project_.targets.find(target_name)};
            auto const* target{found == project_.targets.end()
                                   ? nullptr
                                   : std::get_if<CppSchemaTarget>(&found->second)};
            if (target == nullptr) {
                throw std::logic_error{"Editable project source-list target disappeared"};
            }
            auto const same_path{
                [](std::filesystem::path const& first, std::filesystem::path const& second) {
                    return first.lexically_normal() == second.lexically_normal();
                }};
            if (std::ranges::equal(range.original_sources, target->sources, same_path)) {
                continue;
            }
            auto rendered{
                source_.substr(range.begin_offset, range.end_offset - range.begin_offset)};
            auto insertion_offset{range.closing_offset - range.begin_offset};
            auto const multiline{rendered.find('\n') != std::string::npos};
            if (multiline) {
                while (insertion_offset > 1 && std::isspace(static_cast<unsigned char>(
                                                   rendered[insertion_offset - 1])) != 0) {
                    --insertion_offset;
                }
            }
            std::string insertion;
            for (auto const& source : target->sources) {
                if (std::ranges::any_of(
                        range.original_sources,
                        [&](auto const& original) { return same_path(original, source); }) ||
                    std::ranges::any_of(pending_renames_, [&](auto const& rename) {
                        return same_path(rename.first, source) &&
                               std::ranges::any_of(range.original_sources,
                                                   [&](auto const& original) {
                                                       return same_path(original, rename.second);
                                                   });
                    })) {
                    continue;
                }
                if (multiline) {
                    insertion += "\n" + std::string(range.item_indentation, ' ');
                } else if (!target->sources.empty()) {
                    insertion += ' ';
                }
                insertion += quote(source);
            }
            rendered.insert(insertion_offset, insertion);
            std::vector<SourceListItem> removed_items;
            for (auto const& item : range.original_items) {
                if (!std::ranges::any_of(target->sources, [&](auto const& current) {
                        return same_path(current, item.source);
                    })) {
                    removed_items.push_back(item);
                }
            }
            std::ranges::sort(removed_items, std::greater{}, &SourceListItem::begin_offset);
            for (auto const& item : removed_items) {
                auto const begin{item.begin_offset - range.begin_offset};
                auto const renamed{std::ranges::find_if(pending_renames_, [&](auto const& rename) {
                    return same_path(rename.second, item.source) &&
                           std::ranges::any_of(target->sources, [&](auto const& source) {
                               return same_path(source, rename.first);
                           });
                })};
                if (renamed == pending_renames_.end()) {
                    rendered.erase(begin, item.end_offset - item.begin_offset);
                } else {
                    rendered.replace(
                        begin, item.end_offset - item.begin_offset, quote(renamed->first));
                }
            }
            replacements.push_back({.begin = range.begin_offset,
                                    .end = range.end_offset,
                                    .text = std::move(rendered)});
        }
        std::vector<ProjectSourceUpdate> updates;
        if (!replacements.empty()) {
            std::ranges::sort(replacements, std::greater{}, &Replacement::begin);
            auto updated{source_};
            for (auto const& replacement : replacements) {
                updated.replace(
                    replacement.begin, replacement.end - replacement.begin, replacement.text);
            }
            updates.push_back({.path = path_, .original = source_, .updated = std::move(updated)});
        }
        for (auto const& [source, pending] : pending_sources_) {
            updates.push_back(
                {.path = project_.root / source, .original = {}, .updated = pending.contents});
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
        std::string target_name;
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
        if (read_file(path_) != source_) {
            throw std::invalid_argument{
                "LispB project changed on disk; reload it before saving source-list edits"};
        }
        if (path_entry_exists(temporary_manifest)) {
            throw std::invalid_argument{"Temporary LispB project path already exists: " +
                                        temporary_manifest.string()};
        }

        for (auto const& [source, pending] : pending_sources_) {
            auto const destination{project_.root / source};
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
            write_file(temporary, pending.contents, "temporary LispB schema source");
            staged_sources.push_back({.target_name = pending.target_name,
                                      .source = source,
                                      .destination = destination,
                                      .temporary = std::move(temporary)});
        }

        for (auto const& [destination, original] : pending_renames_) {
            auto const original_path{project_.root / original};
            auto const destination_path{project_.root / destination};
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

        auto candidate{load_project(temporary_manifest)};
        for (auto const& staged : staged_sources) {
            auto found{candidate.targets.find(staged.target_name)};
            auto* target{found == candidate.targets.end()
                             ? nullptr
                             : std::get_if<CppSchemaTarget>(&found->second)};
            if (target == nullptr) {
                throw std::logic_error{
                    "Pending source target disappeared from candidate project: " +
                    staged.target_name};
            }
            auto const registered{
                std::ranges::find_if(target->sources, [&](std::filesystem::path const& source) {
                    return source.lexically_normal() == staged.source;
                })};
            if (registered == target->sources.end()) {
                throw std::logic_error{"Pending source disappeared from candidate project: " +
                                       staged.source.generic_string()};
            }
            auto const temporary_source{staged.temporary.lexically_relative(candidate.root)};
            if (temporary_source.empty() || temporary_source.has_root_path()) {
                throw std::logic_error{"Temporary schema source is outside the project root"};
            }
            *registered = temporary_source;
        }
        for (auto const& [destination, original] : pending_renames_) {
            for (auto& [name, candidate_target] : candidate.targets) {
                static_cast<void>(name);
                if (auto* target{std::get_if<CppSchemaTarget>(&candidate_target)}) {
                    for (auto& source : target->sources) {
                        if (source.lexically_normal() == destination) {
                            source = original;
                        }
                    }
                }
            }
        }
        for (auto const& [target_name, range] : source_lists_) {
            auto const* target{std::get_if<CppSchemaTarget>(&project_.targets.at(target_name))};
            if (target != nullptr && target->sources != range.original_sources) {
                validate_cpp_schema_target(candidate, target_name);
            }
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

auto resolve(RootedPath const& path,
             std::filesystem::path const& project_root,
             std::filesystem::path const& build_root) -> std::filesystem::path {
    return ((path.base == PathBase::project ? project_root : build_root) / path.path)
        .lexically_normal();
}

auto expand_target(Project const& project, std::string const& name) -> std::vector<std::string> {
    std::set<std::string> active;
    std::set<std::string> emitted;
    std::vector<std::string> result;
    expand(project, name, active, emitted, result);
    return result;
}

} // namespace lispb
