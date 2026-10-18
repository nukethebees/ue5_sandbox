#include <lispb/schema/editable_document.h>

#include "editable_source_rendering.h"

#include <codegen/ast/cpp_types.h>
#include <codegen/schema/declaration_handle.h>
#include <codegen/schema/declaration_schema.h>
#include <codegen/schema/declarations.h>
#include <codegen/schema/enum_schemas.h>
#include <codegen/schema/enumerator_schemas.h>
#include <codegen/schema/facade_schemas.h>
#include <codegen/schema/fixed_point_schemas.h>
#include <codegen/schema/fixed_soa_schemas.h>
#include <codegen/schema/function_schemas.h>
#include <codegen/schema/homogeneous_layout_schemas.h>
#include <codegen/schema/indices.h>
#include <codegen/schema/integer_scalar_schemas.h>
#include <codegen/schema/integer_varint_schemas.h>
#include <codegen/schema/linear_quantized_schemas.h>
#include <codegen/schema/manifest.h>
#include <codegen/schema/mini_float_schemas.h>
#include <codegen/schema/module_handle.h>
#include <codegen/schema/module_kind.h>
#include <codegen/schema/module_settings_store.h>
#include <codegen/schema/normal_module_schemas.h>
#include <codegen/schema/optional_presence_bit_schemas.h>
#include <codegen/schema/optional_sentinel_schemas.h>
#include <codegen/schema/packed_value_schemas.h>
#include <codegen/schema/record_schemas.h>
#include <codegen/schema/schema_version.h>
#include <codegen/schema/settings_module_schemas.h>
#include <codegen/schema/soa_member_schemas.h>
#include <codegen/schema/soa_schemas.h>
#include <codegen/schema/static_table_schemas.h>
#include <codegen/schema/tagged_union_schemas.h>
#include <codegen/schema/type_reference_kind.h>
#include <codegen/schema/type_reference_role.h>
#include <codegen/schema/type_refs.h>
#include <codegen/schema/umbrella_module_schemas.h>
#include <codegen/schema/union_schemas.h>
#include <codegen/schema/vector_soa_schemas.h>
#include <codegen/sexpr/indices.h>
#include <codegen/sexpr/reader.h>
#include <codegen/sexpr/source_file_kind.h>
#include <codegen/sexpr/source_location.h>
#include <codegen/sexpr/source_span.h>
#include <codegen/sexpr/sources.h>
#include <codegen/sexpr/syntax.h>
#include <codegen/source_loader.h>
#include <lispb/compiler_storage.h>
#include <lispb/indices.h>
#include <lispb/schema/editable_document_state.h>
#include <lispb/schema/editor_declarations.h>
#include <lispb/schema/indices.h>
#include <lispb/schema/save_replacement_hook.h>
#include <lispb/schema/schema_edit_command.h>
#include <lispb/schema/schema_edit_error.h>
#include <lispb/schema/schema_edit_history_entry.h>
#include <lispb/schema/schema_edit_kind.h>
#include <lispb/schema/schema_source_update.h>
#include <lispb/schema/soa_columns.h>
#include <lispb/schema/soa_types.h>
#include <lispb/schema/source_range.h>
#include <lispb/schema/type_definitions.h>
#include <lispb/schema/type_graph.h>
#include <lispb/schema/type_identity.h>
#include <lispb/schema/type_origin.h>
#include <lispb/strings.h>

#include <algorithm>
#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <exception>
#include <expected>
#include <filesystem>
#include <fstream>
#include <functional>
#include <initializer_list>
#include <ios>
#include <iterator>
#include <map>
#include <memory>
#include <optional>
#include <ranges>
#include <set>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <type_traits>
#include <utility>
#include <vector>

#if defined(_WIN32)
#include <Windows.h>
#endif

namespace lispb::schema {
namespace {

using codegen::sexpr::FormId;
using codegen::sexpr::SourceFileId;
using detail::render_editable_module;

auto form_range(FormId const form, codegen::DeclarationHandle const original = {}) -> SourceRange {
    return {.form = form, .original = original};
}

auto module_generated_type_names(codegen::Declarations const& declarations,
                                 codegen::ModuleHandle const module) -> std::set<std::string> {
    std::set<std::string> names;
    auto const normal{module};
    for (auto const declaration : declarations.items(
             declarations.normal_modules.declarations[declarations.normal_modules.index(normal)])) {
        auto const generated{codegen::generated_cpp_names(declarations, declaration, module)};
        for (auto const name : generated) {
            names.emplace(declarations.strings().view(name));
        }
    }
    return names;
}

void replace_file(std::filesystem::path const& source, std::filesystem::path const& destination) {
#if defined(_WIN32)
    if (!MoveFileExW(source.c_str(),
                     destination.c_str(),
                     MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        throw std::filesystem::filesystem_error{
            "Cannot replace LispB source file",
            source,
            destination,
            std::error_code{static_cast<int>(GetLastError()), std::system_category()}};
    }
#else
    std::filesystem::rename(source, destination);
#endif
}

} // namespace

namespace detail {

namespace {
SaveReplacementHook replacement_hook{};
}

auto set_save_replacement_hook_for_testing(SaveReplacementHook const hook) noexcept
    -> SaveReplacementHook {
    auto const previous{replacement_hook};
    replacement_hook = hook;
    return previous;
}

} // namespace detail

namespace {

void publish_file(std::filesystem::path const& source, std::filesystem::path const& destination) {
    if (detail::replacement_hook != nullptr) {
        detail::replacement_hook(source, destination, replace_file);
    } else {
        replace_file(source, destination);
    }
}

auto create_recovery_directory(std::filesystem::path const& destination) -> std::filesystem::path {
    for (std::uint64_t suffix{1};; ++suffix) {
        auto directory{destination};
        directory += ".layout-planner-recovery-" + std::to_string(suffix);
        std::error_code error;
        if (std::filesystem::create_directory(directory, error)) {
            return directory;
        }
        if (error && error != std::errc::file_exists) {
            throw std::filesystem::filesystem_error{
                "Cannot create LispB recovery directory", directory, error};
        }
    }
}

void write_recovery_file(std::filesystem::path const& path, std::string const& bytes) {
    std::ofstream output{path, std::ios::binary | std::ios::trunc};
    output.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    output.close();
    if (!output) {
        throw std::runtime_error{"Cannot write LispB recovery file: " + path.string()};
    }
}

auto text(codegen::Declarations const& declarations, IdentifierId const value) -> std::string {
    return std::string{declarations.strings().view(value)};
}

auto declaration_ids(codegen::Manifest const& manifest, std::size_t const module_index)
    -> std::span<codegen::DeclarationHandle const> {
    auto const module{((manifest.modules[module_index]).kind() == lispb::codegen::ModuleKind::normal
                           ? std::optional{manifest.modules[module_index]}
                           : std::nullopt)};
    return module ? manifest.declarations.items(
                        manifest.declarations.normal_modules
                            .declarations[manifest.declarations.normal_modules.index(*module)])
                  : std::span<codegen::DeclarationHandle const>{};
}

auto payload_at(codegen::Manifest const& manifest,
                EditorDeclarations const& declarations,
                DeclarationId const info) -> codegen::DeclarationHandle {
    return declaration_ids(manifest,
                           declarations.module_index[declarations.index(
                               info)])[declarations.declaration_index[declarations.index(info)]];
}

void set_declarations(codegen::Manifest& manifest,
                      std::size_t const module_index,
                      std::span<codegen::DeclarationHandle const> const declarations) {
    auto const module{manifest.declarations.normal_modules.clone(manifest.modules[module_index])};
    manifest.declarations.normal_modules
        .declarations[manifest.declarations.normal_modules.index(module)] =
        manifest.declarations.range(declarations);
    manifest.modules[module_index] = module;
}

void set_payload(codegen::Manifest& manifest,
                 EditorDeclarations const& declarations,
                 DeclarationId const info,
                 codegen::DeclarationHandle const payload) {
    auto const current{
        declaration_ids(manifest, declarations.module_index[declarations.index(info)])};
    std::vector<codegen::DeclarationHandle> payloads{current.begin(), current.end()};
    payloads[declarations.declaration_index[declarations.index(info)]] = payload;
    set_declarations(manifest, declarations.module_index[declarations.index(info)], payloads);
}

auto declaration_identity(codegen::Manifest const& manifest,
                          std::size_t const module_index,
                          codegen::DeclarationHandle const declaration) -> TypeIdentity {
    auto const settings{manifest.declarations.module_settings(manifest.modules[module_index])};
    return {.origin = TypeOrigin::declaration,
            .module_name = manifest.declarations.module_settings_store
                               .name[manifest.declarations.module_settings_store.index(settings)],
            .namespace_name =
                manifest.declarations.module_settings_store
                    .namespace_name[manifest.declarations.module_settings_store.index(settings)]
                    .value_or(IdentifierId{}),
            .name = codegen::declaration_name(manifest.declarations, declaration)};
}

template <typename Function>
void for_each_declaration(codegen::Manifest const& manifest, Function&& function) {
    auto const module_count{manifest.modules.size()};
    for (std::size_t module_index{}; module_index < module_count; ++module_index) {
        auto const declarations{declaration_ids(manifest, module_index)};
        auto const settings{manifest.declarations.module_settings(manifest.modules[module_index])};
        auto const count{declarations.size()};
        for (std::size_t index{}; index < count; ++index) {
            function(module_index,
                     index,
                     settings,
                     codegen::declaration_name(manifest.declarations, declarations[index]));
        }
    }
}

void refresh_declaration_locations(codegen::Manifest const& manifest,
                                   TypeGraph const& types,
                                   EditorDeclarations& declarations) {
    std::set<DeclarationId> located;
    for_each_declaration(
        manifest,
        [&](std::size_t const module_index,
            std::size_t const declaration_index,
            codegen::ModuleSettingsId const,
            IdentifierId const name) {
            auto const declaration{
                std::ranges::find_if(declarations.ordered, [&](DeclarationId const candidate) {
                    return declarations.module_index[declarations.index(candidate)] ==
                               module_index &&
                           declarations.identity[declarations.index(candidate)].name == name;
                })};
            if (declaration == declarations.ordered.end() || !located.insert(*declaration).second) {
                throw std::logic_error{"Cannot reconcile declaration locations after module move"};
            }
            auto const type{types.find_declared(manifest.modules[module_index], name)};
            declarations.identity[declarations.index(*declaration)] =
                type ? types.identity(*type)
                     : declaration_identity(
                           manifest,
                           module_index,
                           declaration_ids(manifest, module_index)[declaration_index]);
            declarations.declaration_index[declarations.index(*declaration)] = declaration_index;
        });
    if (located.size() != declarations.ordered.size()) {
        throw std::logic_error{"Cannot reconcile every declaration after module move"};
    }
}

auto use_identity(TypeGraph const& types, TypeUseId const use) -> std::optional<TypeIdentity> {
    if (types.declaration(use).value == 0) {
        return std::nullopt;
    }
    auto const settings{types.declarations().module_settings(types.module(use))};
    return TypeIdentity{
        .origin = TypeOrigin::declaration,
        .module_name = types.declarations()
                           .module_settings_store
                           .name[types.declarations().module_settings_store.index(settings)],
        .namespace_name =
            types.declarations()
                .module_settings_store
                .namespace_name[types.declarations().module_settings_store.index(settings)]
                .value_or(IdentifierId{}),
        .name = codegen::declaration_name(types.declarations(), types.declaration(use))};
}

enum class LocalSoaReferencePolicy : std::uint8_t { ignore, rename, reject };

void validate_relocated_references(codegen::Manifest const& manifest,
                                   TypeGraph const& previous,
                                   TypeGraph const& candidate,
                                   codegen::DeclarationHandle const old_declaration,
                                   codegen::DeclarationHandle const new_declaration,
                                   TypeIdentity const& old_owner,
                                   TypeIdentity const& new_owner) {
    auto const old_types{previous.types_for_declaration(old_declaration)};
    auto const new_types{candidate.types_for_declaration(new_declaration)};
    if (old_types.size() != new_types.size()) {
        throw std::invalid_argument{"Rename/move changed the declaration's owned types"};
    }
    auto expected_binding = [&](TypeId const old_type) -> std::optional<TypeId> {
        auto const owned{std::ranges::find(old_types, old_type)};
        return owned != old_types.end()
                 ? std::optional{new_types[static_cast<std::size_t>(owned - old_types.begin())]}
                 : candidate.find(previous.identity(old_type));
    };
    for (auto const use : previous.type_uses()) {
        if (previous.origin(
                previous.occurrences().type[previous.occurrences().index(previous.target(use))]) !=
            TypeOrigin::declaration) {
            continue;
        }
        auto owner{use_identity(previous, use)};
        if (owner == old_owner) {
            owner = new_owner;
        }
        auto const expected{expected_binding(
            previous.occurrences().type[previous.occurrences().index(previous.target(use))])};
        auto const preserved{std::ranges::any_of(candidate.type_uses(), [&](auto const updated) {
            return use_identity(candidate, updated) == owner &&
                   candidate.role(updated) == previous.role(use) &&
                   (owner ||
                    candidate.declarations()
                            .module_settings_store
                            .name[candidate.declarations().module_settings_store.index(
                                candidate.declarations().module_settings(
                                    candidate.module(updated)))] ==
                        previous.declarations()
                            .module_settings_store
                            .name[previous.declarations().module_settings_store.index(
                                previous.declarations().module_settings(previous.module(use)))]) &&
                   candidate.occurrences()
                           .type[candidate.occurrences().index(candidate.target(updated))] ==
                       expected;
        })};
        if (!preserved) {
            throw std::invalid_argument{
                "Cannot preserve semantic reference to '" +
                std::string{previous.strings().view(previous.cpp_spelling(
                    previous.occurrences()
                        .type[previous.occurrences().index(previous.target(use))]))} +
                "' from '" +
                text(manifest.declarations,
                     previous.declarations()
                         .module_settings_store
                         .name[previous.declarations().module_settings_store.index(
                             previous.declarations().module_settings(previous.module(use)))]) +
                " / " +
                codegen::type_reference_role_text(manifest.declarations.strings(),
                                                  previous.role(use)) +
                "' after rename/move"};
        }
    }
    for (auto const& [name, registration] : manifest.types.entries) {
        static_cast<void>(registration);
        auto const binding{previous.find_registered(name)};
        if (binding && previous.origin(*binding) == TypeOrigin::declaration &&
            candidate.find_registered(name) != expected_binding(*binding)) {
            throw std::invalid_argument{"Cannot preserve registered semantic reference '@" +
                                        text(manifest.declarations, name) + "' after rename/move"};
        }
    }
}

void repair_semantic_references(codegen::Manifest& manifest,
                                TypeGraph const& types,
                                EditorDeclarations const& declarations,
                                TypeId const target,
                                IdentifierId const new_spelling,
                                std::size_t const target_module_index,
                                IdentifierId const old_name,
                                LocalSoaReferencePolicy const local_soa_policy,
                                IdentifierId const new_local_name,
                                std::size_t const destination_module_index) {
    auto& store{manifest.declarations};
    auto& strings{store.strings()};
    auto const target_node{target};
    auto const requires_local_spelling{std::ranges::any_of(types.types(), [&](auto const node) {
        return types.origin(node) == TypeOrigin::declaration &&
               types.identity(node) != types.identity(target_node) &&
               strings.view(types.cpp_spelling(node)) == strings.view(new_spelling);
    })};
    for (auto const user_info : declarations.ordered) {
        auto const effective_module_index{
            declarations.module_index[declarations.index(user_info)] == target_module_index &&
                    declarations.identity[declarations.index(user_info)].name ==
                        codegen::declaration_name(store, types.source_declaration(target_node))
                ? destination_module_index
                : declarations.module_index[declarations.index(user_info)]};
        auto replacement{codegen::rewrite_type_references(
            store,
            payload_at(manifest, declarations, user_info),
            [&](codegen::TypeReferenceRole const& role,
                codegen::TypeRefId const reference_id,
                codegen::TypeReferenceKind) {
                auto const uses{types.type_uses()};
                auto const found{std::ranges::find_if(uses, [&](auto const use) {
                    return use_identity(types, use) ==
                               declarations.identity[declarations.index(user_info)] &&
                           types.role(use) == role;
                })};
                if (found == uses.end() ||
                    types.occurrences().type[types.occurrences().index(types.target(*found))] !=
                        target) {
                    return reference_id;
                }
                auto const reference{store.type_refs.clone(reference_id)};
                if (strings.view(store.type_refs.name[store.type_refs.index(reference)])
                        .starts_with('@')) {
                    if (strings.view(new_spelling) ==
                        strings.view(types.cpp_spelling(target_node))) {
                        return reference_id;
                    }
                    throw std::invalid_argument{
                        "Cannot repair registered semantic reference '" +
                        text(store, store.type_refs.name[store.type_refs.index(reference)]) +
                        "' to '" + text(store, old_name) + "'"};
                }
                store.type_refs.name[store.type_refs.index(reference)] =
                    effective_module_index == destination_module_index &&
                            (!strings.view(store.type_refs.name[store.type_refs.index(reference)])
                                  .contains("::") ||
                             requires_local_spelling)
                        ? new_local_name
                        : new_spelling;
                return reference;
            })};
        auto soa{((replacement).kind() == lispb::codegen::DeclarationKind::soa
                      ? std::optional{replacement}
                      : std::nullopt)};
        if (soa &&
            ((types.definition(target_node)).kind() == lispb::schema::TypeDefinitionKind::soa)) {
            auto const user_type{types.find(declarations.identity[declarations.index(user_info)])};
            auto const resolved{types.definition(*user_type)};
            auto const current{store.items(store.soas.members[store.soas.index(*soa)])};
            std::vector<codegen::SoaMemberId> members{current.begin(), current.end()};
            bool changed{};
            auto const count{members.size()};
            for (std::size_t index{}; index < count; ++index) {
                auto const member{store.soa_members.clone(members[index])};
                auto const nested{
                    types.definitions()
                            .soa_columns.nested_type[types.definitions().soa_columns.index(
                                types.definitions().soas.columns_of(resolved)[index])] == target &&
                    store.soa_members.nested_schema[store.soa_members.index(member)].has_value()};
                auto const fixed{declarations.module_index[declarations.index(user_info)] ==
                                     target_module_index &&
                                 store.soa_members.fixed_schema[store.soa_members.index(member)] ==
                                     old_name};
                if ((nested || fixed) && local_soa_policy == LocalSoaReferencePolicy::reject) {
                    throw std::invalid_argument{
                        "Cannot move SoA '" + text(store, old_name) +
                        "' across modules while module-local nested/fixed schema references exist"};
                }
                if (local_soa_policy == LocalSoaReferencePolicy::rename && (nested || fixed)) {
                    if (nested) {
                        store.soa_members.nested_schema[store.soa_members.index(member)] =
                            new_local_name;
                    }
                    if (fixed) {
                        store.soa_members.fixed_schema[store.soa_members.index(member)] =
                            new_local_name;
                    }
                    members[index] = member;
                    changed = true;
                }
            }
            if (changed) {
                auto const updated{store.soas.clone(*soa)};
                store.soas.members[store.soas.index(updated)] = store.range(members);
                replacement = updated;
            }
        }
        if (replacement != payload_at(manifest, declarations, user_info)) {
            set_payload(manifest, declarations, user_info, replacement);
        }
    }
    for (auto const use : types.type_uses()) {
        if (types.declaration(use).value == 0 &&
            types.occurrences().type[types.occurrences().index(types.target(use))] == target &&
            (strings.view(new_spelling) != strings.view(types.cpp_spelling(target_node)) ||
             manifest.types.cpp_types.spelling(
                 types.occurrences().cpp_type[types.occurrences().index(types.target(use))]) !=
                 strings.view(new_spelling))) {
            throw std::invalid_argument{
                "Cannot rename or move a type referenced by module configuration: " +
                text(store,
                     store.module_settings_store.name[store.module_settings_store.index(
                         store.module_settings(types.module(use)))]) +
                " / " + codegen::type_reference_role_text(strings, types.role(use))};
        }
    }
}

} // namespace

EditableSchemaDocument::~EditableSchemaDocument() = default;
EditableSchemaDocument::EditableSchemaDocument(EditableSchemaDocument&&) noexcept = default;
auto EditableSchemaDocument::operator=(EditableSchemaDocument&& other) noexcept
    -> EditableSchemaDocument& {
    if (this != &other) {
        // Transfer the stores and their borrowers together without rebinding live references.
        std::destroy_at(this);
        std::construct_at(this, std::move(other));
    }
    return *this;
}

EditableSchemaDocument::EditableSchemaDocument(std::unique_ptr<CompilerStorage> storage,
                                               EditableDocumentState state)
    : storage_{std::move(storage)}
    , state_{std::move(state)} {}

auto EditableSchemaDocument::from_manifest(std::unique_ptr<CompilerStorage> storage,
                                           codegen::Manifest manifest) -> EditableSchemaDocument {
    EditableDocumentState state{std::move(manifest)};
    state.types_ = resolve_type_graph(state.manifest_);
    initialize_declarations(state, {});
    return EditableSchemaDocument{std::move(storage), std::move(state)};
}

auto EditableSchemaDocument::manifest() const -> codegen::Manifest const& {
    return state_.manifest_;
}

auto EditableSchemaDocument::types() const -> TypeGraph const& {
    return state_.types_;
}

auto EditableSchemaDocument::source_files() const -> std::span<SourceFileId const> {
    return state_.source_files_;
}

auto EditableSchemaDocument::module_source_file(std::size_t const module_index) const
    -> std::optional<SourceFileId> {
    if (auto const pending{state_.pending_module_sources_.find(module_index)};
        pending != state_.pending_module_sources_.end()) {
        return pending->second;
    }
    if (module_index < state_.module_source_ranges_.size() &&
        state_.module_source_ranges_[module_index].has_value()) {
        return source_span(*state_.module_source_ranges_[module_index]).file;
    }
    return std::nullopt;
}

auto EditableSchemaDocument::registered_type_source(std::string_view const name) const
    -> std::optional<SourceRange> {
    auto const found{state_.registered_type_sources_.find(storage_->strings.intern(name))};
    return found == state_.registered_type_sources_.end() ? std::nullopt
                                                          : std::optional{found->second};
}

auto EditableSchemaDocument::declarations() const -> EditorDeclarations const& {
    return state_.declarations_;
}

auto EditableSchemaDocument::find_declaration(TypeIdentity const& identity) const
    -> std::optional<DeclarationId> {
    auto const& declarations{state_.declarations_};
    auto const found{std::ranges::find_if(declarations.ordered, [&](DeclarationId id) {
        return declarations.identity[declarations.index(id)] == identity;
    })};
    return found == declarations.ordered.end() ? std::nullopt : std::optional{*found};
}

auto EditableSchemaDocument::find_declaration(codegen::DeclarationHandle const payload) const
    -> std::optional<DeclarationId> {
    for (auto const& info : state_.declarations_.ordered) {
        if (payload_at(state_.manifest_, state_.declarations_, info) == payload) {
            return info;
        }
    }
    return std::nullopt;
}

auto EditableSchemaDocument::source_span(SourceRange const& range) const
    -> codegen::sexpr::SourceSpan {
    return storage_->syntax.span(range.form);
}

auto EditableSchemaDocument::source_location(SourceRange const& range) const
    -> codegen::sexpr::SourceLocation {
    return storage_->sources.location(source_span(range));
}

auto EditableSchemaDocument::declaration_handle(
    DeclarationId const declaration_id,
    std::optional<codegen::DeclarationKind> const expected_kind) const
    -> std::optional<codegen::DeclarationHandle> {
    auto const info{state_.declarations_.find(declaration_id)};
    if (!info) {
        return std::nullopt;
    }
    auto const module{
        state_.manifest_
            .modules[state_.declarations_.module_index[state_.declarations_.index(*info)]]};
    auto const handle{
        storage_->declarations.items(
            storage_->declarations.normal_modules
                .declarations[storage_->declarations.normal_modules.index(module)])
            [state_.declarations_.declaration_index[state_.declarations_.index(*info)]]};
    return !expected_kind || handle.kind() == *expected_kind ? std::optional{handle} : std::nullopt;
}

auto EditableSchemaDocument::unique_soa_generated_type_name(DeclarationId const declaration_id,
                                                            std::string const& base) const
    -> std::expected<std::string, SchemaEditError> {
    auto const info{state_.declarations_.find(declaration_id)};
    if (!info.has_value() ||
        !(*this).declaration_handle(declaration_id, lispb::codegen::DeclarationKind::soa)) {
        return std::unexpected{SchemaEditError{"Unknown SoA declaration"}};
    }
    auto const occupied{module_generated_type_names(
        storage_->declarations,
        state_.manifest_
            .modules[state_.declarations_.module_index[state_.declarations_.index(*info)]])};
    auto candidate{base};
    for (auto suffix{std::size_t{2}}; occupied.contains(candidate); ++suffix) {
        candidate = base + std::to_string(suffix);
    }
    return candidate;
}

auto EditableSchemaDocument::unique_soa_storage_owner_name(DeclarationId const declaration_id,
                                                           std::string const& base) const
    -> std::expected<std::string, SchemaEditError> {
    auto const info{state_.declarations_.find(declaration_id)};
    if (!info.has_value() ||
        !(*this).declaration_handle(declaration_id, lispb::codegen::DeclarationKind::soa)) {
        return std::unexpected{SchemaEditError{"Unknown SoA declaration"}};
    }
    auto const occupied{module_generated_type_names(
        storage_->declarations,
        state_.manifest_
            .modules[state_.declarations_.module_index[state_.declarations_.index(*info)]])};
    auto candidate{base};
    for (auto suffix{std::size_t{2}};
         occupied.contains(candidate) || occupied.contains(candidate + "Storage");
         ++suffix) {
        candidate = base + std::to_string(suffix);
    }
    return candidate;
}

auto EditableSchemaDocument::prepare_soa_duplicate(DeclarationId const declaration_id) const
    -> std::expected<codegen::DeclarationHandle, SchemaEditError> {
    auto const info{state_.declarations_.find(declaration_id)};
    auto const source{
        (*this).declaration_handle(declaration_id, lispb::codegen::DeclarationKind::soa)};
    if (!info.has_value() || !source) {
        return std::unexpected{SchemaEditError{"Unknown SoA declaration"}};
    }
    auto occupied{module_generated_type_names(
        storage_->declarations,
        state_.manifest_
            .modules[state_.declarations_.module_index[state_.declarations_.index(*info)]])};

    auto const numbered_candidate = [](std::string const& base, std::size_t const suffix) {
        return suffix == 1 ? base : base + std::to_string(suffix);
    };
    auto reserve_unique = [&](std::string const& base) {
        for (auto suffix{std::size_t{1}};; ++suffix) {
            auto candidate{numbered_candidate(base, suffix)};
            if (occupied.insert(candidate).second) {
                return candidate;
            }
        }
    };
    auto reserve_unique_group = [&](std::string const& base,
                                    std::initializer_list<std::string_view> const endings) {
        for (auto suffix{std::size_t{1}};; ++suffix) {
            auto candidate{numbered_candidate(base, suffix)};
            auto const available{std::ranges::all_of(endings, [&](std::string_view const ending) {
                return !occupied.contains(candidate + std::string{ending});
            })};
            if (!available) {
                continue;
            }
            for (auto const ending : endings) {
                occupied.insert(candidate + std::string{ending});
            }
            return candidate;
        }
    };

    auto& store{storage_->declarations};
    auto& strings{storage_->strings};
    auto const original_name{
        text(store,
             (*this).storage().declarations.soas.name[(*this).storage().declarations.soas.index(
                 *source)])};
    auto const copy{store.soas.clone(*source)};
    std::string copy_name;
    auto schema_name_available = [&](std::string const& candidate) {
        if (occupied.contains(candidate) || occupied.contains(candidate + "Schema")) {
            return false;
        }
        if (!(*this)
                 .storage()
                 .declarations.soas.view_name[(*this).storage().declarations.soas.index(*source)]
                 .has_value() &&
            occupied.contains(candidate + "View")) {
            return false;
        }
        if (!(*this)
                 .storage()
                 .declarations.soas
                 .const_view_name[(*this).storage().declarations.soas.index(*source)]
                 .has_value() &&
            occupied.contains(candidate + "ConstView")) {
            return false;
        }
        if ((*this)
                .storage()
                .declarations.soas
                .single_allocation[(*this).storage().declarations.soas.index(*source)]
                .has_value() &&
            (occupied.contains(candidate + "SingleLayout") ||
             occupied.contains(candidate + "SingleView") ||
             occupied.contains(candidate + "SingleConstView"))) {
            return false;
        }
        return std::ranges::none_of(
            store.items((*this)
                            .storage()
                            .declarations.soas
                            .functions[(*this).storage().declarations.soas.index(*source)]),
            [&](codegen::FunctionSchemaId const function) {
                return strings.view(store.functions.name[store.functions.index(function)]) ==
                       candidate;
            });
    };
    auto const schema_name_base{original_name + "_copy"};
    for (auto suffix{std::size_t{1}};; ++suffix) {
        auto candidate{numbered_candidate(schema_name_base, suffix)};
        if (!schema_name_available(candidate)) {
            continue;
        }
        copy_name = candidate;
        (*this).storage().declarations.soas.name[(*this).storage().declarations.soas.index(copy)] =
            strings.intern(candidate);
        occupied.insert(copy_name);
        occupied.insert(copy_name + "Schema");
        if (!(*this)
                 .storage()
                 .declarations.soas.view_name[(*this).storage().declarations.soas.index(*source)]
                 .has_value()) {
            occupied.insert(copy_name + "View");
        }
        if (!(*this)
                 .storage()
                 .declarations.soas
                 .const_view_name[(*this).storage().declarations.soas.index(*source)]
                 .has_value()) {
            occupied.insert(copy_name + "ConstView");
        }
        if ((*this)
                .storage()
                .declarations.soas
                .single_allocation[(*this).storage().declarations.soas.index(*source)]
                .has_value()) {
            occupied.insert(copy_name + "SingleLayout");
            occupied.insert(copy_name + "SingleView");
            occupied.insert(copy_name + "SingleConstView");
        }
        break;
    }

    auto duplicate_helper_base = [&](std::string const& original) {
        if (original.starts_with(original_name)) {
            return copy_name + original.substr(original_name.size());
        }
        return original + "_copy";
    };
    if ((*this)
            .storage()
            .declarations.soas.view_name[(*this).storage().declarations.soas.index(*source)]
            .has_value()) {
        (*this)
            .storage()
            .declarations.soas.view_name[(*this).storage().declarations.soas.index(copy)] =
            strings.intern(reserve_unique(duplicate_helper_base(
                text(store,
                     *(*this).storage().declarations.soas.view_name
                          [(*this).storage().declarations.soas.index(*source)]))));
    }
    if ((*this)
            .storage()
            .declarations.soas.const_view_name[(*this).storage().declarations.soas.index(*source)]
            .has_value()) {
        (*this)
            .storage()
            .declarations.soas.const_view_name[(*this).storage().declarations.soas.index(copy)] =
            strings.intern(reserve_unique(duplicate_helper_base(
                text(store,
                     *(*this).storage().declarations.soas.const_view_name
                          [(*this).storage().declarations.soas.index(*source)]))));
    }
    if ((*this)
            .storage()
            .declarations.soas.field_mask_name[(*this).storage().declarations.soas.index(*source)]
            .has_value()) {
        auto const old_mask_name{
            *(*this)
                 .storage()
                 .declarations.soas
                 .field_mask_name[(*this).storage().declarations.soas.index(*source)]};
        (*this)
            .storage()
            .declarations.soas.field_mask_name[(*this).storage().declarations.soas.index(copy)] =
            strings.intern(reserve_unique(duplicate_helper_base(text(store, old_mask_name))));
        (*this)
            .storage()
            .declarations.soas.field_enum_name[(*this).storage().declarations.soas.index(copy)] =
            strings.intern(reserve_unique(duplicate_helper_base(
                text(store,
                     *(*this).storage().declarations.soas.field_enum_name
                          [(*this).storage().declarations.soas.index(*source)]))));
        auto const current{store.items(
            (*this).storage().declarations.soas.members[(*this).storage().declarations.soas.index(
                copy)])};
        std::vector<codegen::SoaMemberId> members{current.begin(), current.end()};
        for (auto& member_id : members) {
            auto const member{store.soa_members.clone(member_id)};
            auto const type{
                store.type_refs.clone(store.soa_members.type[store.soa_members.index(member)])};
            if (store.type_refs.name[store.type_refs.index(type)] == old_mask_name) {
                store.type_refs.name[store.type_refs.index(type)] =
                    *(*this)
                         .storage()
                         .declarations.soas
                         .field_mask_name[(*this).storage().declarations.soas.index(copy)];
                store.soa_members.type[store.soa_members.index(member)] = type;
                member_id = member;
            }
        }
        (*this).storage().declarations.soas.members[(*this).storage().declarations.soas.index(
            copy)] = store.range(members);
    }
    if ((*this)
            .storage()
            .declarations.soas.fixed[(*this).storage().declarations.soas.index(*source)]
            .has_value()) {
        auto const fixed{store.fixed_soas.clone(*store.soas.fixed[store.soas.index(*source)])};
        store.fixed_soas.storage_name[store.fixed_soas.index(fixed)] =
            strings.intern(reserve_unique(duplicate_helper_base(
                text(store, store.fixed_soas.storage_name[store.fixed_soas.index(fixed)]))));
        std::vector<IdentifierId> containers;
        for (auto const container :
             store.items(store.fixed_soas.containers[store.fixed_soas.index(fixed)])) {
            containers.push_back(strings.intern(
                reserve_unique(duplicate_helper_base(std::string{strings.view(container)}))));
        }
        store.fixed_soas.containers[store.fixed_soas.index(fixed)] = store.range(containers);
        (*this).storage().declarations.soas.fixed[(*this).storage().declarations.soas.index(copy)] =
            fixed;
    }
    if ((*this)
            .storage()
            .declarations.soas.single_allocation[(*this).storage().declarations.soas.index(*source)]
            .has_value()) {
        (*this)
            .storage()
            .declarations.soas.single_allocation[(*this).storage().declarations.soas.index(copy)] =
            strings.intern(reserve_unique_group(
                duplicate_helper_base(std::string{
                    strings.view(*(*this).storage().declarations.soas.single_allocation
                                      [(*this).storage().declarations.soas.index(*source)])}),
                {"", "Storage"}));
    }

    return copy;
}

auto EditableSchemaDocument::allocate_declaration_id() -> DeclarationId {
    return state_.declarations_.allocate();
}

auto EditableSchemaDocument::create_declaration(DeclarationId const declaration,
                                                std::size_t const module_index,
                                                codegen::DeclarationHandle schema,
                                                std::optional<std::size_t> const insertion_index)
    -> SchemaEditCommand {
    auto const command{SchemaEditCommand{SchemaEditKind::create_declaration,
                                         state_.create_declaration_edits_.declarations.size()}};
    state_.create_declaration_edits_.declarations.push_back(declaration);
    state_.create_declaration_edits_.module_indices.push_back(module_index);
    state_.create_declaration_edits_.versions.push_back(schema);
    state_.create_declaration_edits_.insertion_indices.push_back(insertion_index);
    return command;
}

auto EditableSchemaDocument::replace_declaration(DeclarationId const declaration,
                                                 codegen::DeclarationHandle schema)
    -> SchemaEditCommand {
    auto const command{SchemaEditCommand{SchemaEditKind::replace_declaration,
                                         state_.replace_declaration_edits_.declarations.size()}};
    state_.replace_declaration_edits_.declarations.push_back(declaration);
    state_.replace_declaration_edits_.versions.push_back(schema);
    return command;
}

auto EditableSchemaDocument::delete_declaration(DeclarationId const declaration)
    -> SchemaEditCommand {
    auto const command{SchemaEditCommand{SchemaEditKind::delete_declaration,
                                         state_.delete_declaration_edits_.size()}};
    state_.delete_declaration_edits_.push_back(declaration);
    return command;
}

auto EditableSchemaDocument::create_module(SourceFileId const source_file,
                                           codegen::ModuleHandle schema) -> SchemaEditCommand {
    auto const command{SchemaEditCommand{SchemaEditKind::create_module,
                                         state_.create_module_edits_.source_files.size()}};
    state_.create_module_edits_.source_files.push_back(source_file);
    state_.create_module_edits_.versions.push_back(schema);
    return command;
}

auto EditableSchemaDocument::delete_module(std::size_t const module_index) -> SchemaEditCommand {
    auto const command{
        SchemaEditCommand{SchemaEditKind::delete_module, state_.delete_module_edits_.size()}};
    state_.delete_module_edits_.push_back(module_index);
    return command;
}

auto EditableSchemaDocument::restore_module(std::size_t const module_index,
                                            codegen::ModuleHandle schema,
                                            std::optional<SourceRange> const source,
                                            std::optional<SourceFileId> const pending_source_file,
                                            std::span<DeclarationId const> const declarations)
    -> SchemaEditCommand {
    auto const command{SchemaEditCommand{SchemaEditKind::restore_module,
                                         state_.restore_module_edits_.module_indices.size()}};
    state_.restore_module_edits_.module_indices.push_back(module_index);
    state_.restore_module_edits_.versions.push_back(schema);
    state_.restore_module_edits_.sources.push_back(source);
    state_.restore_module_edits_.pending_source_files.push_back(pending_source_file);
    state_.restore_module_edits_.declaration_offsets.push_back(
        static_cast<std::uint32_t>(state_.restore_module_edits_.declarations.size()));
    state_.restore_module_edits_.declaration_counts.push_back(
        static_cast<std::uint32_t>(declarations.size()));
    state_.restore_module_edits_.declarations.insert(
        state_.restore_module_edits_.declarations.end(), declarations.begin(), declarations.end());
    return command;
}

auto EditableSchemaDocument::move_declaration(DeclarationId const declaration,
                                              std::size_t const module_index,
                                              std::optional<std::size_t> const insertion_index,
                                              bool const restore_source_ownership)
    -> SchemaEditCommand {
    auto const command{SchemaEditCommand{SchemaEditKind::move_declaration,
                                         state_.move_declaration_edits_.declarations.size()}};
    state_.move_declaration_edits_.declarations.push_back(declaration);
    state_.move_declaration_edits_.module_indices.push_back(module_index);
    state_.move_declaration_edits_.insertion_indices.push_back(insertion_index);
    state_.move_declaration_edits_.restore_source_ownership.push_back(restore_source_ownership);
    return command;
}

auto EditableSchemaDocument::set_enumerator_display_name(DeclarationId const declaration,
                                                         std::string_view const enumerator_name,
                                                         std::optional<std::string> display_name)
    -> SchemaEditCommand {
    auto const command{
        SchemaEditCommand{SchemaEditKind::set_enumerator_display_name,
                          state_.enumerator_display_name_edits_.declarations.size()}};
    auto const name{storage_->strings.intern(enumerator_name)};
    auto const display{display_name.transform(
        [&](std::string const& text) { return storage_->strings.add(text); })};
    state_.enumerator_display_name_edits_.declarations.push_back(declaration);
    state_.enumerator_display_name_edits_.names.push_back(name);
    state_.enumerator_display_name_edits_.display_names.push_back(display);
    return command;
}

auto EditableSchemaDocument::set_enumerator_name(DeclarationId const declaration,
                                                 std::string_view const current_name,
                                                 std::string_view const new_name)
    -> SchemaEditCommand {
    auto const command{SchemaEditCommand{SchemaEditKind::set_enumerator_name,
                                         state_.enumerator_name_edits_.declarations.size()}};
    auto const current{storage_->strings.intern(current_name)};
    auto const replacement{storage_->strings.intern(new_name)};
    state_.enumerator_name_edits_.declarations.push_back(declaration);
    state_.enumerator_name_edits_.current_names.push_back(current);
    state_.enumerator_name_edits_.new_names.push_back(replacement);
    return command;
}

auto EditableSchemaDocument::rename_declaration(DeclarationId const declaration,
                                                std::string_view const new_name)
    -> SchemaEditCommand {
    auto const command{SchemaEditCommand{SchemaEditKind::rename_declaration,
                                         state_.rename_declaration_edits_.declarations.size()}};
    auto const name{storage_->strings.intern(new_name)};
    state_.rename_declaration_edits_.declarations.push_back(declaration);
    state_.rename_declaration_edits_.names.push_back(name);
    return command;
}

auto EditableSchemaDocument::apply(SchemaEditCommand command)
    -> std::expected<bool, SchemaEditError> {
    auto inverse{execute(command)};
    if (!inverse.has_value()) {
        return std::unexpected{std::move(inverse.error())};
    }
    if (!inverse->has_value()) {
        return false;
    }

    if (state_.saved_history_position_.has_value() &&
        *state_.saved_history_position_ > state_.history_position_) {
        state_.saved_history_position_.reset();
    }
    state_.history_.erase(state_.history_.begin() +
                              static_cast<std::ptrdiff_t>(state_.history_position_),
                          state_.history_.end());
    state_.history_.push_back(
        SchemaEditHistoryEntry{.forward = std::move(command), .inverse = std::move(**inverse)});
    ++state_.history_position_;
    ++state_.revision_;
    return true;
}

auto EditableSchemaDocument::command_declaration(SchemaEditCommand const command) const
    -> std::optional<DeclarationId> {
    auto const index{command.index()};
    switch (command.kind()) {
        case SchemaEditKind::create_declaration:
            return state_.create_declaration_edits_.declarations[index];
        case SchemaEditKind::replace_declaration:
            return state_.replace_declaration_edits_.declarations[index];
        case SchemaEditKind::delete_declaration:
            return state_.delete_declaration_edits_[index];
        case SchemaEditKind::move_declaration:
            return state_.move_declaration_edits_.declarations[index];
        case SchemaEditKind::set_enumerator_display_name:
            return state_.enumerator_display_name_edits_.declarations[index];
        case SchemaEditKind::set_enumerator_name:
            return state_.enumerator_name_edits_.declarations[index];
        case SchemaEditKind::rename_declaration:
            return state_.rename_declaration_edits_.declarations[index];
        case SchemaEditKind::create_module:
        case SchemaEditKind::delete_module:
        case SchemaEditKind::restore_module:
            return std::nullopt;
    }
    std::unreachable();
}

auto EditableSchemaDocument::undo() -> std::expected<bool, SchemaEditError> {
    if (!can_undo()) {
        return false;
    }
    auto const& entry{state_.history_[state_.history_position_ - 1]};
    auto result{execute(entry.inverse, false)};
    if (!result.has_value()) {
        return std::unexpected{std::move(result.error())};
    }
    if (!result->has_value()) {
        return std::unexpected{SchemaEditError{"Undo command did not change the schema draft"}};
    }
    --state_.history_position_;
    ++state_.revision_;
    return true;
}

auto EditableSchemaDocument::redo() -> std::expected<bool, SchemaEditError> {
    if (!can_redo()) {
        return false;
    }
    auto const& entry{state_.history_[state_.history_position_]};
    auto result{execute(entry.forward, false)};
    if (!result.has_value()) {
        return std::unexpected{std::move(result.error())};
    }
    if (!result->has_value()) {
        return std::unexpected{SchemaEditError{"Redo command did not change the schema draft"}};
    }
    ++state_.history_position_;
    ++state_.revision_;
    return true;
}

auto EditableSchemaDocument::can_undo() const -> bool {
    return state_.history_position_ != 0;
}

auto EditableSchemaDocument::can_redo() const -> bool {
    return state_.history_position_ != state_.history_.size();
}

auto EditableSchemaDocument::dirty() const -> bool {
    return !state_.saved_history_position_.has_value() ||
           state_.history_position_ != *state_.saved_history_position_;
}

auto EditableSchemaDocument::revision() const -> std::uint64_t {
    return state_.revision_;
}

void EditableSchemaDocument::mark_saved() {
    state_.saved_history_position_ = state_.history_position_;
}

auto EditableSchemaDocument::preview_source_updates() const
    -> std::expected<std::vector<SchemaSourceUpdate>, SchemaEditError> {
    if (!dirty()) {
        return std::vector<SchemaSourceUpdate>{};
    }
    if (state_.source_files_.empty() ||
        state_.module_source_ranges_.size() != state_.manifest_.modules.size()) {
        return std::unexpected{SchemaEditError{"Schema draft has no source ownership information"}};
    }

    std::set<DeclarationId> touched;
    for (std::size_t index{}; index < state_.history_position_; ++index) {
        auto const command{state_.history_[index].forward};
        auto const edited_declaration{command_declaration(command)};
        if (!edited_declaration.has_value()) {
            continue;
        }
        touched.insert(*edited_declaration);
        if (command.kind() == SchemaEditKind::rename_declaration ||
            command.kind() == SchemaEditKind::move_declaration) {
            auto const renamed{state_.declarations_.find(*edited_declaration)};
            if (renamed.has_value()) {
                auto const owned_types{state_.types_.types_for_declaration(
                    payload_at(state_.manifest_, state_.declarations_, *renamed))};
                for (auto const& use : state_.types_.type_uses()) {
                    if (state_.types_.declaration(use).value != 0 &&
                        std::ranges::find(
                            owned_types,
                            state_.types_.occurrences().type[state_.types_.occurrences().index(
                                state_.types_.target(use))]) != owned_types.end()) {
                        if (auto const owner{find_declaration(*use_identity(state_.types_, use))}) {
                            touched.insert(*owner);
                        }
                    }
                }
                auto const type{state_.types_.find(
                    state_.declarations_.identity[state_.declarations_.index(*renamed)])};
                if (type.has_value()) {
                    for (auto const user : state_.types_.users_of(*type)) {
                        auto const user_declaration{find_declaration(state_.types_.identity(user))};
                        if (user_declaration.has_value()) {
                            touched.insert(*user_declaration);
                        }
                    }
                    if (((state_.types_.definition(*type)).kind() ==
                         lispb::schema::TypeDefinitionKind::soa) &&
                        (*this)
                            .declaration_handle(*edited_declaration,
                                                lispb::codegen::DeclarationKind::soa)
                            .has_value()) {
                        for (auto const& candidate : state_.declarations_.ordered) {
                            if (state_.declarations_
                                    .module_index[state_.declarations_.index(candidate)] !=
                                state_.declarations_
                                    .module_index[state_.declarations_.index(*renamed)]) {
                                continue;
                            }
                            auto const candidate_schema{(*this).declaration_handle(
                                candidate, lispb::codegen::DeclarationKind::soa)};
                            if (!candidate_schema) {
                                continue;
                            }
                            auto const references_renamed{std::ranges::any_of(
                                storage_->declarations.items(
                                    (*this)
                                        .storage()
                                        .declarations.soas
                                        .members[(*this).storage().declarations.soas.index(
                                            *candidate_schema)]),
                                [&](auto const member_id) {
                                    auto const member{member_id};
                                    return storage_->declarations.soa_members
                                                   .nested_schema[storage_->declarations.soa_members
                                                                      .index(member)] ==
                                               state_.declarations_
                                                   .identity[state_.declarations_.index(*renamed)]
                                                   .name ||
                                           storage_->declarations.soa_members
                                                   .fixed_schema[storage_->declarations.soa_members
                                                                     .index(member)] ==
                                               state_.declarations_
                                                   .identity[state_.declarations_.index(*renamed)]
                                                   .name;
                                })};
                            if (references_renamed) {
                                touched.insert(candidate);
                            }
                        }
                    }
                }
            }
        }
    }

    struct Replacement {
        std::size_t begin{};
        std::size_t end{};
        std::string text;
    };
    std::map<SourceFileId, std::vector<Replacement>> replacements;
    std::map<std::size_t, std::set<DeclarationId>> insertions;
    auto covered_by_deleted_module = [&](SourceRange const& range) {
        return std::ranges::any_of(
            state_.deleted_module_source_ranges_, [&](SourceRange const& module) {
                return source_span(module).file == source_span(range).file &&
                       source_span(module).offset <= source_span(range).offset &&
                       source_span(range).end_offset <= source_span(module).end_offset;
            });
    };
    for (auto const& range : state_.deleted_module_source_ranges_) {
        replacements[source_span(range).file].push_back(
            {.begin = source_span(range).offset, .end = source_span(range).end_offset, .text = {}});
    }
    auto render_declaration = [&](DeclarationId const id) -> std::optional<std::string> {
        auto const info{state_.declarations_.find(id)};
        if (!info.has_value() ||
            state_.declarations_.module_index[state_.declarations_.index(*info)] >=
                state_.manifest_.modules.size()) {
            return std::nullopt;
        }
        auto const module{
            ((state_.manifest_
                  .modules[state_.declarations_.module_index[state_.declarations_.index(*info)]])
                         .kind() == lispb::codegen::ModuleKind::normal
                 ? std::optional{state_.manifest_
                                     .modules[state_.declarations_
                                                  .module_index[state_.declarations_.index(*info)]]}
                 : std::nullopt)};
        if (!module || state_.declarations_.declaration_index[state_.declarations_.index(*info)] >=
                           storage_->declarations.normal_modules
                               .declarations[storage_->declarations.normal_modules.index(*module)]
                               .size()) {
            return std::nullopt;
        }
        auto range{state_.declarations_.source[state_.declarations_.index(*info)]};
        if (!range.has_value()) {
            auto const tombstone{state_.source_tombstones_.find(id)};
            if (tombstone != state_.source_tombstones_.end()) {
                range = tombstone->second;
            }
        }
        return detail::render_declaration_source(
            *storage_,
            storage_->declarations.items(
                storage_->declarations.normal_modules
                    .declarations[storage_->declarations.normal_modules.index(*module)])
                [state_.declarations_.declaration_index[state_.declarations_.index(*info)]],
            range);
    };
    for (auto const id : touched) {
        auto const info{state_.declarations_.find(id)};
        if (!info.has_value()) {
            auto const tombstone{state_.source_tombstones_.find(id)};
            if (tombstone != state_.source_tombstones_.end() &&
                !covered_by_deleted_module(tombstone->second)) {
                auto const& source{tombstone->second};
                replacements[source_span(source).file].push_back(
                    {.begin = source_span(source).offset,
                     .end = source_span(source).end_offset,
                     .text = {}});
            }
            continue;
        }

        auto const rendered{render_declaration(id)};
        if (!rendered.has_value()) {
            return std::unexpected{SchemaEditError{"Touched declaration cannot be serialized"}};
        }
        if (state_.declarations_.source[state_.declarations_.index(*info)].has_value() &&
            !covered_by_deleted_module(
                *state_.declarations_.source[state_.declarations_.index(*info)])) {
            replacements
                [source_span(*state_.declarations_.source[state_.declarations_.index(*info)]).file]
                    .push_back(
                        {.begin =
                             source_span(
                                 *state_.declarations_.source[state_.declarations_.index(*info)])
                                 .offset,
                         .end = source_span(
                                    *state_.declarations_.source[state_.declarations_.index(*info)])
                                    .end_offset,
                         .text = *rendered});
        } else {
            auto const tombstone{state_.source_tombstones_.find(id)};
            if (tombstone != state_.source_tombstones_.end() &&
                !covered_by_deleted_module(tombstone->second)) {
                auto const& source{tombstone->second};
                replacements[source_span(source).file].push_back(
                    {.begin = source_span(source).offset,
                     .end = source_span(source).end_offset,
                     .text = {}});
            }
            insertions[state_.declarations_.module_index[state_.declarations_.index(*info)]].insert(
                id);
        }
    }
    struct InsertionBoundary {
        SourceFileId file{};
        std::size_t offset{};
        bool before_declaration{};

        auto operator<=>(InsertionBoundary const&) const = default;
    };
    std::map<InsertionBoundary, std::vector<DeclarationId>> bounded_insertions;
    for (auto const& [module_index, ids] : insertions) {

        if (state_.pending_module_sources_.contains(module_index)) {
            continue;
        }
        if (module_index >= state_.module_source_ranges_.size() ||
            !state_.module_source_ranges_[module_index].has_value()) {
            return std::unexpected{SchemaEditError{"New declaration's module has no source range"}};
        }
        for (auto const& declaration_info : state_.declarations_.ordered) {
            if (state_.declarations_.module_index[state_.declarations_.index(declaration_info)] !=
                    module_index ||
                !ids.contains(declaration_info)) {
                continue;
            }

            std::optional<DeclarationId> next{};
            for (auto const& candidate : state_.declarations_.ordered) {
                if (state_.declarations_.module_index[state_.declarations_.index(candidate)] ==
                        module_index &&
                    state_.declarations_.source[state_.declarations_.index(candidate)]
                        .has_value() &&
                    state_.declarations_.declaration_index[state_.declarations_.index(candidate)] >
                        state_.declarations_
                            .declaration_index[state_.declarations_.index(declaration_info)] &&
                    (!next.has_value() ||
                     state_.declarations_.declaration_index[state_.declarations_.index(candidate)] <
                         state_.declarations_
                             .declaration_index[state_.declarations_.index(*next)])) {
                    next = candidate;
                }
            }
            if (next.has_value()) {
                auto const source{storage_->sources.text(
                    source_span(*state_.declarations_.source[state_.declarations_.index(*next)])
                        .file)};
                auto const line_break{
                    source_span(*state_.declarations_.source[state_.declarations_.index(*next)])
                                .offset == 0
                        ? std::string::npos
                        : source.rfind(
                              '\n',
                              source_span(
                                  *state_.declarations_.source[state_.declarations_.index(*next)])
                                      .offset -
                                  1)};
                auto const line_begin{line_break == std::string::npos ? 0 : line_break + 1};
                bounded_insertions[{.file =
                                        source_span(*state_.declarations_
                                                         .source[state_.declarations_.index(*next)])
                                            .file,
                                    .offset = line_begin,
                                    .before_declaration = true}]
                    .push_back(declaration_info);
            } else {
                auto const& module_range{*state_.module_source_ranges_[module_index]};
                bounded_insertions[{.file = source_span(module_range).file,
                                    .offset = source_span(module_range).end_offset - 1,
                                    .before_declaration = false}]
                    .push_back(declaration_info);
            }
        }
    }
    for (auto& [boundary, declarations] : bounded_insertions) {
        std::ranges::sort(declarations, {}, [&](DeclarationId id) {
            return state_.declarations_.declaration_index[state_.declarations_.index(id)];
        });
        std::string insertion;
        for (auto const declaration_info : declarations) {
            auto const rendered{render_declaration(declaration_info)};
            if (!rendered.has_value()) {
                return std::unexpected{
                    SchemaEditError{"New declaration kind cannot be serialized"}};
            }
            insertion += boundary.before_declaration ? "  " + *rendered + "\n" : "\n  " + *rendered;
        }
        replacements[boundary.file].push_back(
            {.begin = boundary.offset, .end = boundary.offset, .text = std::move(insertion)});
    }
    std::map<SourceFileId, std::string> pending_module_text;
    for (auto const& [module_index, source_file] : state_.pending_module_sources_) {
        if (module_index >= state_.manifest_.modules.size() ||
            std::ranges::find(state_.source_files_, source_file) == state_.source_files_.end()) {
            return std::unexpected{SchemaEditError{"Invalid pending module source ownership"}};
        }
        auto rendered{render_editable_module(*storage_, state_.manifest_.modules[module_index])};
        if (!rendered.has_value()) {
            return std::unexpected{SchemaEditError{"New module kind cannot be serialized"}};
        }
        auto& insertion{pending_module_text[source_file]};
        auto const& source{storage_->sources.text(source_file)};
        if (insertion.empty() && !source.empty()) {
            insertion = source.ends_with('\n') ? "\n" : "\n\n";
        }
        insertion += std::move(*rendered) + "\n\n";
    }
    for (auto& [source_file, insertion] : pending_module_text) {
        insertion.pop_back();
        auto const& source{storage_->sources.text(source_file)};
        replacements[source_file].push_back(
            {.begin = source.size(), .end = source.size(), .text = std::move(insertion)});
    }

    std::vector<SchemaSourceUpdate> updates;
    for (auto& [source_file, source_replacements] : replacements) {
        if (source_replacements.empty()) {
            continue;
        }
        std::ranges::sort(source_replacements, std::greater{}, &Replacement::begin);
        auto updated{std::string{storage_->sources.text(source_file)}};
        for (auto const& replacement : source_replacements) {
            if (replacement.begin > replacement.end || replacement.end > updated.size()) {
                return std::unexpected{SchemaEditError{"Invalid source replacement range"}};
            }
            updated.replace(
                replacement.begin, replacement.end - replacement.begin, replacement.text);
        }
        auto const& original{storage_->sources.text(source_file)};
        if (original.contains("\r\n")) {
            auto const has_bare_line_feed{[&] {
                for (std::size_t index{}; index < original.size(); ++index) {
                    if (original[index] == '\n' && (index == 0 || original[index - 1] != '\r')) {
                        return true;
                    }
                }
                return false;
            }()};
            if (!has_bare_line_feed) {
                std::string converted;
                converted.reserve(updated.size());
                char previous{};
                for (auto const character : updated) {
                    if (character == '\n' && previous != '\r') {
                        converted += '\r';
                    }
                    converted += character;
                    previous = character;
                }
                updated = std::move(converted);
            }
        }
        if (updated == original) {
            continue;
        }
        updates.push_back({.path = storage_->sources.filesystem_path(source_file),
                           .original = std::string{storage_->sources.text(source_file)},
                           .updated = std::move(updated)});
    }
    return updates;
}

auto EditableSchemaDocument::save()
    -> std::expected<std::vector<std::filesystem::path>, SchemaEditError> {
    auto updates{preview_source_updates()};
    if (!updates.has_value()) {
        return std::unexpected{std::move(updates.error())};
    }
    struct RecoveryFile {
        std::filesystem::path destination;
        std::filesystem::path directory;
        std::filesystem::path staged;
        std::filesystem::path original;
        std::string_view published_content;
    };
    std::vector<RecoveryFile> recovery;
    recovery.reserve(updates->size());
    auto cleanup_safe{true};
    auto cleanup{[&]() noexcept {
        if (!cleanup_safe) {
            return;
        }
        for (auto const& file : recovery) {
            try {
                std::error_code error;
                std::filesystem::remove_all(file.directory, error);
                if (error) {
                    std::fputs("LispB: unable to remove a temporary save recovery directory.\n",
                               stderr);
                }
            } catch (...) {
                // Cleanup must not turn a committed save into a reported failure.
                std::fputs("LispB: exception removing a temporary save recovery directory.\n",
                           stderr);
            }
        }
    }};
    auto verify_snapshots{[&] {
        for (auto const& source : state_.source_files_) {
            std::error_code status_error;
            auto const exists{
                std::filesystem::exists(storage_->sources.filesystem_path(source), status_error)};
            if (status_error) {
                throw std::runtime_error{"Cannot verify LispB source '" +
                                         storage_->sources.filesystem_path(source).string() +
                                         "': " + status_error.message()};
            }
            if (!exists) {
                throw std::runtime_error{"LispB source changed or deleted: " +
                                         storage_->sources.filesystem_path(source).string()};
            }
            std::ifstream input{storage_->sources.filesystem_path(source), std::ios::binary};
            if (!input) {
                throw std::runtime_error{"Cannot verify LispB source '" +
                                         storage_->sources.filesystem_path(source).string() +
                                         "': cannot open file"};
            }
            auto const current{std::string{std::istreambuf_iterator<char>{input},
                                           std::istreambuf_iterator<char>{}}};
            if (input.bad()) {
                throw std::runtime_error{"Cannot verify LispB source '" +
                                         storage_->sources.filesystem_path(source).string() +
                                         "': cannot read file"};
            }
            if (current != storage_->sources.text(source)) {
                throw std::runtime_error{"LispB source changed externally: " +
                                         storage_->sources.filesystem_path(source).string()};
            }
        }
    }};
    try {
        verify_snapshots();
        if (updates->empty()) {
            return std::vector<std::filesystem::path>{};
        }
        for (auto const& update : *updates) {
            auto directory{create_recovery_directory(update.path)};
            recovery.push_back({.destination = update.path,
                                .directory = directory,
                                .staged = directory / "staged",
                                .original = directory / "original",
                                .published_content = update.updated});
            write_recovery_file(recovery.back().staged, update.updated);
            write_recovery_file(recovery.back().original, update.original);
        }

        std::vector<std::filesystem::path> validation_modules;
        validation_modules.reserve(state_.module_paths_.size());
        for (auto const& path : state_.module_paths_) {
            auto const staged{std::ranges::find(recovery, path, &RecoveryFile::destination)};
            validation_modules.push_back(staged == recovery.end() ? path : staged->staged);
        }
        auto prepared{read_state(*storage_, state_.types_path_, validation_modules)};
        for (auto& source : prepared.source_files_) {
            auto const staged{std::ranges::find(
                recovery, storage_->sources.filesystem_path(source), &RecoveryFile::staged)};
            if (staged != recovery.end()) {
                storage_->sources.set_path(source, staged->destination.string());
            }
        }
        prepared.module_paths_ = state_.module_paths_;

        using DeclarationKey = std::pair<TypeIdentity, std::size_t>;
        auto declaration_key = [](EditableDocumentState const& document,
                                  DeclarationId const info) -> DeclarationKey {
            return {document.declarations_.identity[document.declarations_.index(info)],
                    static_cast<std::size_t>(
                        payload_at(document.manifest_, document.declarations_, info).kind())};
        };
        std::map<DeclarationKey, DeclarationId> draft_ids;
        for (auto const& info : state_.declarations_.ordered) {
            if (!draft_ids.emplace(declaration_key(state_, info), info).second) {
                throw std::logic_error{"Ambiguous draft declaration identity during save"};
            }
        }
        if (prepared.declarations_.ordered.size() != draft_ids.size()) {
            throw std::logic_error{"Saved declarations do not match the current draft"};
        }
        EditorDeclarations saved_declarations;
        if (!state_.declarations_.identity.empty()) {
            saved_declarations.reserve(
                DeclarationId{static_cast<std::uint32_t>(state_.declarations_.identity.size())});
        }
        for (auto const info : prepared.declarations_.ordered) {
            auto const found{draft_ids.find(declaration_key(prepared, info))};
            if (found == draft_ids.end()) {
                throw std::logic_error{"Saved declaration does not match the current draft"};
            }
            auto const id{found->second};
            saved_declarations.activate(id);
            auto const row{saved_declarations.index(id)};
            auto const source_row{prepared.declarations_.index(info)};
            saved_declarations.identity[row] = prepared.declarations_.identity[source_row];
            saved_declarations.module_index[row] = prepared.declarations_.module_index[source_row];
            saved_declarations.declaration_index[row] =
                prepared.declarations_.declaration_index[source_row];
            saved_declarations.source[row] = prepared.declarations_.source[source_row];
            draft_ids.erase(found);
        }
        if (!draft_ids.empty()) {
            throw std::logic_error{"Saved declarations do not match the current draft"};
        }
        prepared.declarations_ = std::move(saved_declarations);

        std::vector<std::filesystem::path> saved;
        saved.reserve(recovery.size());
        for (auto const& file : recovery) {
            saved.push_back(file.destination);
        }

        verify_snapshots();
        std::size_t published{};
        cleanup_safe = false;
        try {
            for (auto const& file : recovery) {
                publish_file(file.staged, file.destination);
                ++published;
            }
        } catch (std::exception const& publication_error) {
            auto message{std::string{"Cannot publish LispB source: "} + publication_error.what()};
            auto rollback_failed{false};
            while (published > 0) {
                auto const& file{recovery[--published]};
                try {
                    {
                        std::ifstream input{file.destination, std::ios::binary};
                        if (!input) {
                            throw std::runtime_error{
                                "rollback skipped: cannot open destination for verification"};
                        }
                        auto const current{std::string{std::istreambuf_iterator<char>{input},
                                                       std::istreambuf_iterator<char>{}}};
                        if (input.bad()) {
                            throw std::runtime_error{
                                "rollback skipped: cannot read destination for verification"};
                        }
                        if (current != file.published_content) {
                            throw std::runtime_error{"rollback skipped: destination no longer "
                                                     "matches published content"};
                        }
                    }
                    publish_file(file.original, file.destination);
                } catch (std::exception const& restoration_error) {
                    rollback_failed = true;
                    message += "; cannot restore '" + file.destination.string() +
                               "': " + restoration_error.what() +
                               "; original recovery copy: " + file.original.string();
                }
            }
            if (rollback_failed) {
                for (auto const& file : recovery) {
                    message += "; recovery for '" + file.destination.string() +
                               "': " + file.directory.string();
                }
            } else {
                cleanup_safe = true;
                cleanup();
            }
            return std::unexpected{SchemaEditError{std::move(message)}};
        }

        static_assert(std::is_nothrow_move_assignable_v<EditableSchemaDocument>);
        state_ = std::move(prepared);
        storage_->sources.refresh();
        cleanup_safe = true;
        cleanup();
        return saved;
    } catch (std::exception const& error) {
        cleanup();
        return std::unexpected{SchemaEditError{error.what()}};
    }
}

void EditableSchemaDocument::initialize_declarations(
    EditableDocumentState& state, std::vector<std::optional<SourceRange>> source_ranges) {
    state.declarations_.clear();
    auto source_index{std::size_t{}};
    for_each_declaration(
        state.manifest_,
        [&](std::size_t const module_index,
            std::size_t const declaration_index,
            codegen::ModuleSettingsId const settings,
            IdentifierId const name) {
            auto const type{
                state.types_.find_declared(state.manifest_.modules[module_index], name)};
            auto identity{
                type.has_value()
                    ? state.types_.identity(*type)
                    : TypeIdentity{
                          .origin = TypeOrigin::declaration,
                          .module_name =
                              state.manifest_.declarations.module_settings_store
                                  .name[state.manifest_.declarations.module_settings_store.index(
                                      settings)],
                          .namespace_name =
                              state.manifest_.declarations.module_settings_store
                                  .namespace_name[state.manifest_.declarations.module_settings_store
                                                      .index(settings)]
                                  .value_or(IdentifierId{}),
                          .name = name}};
            auto source{source_index < source_ranges.size() ? source_ranges[source_index]
                                                            : std::nullopt};
            auto const id{state.declarations_.allocate()};
            state.declarations_.activate(id);
            auto const row{state.declarations_.index(id)};
            state.declarations_.identity[row] = identity;
            state.declarations_.module_index[row] = module_index;
            state.declarations_.declaration_index[row] = declaration_index;
            state.declarations_.source[row] = source;
            ++source_index;
        });
    if (!source_ranges.empty() && source_index != source_ranges.size()) {
        throw std::logic_error{"Source declaration count does not match resolved schema"};
    }
}

auto EditableSchemaDocument::execute(SchemaEditCommand const command, bool const record_inverse)
    -> std::expected<std::optional<SchemaEditCommand>, SchemaEditError> {
    auto& store{storage_->declarations};
    auto& strings{storage_->strings};
    auto use_description = [&](TypeUseId const use) {
        return text(store,
                    store.module_settings_store.name[store.module_settings_store.index(
                        store.module_settings(state_.types_.module(use)))]) +
               " / " +
               (state_.types_.declaration(use).value != 0
                    ? text(store, codegen::declaration_name(store, state_.types_.declaration(use)))
                    : codegen::type_reference_role_text(strings, state_.types_.role(use)));
    };
    auto deletion_blocker = [&](DeclarationId const info) -> std::optional<SchemaEditError> {
        auto const owned_types{state_.types_.types_for_declaration(
            payload_at(state_.manifest_, state_.declarations_, info))};
        for (auto const& [registered_name_id, cpp_type] : state_.manifest_.types.entries) {
            auto const registered_name{
                std::string{state_.manifest_.types.cpp_types.strings().view(registered_name_id)}};
            static_cast<void>(cpp_type);
            auto const registered{state_.types_.find_registered(registered_name_id)};
            if (registered.has_value() &&
                std::ranges::find(owned_types, *registered) != owned_types.end()) {
                return SchemaEditError{
                    "Cannot delete declaration '" +
                    text(store,
                         state_.declarations_.identity[state_.declarations_.index(info)].name) +
                    "'; it is registered as '@" + registered_name + "'"};
            }
        }

        for (auto const& use : state_.types_.type_uses()) {
            if (state_.types_.declaration(use) !=
                    payload_at(state_.manifest_, state_.declarations_, info) &&
                std::ranges::find(
                    owned_types,
                    state_.types_.occurrences()
                        .type[state_.types_.occurrences().index(state_.types_.target(use))]) !=
                    owned_types.end()) {
                return SchemaEditError{
                    "Cannot delete declaration '" +
                    text(store,
                         state_.declarations_.identity[state_.declarations_.index(info)].name) +
                    "'; it is used by '" + use_description(use) + "'"};
            }
        }
        for (auto const owned : owned_types) {
            for (auto const user : state_.types_.users_of(owned)) {
                auto const& node{user};
                if (state_.types_.source_declaration(node) !=
                    payload_at(state_.manifest_, state_.declarations_, info)) {
                    return SchemaEditError{
                        "Cannot delete declaration '" +
                        text(store,
                             state_.declarations_.identity[state_.declarations_.index(info)].name) +
                        "'; it is used by '" +
                        std::string{strings.view(state_.types_.cpp_spelling(node))} + "'"};
                }
            }
        }
        return std::nullopt;
    };
    auto creation_source = [&](DeclarationId const declaration_id) -> std::optional<SourceRange> {
        auto const found{state_.source_tombstones_.find(declaration_id)};
        return found == state_.source_tombstones_.end() ? std::nullopt
                                                        : std::optional{found->second};
    };
    auto remember_source_tombstone = [&](DeclarationId const info) {
        if (state_.declarations_.source[state_.declarations_.index(info)].has_value()) {
            state_.source_tombstones_.insert_or_assign(
                info, *state_.declarations_.source[state_.declarations_.index(info)]);
        }
    };

    auto const index{command.index()};
    switch (command.kind()) {
        case SchemaEditKind::create_declaration: {
            if (!lispb::schema::is_valid(state_.create_declaration_edits_.declarations[index]) ||
                state_.declarations_.find(state_.create_declaration_edits_.declarations[index])
                    .has_value()) {
                return std::unexpected{SchemaEditError{"New declaration requires a unique id"}};
            }
            if (state_.create_declaration_edits_.module_indices[index] >=
                state_.manifest_.modules.size()) {
                return std::unexpected{SchemaEditError{"Unknown module index"}};
            }
            auto candidate{state_.manifest_};
            auto const module_index{state_.create_declaration_edits_.module_indices[index]};
            if (candidate.modules[module_index].kind() != codegen::ModuleKind::normal) {
                return std::unexpected{SchemaEditError{"Declaration requires a normal module"}};
            }
            auto const current{declaration_ids(candidate, module_index)};
            std::vector<codegen::DeclarationHandle> declarations{current.begin(), current.end()};
            auto const insertion{state_.create_declaration_edits_.insertion_indices[index].value_or(
                declarations.size())};
            if (insertion > declarations.size()) {
                return std::unexpected{SchemaEditError{"Invalid declaration insertion index"}};
            }
            declarations.insert(declarations.begin() + static_cast<std::ptrdiff_t>(insertion),
                                state_.create_declaration_edits_.versions[index]);
            set_declarations(candidate, module_index, declarations);
            TypeGraph candidate_types;
            try {
                candidate_types = resolve_type_graph(candidate);
            } catch (std::exception const& error) {
                return std::unexpected{SchemaEditError{error.what()}};
            }
            auto const identity{declaration_identity(
                candidate, module_index, state_.create_declaration_edits_.versions[index])};
            for (auto& info : state_.declarations_.ordered) {
                if (state_.declarations_.module_index[state_.declarations_.index(info)] ==
                        state_.create_declaration_edits_.module_indices[index] &&
                    state_.declarations_.declaration_index[state_.declarations_.index(info)] >=
                        insertion) {
                    ++state_.declarations_.declaration_index[state_.declarations_.index(info)];
                }
            }
            auto const declaration_id{state_.create_declaration_edits_.declarations[index]};
            state_.declarations_.activate(declaration_id);
            auto const row{state_.declarations_.index(declaration_id)};
            state_.declarations_.identity[row] = identity;
            state_.declarations_.module_index[row] = module_index;
            state_.declarations_.declaration_index[row] = insertion;
            state_.declarations_.source[row] = creation_source(declaration_id);
            state_.manifest_ = std::move(candidate);
            state_.types_ = std::move(candidate_types);
            return record_inverse
                     ? delete_declaration(state_.create_declaration_edits_.declarations[index])
                     : command;
        }
        case SchemaEditKind::replace_declaration: {
            auto const info{
                state_.declarations_.find(state_.replace_declaration_edits_.declarations[index])};
            if (!info.has_value()) {
                return std::unexpected{SchemaEditError{"Unknown declaration"}};
            }
            auto candidate{state_.manifest_};
            auto const previous{payload_at(candidate, state_.declarations_, *info)};
            auto const current{state_.replace_declaration_edits_.versions[index]};
            if (previous.kind() != current.kind()) {
                return std::unexpected{SchemaEditError{"Replacement declaration kind differs"}};
            }
            if (codegen::declaration_name(store, previous) !=
                codegen::declaration_name(store, current)) {
                return std::unexpected{SchemaEditError{"Use RenameDeclaration to change a name"}};
            }
            set_payload(candidate, state_.declarations_, *info, current);
            TypeGraph candidate_types;
            try {
                candidate_types = resolve_type_graph(candidate);
                for (auto const owned : state_.types_.types_for_declaration(
                         payload_at(state_.manifest_, state_.declarations_, *info))) {
                    if (candidate_types.find(state_.types_.identity(owned)).has_value()) {
                        continue;
                    }
                    for (auto const& [name_id, registration] : state_.manifest_.types.entries) {
                        auto const name{
                            std::string{state_.manifest_.types.cpp_types.strings().view(name_id)}};
                        static_cast<void>(registration);
                        if (state_.types_.find_registered(name_id) == owned) {
                            throw std::invalid_argument{
                                "Cannot remove or rename generated type '" +
                                std::string{strings.view(state_.types_.cpp_spelling(owned))} +
                                "'; it is registered as '@" + name + "'"};
                        }
                    }
                    codegen::visit_type_references(
                        store,
                        current,
                        [&](codegen::TypeReferenceRole const& role,
                            codegen::TypeRefId const reference,
                            codegen::TypeReferenceKind) {
                            if (state_.types_.find_reference(
                                    reference,
                                    state_.manifest_
                                        .modules[state_.declarations_.module_index
                                                     [state_.declarations_.index(*info)]]) ==
                                owned) {
                                throw std::invalid_argument{
                                    "Cannot remove or rename generated type '" +
                                    std::string{strings.view(state_.types_.cpp_spelling(owned))} +
                                    "'; it is used by '" +
                                    text(store,
                                         state_.declarations_
                                             .identity[state_.declarations_.index(*info)]
                                             .name) +
                                    " / " +
                                    codegen::type_reference_role_text(storage_->strings, role) +
                                    "'"};
                            }
                        });
                    for (auto const& use : state_.types_.type_uses()) {
                        if (state_.types_.occurrences().type[state_.types_.occurrences().index(
                                state_.types_.target(use))] == owned &&
                            state_.types_.declaration(use) != previous) {
                            throw std::invalid_argument{
                                "Cannot remove or rename generated type '" +
                                std::string{strings.view(state_.types_.cpp_spelling(owned))} +
                                "'; it is used by '" + use_description(use) + "'"};
                        }
                    }
                }
            } catch (std::exception const& error) {
                return std::unexpected{SchemaEditError{error.what()}};
            }
            state_.manifest_ = std::move(candidate);
            state_.types_ = std::move(candidate_types);
            return record_inverse
                     ? replace_declaration(state_.replace_declaration_edits_.declarations[index],
                                           std::move(previous))
                     : command;
        }
        case SchemaEditKind::delete_declaration: {
            auto const found{state_.declarations_.find(state_.delete_declaration_edits_[index])};
            if (!found.has_value()) {
                return std::unexpected{SchemaEditError{"Unknown declaration"}};
            }
            if (auto const blocker{deletion_blocker(*found)}; blocker.has_value()) {
                return std::unexpected{*blocker};
            }
            auto const info{*found};
            auto candidate{state_.manifest_};
            auto const current{declaration_ids(
                candidate, state_.declarations_.module_index[state_.declarations_.index(info)])};
            std::vector<codegen::DeclarationHandle> declarations{current.begin(), current.end()};
            auto const schema{
                declarations[state_.declarations_
                                 .declaration_index[state_.declarations_.index(info)]]};
            declarations.erase(
                declarations.begin() +
                static_cast<std::ptrdiff_t>(
                    state_.declarations_.declaration_index[state_.declarations_.index(info)]));
            set_declarations(candidate,
                             state_.declarations_.module_index[state_.declarations_.index(info)],
                             declarations);
            TypeGraph candidate_types;
            try {
                candidate_types = resolve_type_graph(candidate);
            } catch (std::exception const& error) {
                return std::unexpected{SchemaEditError{error.what()}};
            }
            remember_source_tombstone(info);
            state_.declarations_.remove(state_.delete_declaration_edits_[index]);
            for (auto& existing : state_.declarations_.ordered) {
                if (state_.declarations_.module_index[state_.declarations_.index(existing)] ==
                        state_.declarations_.module_index[state_.declarations_.index(info)] &&
                    state_.declarations_.declaration_index[state_.declarations_.index(existing)] >
                        state_.declarations_.declaration_index[state_.declarations_.index(info)]) {
                    --state_.declarations_.declaration_index[state_.declarations_.index(existing)];
                }
            }
            state_.manifest_ = std::move(candidate);
            state_.types_ = std::move(candidate_types);
            return record_inverse
                     ? create_declaration(
                           state_.delete_declaration_edits_[index],
                           state_.declarations_.module_index[state_.declarations_.index(info)],
                           std::move(schema),
                           state_.declarations_.declaration_index[state_.declarations_.index(info)])
                     : command;
        }
        case SchemaEditKind::create_module: {
            auto const source_file{state_.create_module_edits_.source_files[index]};
            if (std::ranges::find(state_.source_files_, source_file) ==
                    state_.source_files_.end() ||
                storage_->sources.kind(source_file) != codegen::sexpr::SourceFileKind::module ||
                std::ranges::find(state_.module_paths_,
                                  storage_->sources.filesystem_path(source_file)) ==
                    state_.module_paths_.end()) {
                return std::unexpected{
                    SchemaEditError{"New modules require an existing loaded module source"}};
            }
            if (!render_editable_module(*storage_, state_.create_module_edits_.versions[index])
                     .has_value()) {
                return std::unexpected{SchemaEditError{"Module kind is not editable"}};
            }
            if (store.normal_modules
                    .declarations[store.normal_modules.index(
                        state_.create_module_edits_.versions[index])]
                    .size() != 0) {
                return std::unexpected{
                    SchemaEditError{"CreateModule requires an empty module schema"}};
            }

            auto const module_index{state_.manifest_.modules.size()};
            state_.manifest_.modules.push_back(state_.create_module_edits_.versions[index]);
            state_.module_source_ranges_.push_back(std::nullopt);

            state_.pending_module_sources_.emplace(module_index,
                                                   state_.create_module_edits_.source_files[index]);
            try {
                state_.types_ = resolve_type_graph(state_.manifest_);
            } catch (std::exception const& error) {
                state_.pending_module_sources_.erase(module_index);
                state_.module_source_ranges_.pop_back();

                state_.manifest_.modules.pop_back();
                return std::unexpected{SchemaEditError{error.what()}};
            }
            return record_inverse ? delete_module(module_index) : command;
        }
        case SchemaEditKind::delete_module: {
            if (state_.delete_module_edits_[index] >= state_.manifest_.modules.size()) {
                return std::unexpected{SchemaEditError{"Unknown module index"}};
            }
            if (state_.manifest_.modules.size() == 1) {
                return std::unexpected{
                    SchemaEditError{"The project must retain at least one module"}};
            }

            auto const source{state_.module_source_ranges_[state_.delete_module_edits_[index]]};
            auto const pending{
                state_.pending_module_sources_.find(state_.delete_module_edits_[index])};
            if (!source.has_value() && pending == state_.pending_module_sources_.end()) {
                return std::unexpected{SchemaEditError{"Module has no editable source ownership"}};
            }

            auto const module{state_.manifest_.modules[state_.delete_module_edits_[index]]};
            auto const module_name{
                text(store,
                     store.module_settings_store
                         .name[store.module_settings_store.index(store.module_settings(module))])};
            for (auto const& info : state_.declarations_.ordered) {
                if (state_.declarations_.module_index[state_.declarations_.index(info)] !=
                    state_.delete_module_edits_[index]) {
                    continue;
                }
                auto const owned_types{state_.types_.types_for_declaration(
                    payload_at(state_.manifest_, state_.declarations_, info))};
                for (auto const use : state_.types_.type_uses()) {
                    if (state_.types_.module(use) != module &&
                        std::ranges::find(
                            owned_types,
                            state_.types_.occurrences().type[state_.types_.occurrences().index(
                                state_.types_.target(use))]) != owned_types.end()) {
                        return std::unexpected{SchemaEditError{
                            "Cannot delete module '" + module_name + "'; declaration '" +
                            text(store,
                                 state_.declarations_.identity[state_.declarations_.index(info)]
                                     .name) +
                            "' is used by '" + use_description(use) + "'"}};
                    }
                }
                for (auto const& [registered_name, registration] : state_.manifest_.types.entries) {
                    static_cast<void>(registration);
                    auto const registered{state_.types_.find_registered(registered_name)};
                    if (registered &&
                        std::ranges::find(owned_types, *registered) != owned_types.end()) {
                        return std::unexpected{
                            SchemaEditError{"Cannot delete module '" + module_name +
                                            "'; it contains a type registered as '@" +
                                            text(store, registered_name) + "'"}};
                    }
                }
                for (auto const owned : owned_types) {
                    for (auto const user : state_.types_.users_of(owned)) {
                        auto const node{user};
                        if (state_.types_.module(node) != module) {
                            return std::unexpected{SchemaEditError{
                                "Cannot delete module '" + module_name + "'; declaration '" +
                                text(store,
                                     state_.declarations_.identity[state_.declarations_.index(info)]
                                         .name) +
                                "' is used by '" +
                                std::string{strings.view(state_.types_.cpp_spelling(node))} + "'"}};
                        }
                    }
                }
            }

            auto candidate{state_.manifest_};
            candidate.modules.erase(
                candidate.modules.begin() +
                static_cast<std::ptrdiff_t>(state_.delete_module_edits_[index]));
            TypeGraph candidate_types;
            try {
                candidate_types = resolve_type_graph(candidate);
            } catch (std::exception const& error) {
                return std::unexpected{SchemaEditError{error.what()}};
            }

            std::vector<DeclarationId> deleted_declarations;
            for (auto const& info : state_.declarations_.ordered) {
                if (state_.declarations_.module_index[state_.declarations_.index(info)] ==
                    state_.delete_module_edits_[index]) {
                    deleted_declarations.push_back(info);
                }
            }

            auto const inverse{
                record_inverse
                    ? restore_module(state_.delete_module_edits_[index],
                                     state_.manifest_.modules[state_.delete_module_edits_[index]],
                                     source,
                                     pending == state_.pending_module_sources_.end()
                                         ? std::nullopt
                                         : std::optional{pending->second},
                                     deleted_declarations)
                    : command};
            state_.manifest_ = std::move(candidate);
            state_.types_ = std::move(candidate_types);
            state_.module_source_ranges_.erase(
                state_.module_source_ranges_.begin() +
                static_cast<std::ptrdiff_t>(state_.delete_module_edits_[index]));

            if (source.has_value()) {
                state_.deleted_module_source_ranges_.push_back(*source);
            }
            for (auto const id : deleted_declarations) {
                state_.declarations_.remove(id);
            }
            for (auto& info : state_.declarations_.ordered) {
                if (state_.declarations_.module_index[state_.declarations_.index(info)] >
                    state_.delete_module_edits_[index]) {
                    --state_.declarations_.module_index[state_.declarations_.index(info)];
                }
            }
            auto shifted_pending{std::map<std::size_t, SourceFileId>{}};
            for (auto const& [module_index, source_file] : state_.pending_module_sources_) {
                if (module_index != state_.delete_module_edits_[index]) {
                    shifted_pending.emplace(module_index > state_.delete_module_edits_[index]
                                                ? module_index - 1
                                                : module_index,
                                            source_file);
                }
            }
            state_.pending_module_sources_ = std::move(shifted_pending);
            return inverse;
        }
        case SchemaEditKind::restore_module: {
            auto const restored_declarations{
                std::span{state_.restore_module_edits_.declarations}.subspan(
                    state_.restore_module_edits_.declaration_offsets[index],
                    state_.restore_module_edits_.declaration_counts[index])};
            if (state_.restore_module_edits_.module_indices[index] >
                state_.manifest_.modules.size()) {
                return std::unexpected{SchemaEditError{"Unknown module insertion index"}};
            }
            if (state_.restore_module_edits_.sources[index].has_value() ==
                state_.restore_module_edits_.pending_source_files[index].has_value()) {
                return std::unexpected{
                    SchemaEditError{"Module restore has invalid source ownership"}};
            }
            auto deleted_range{state_.deleted_module_source_ranges_.end()};
            if (state_.restore_module_edits_.sources[index].has_value()) {
                deleted_range = std::ranges::find_if(
                    state_.deleted_module_source_ranges_, [&](SourceRange const& range) {
                        return range == *state_.restore_module_edits_.sources[index];
                    });
                if (deleted_range == state_.deleted_module_source_ranges_.end()) {
                    return std::unexpected{
                        SchemaEditError{"Deleted module source range is missing"}};
                }
            }

            auto candidate{state_.manifest_};
            candidate.modules.insert(
                candidate.modules.begin() +
                    static_cast<std::ptrdiff_t>(state_.restore_module_edits_.module_indices[index]),
                state_.restore_module_edits_.versions[index]);
            TypeGraph candidate_types;
            try {
                candidate_types = resolve_type_graph(candidate);
            } catch (std::exception const& error) {
                return std::unexpected{SchemaEditError{error.what()}};
            }

            state_.manifest_ = std::move(candidate);
            state_.types_ = std::move(candidate_types);
            state_.module_source_ranges_.insert(
                state_.module_source_ranges_.begin() +
                    static_cast<std::ptrdiff_t>(state_.restore_module_edits_.module_indices[index]),
                state_.restore_module_edits_.sources[index]);

            if (state_.restore_module_edits_.sources[index].has_value()) {
                state_.deleted_module_source_ranges_.erase(deleted_range);
            }
            for (auto& info : state_.declarations_.ordered) {
                if (state_.declarations_.module_index[state_.declarations_.index(info)] >=
                    state_.restore_module_edits_.module_indices[index]) {
                    ++state_.declarations_.module_index[state_.declarations_.index(info)];
                }
            }
            for (auto const id : restored_declarations) {
                state_.declarations_.activate(id);
            }
            std::ranges::sort(state_.declarations_.ordered,
                              [&](DeclarationId first, DeclarationId second) {
                                  auto const left{state_.declarations_.index(first)};
                                  auto const right{state_.declarations_.index(second)};
                                  return std::pair{state_.declarations_.module_index[left],
                                                   state_.declarations_.declaration_index[left]} <
                                         std::pair{state_.declarations_.module_index[right],
                                                   state_.declarations_.declaration_index[right]};
                              });
            auto shifted_pending{std::map<std::size_t, SourceFileId>{}};
            for (auto const& [module_index, source_file] : state_.pending_module_sources_) {
                shifted_pending.emplace(module_index >=
                                                state_.restore_module_edits_.module_indices[index]
                                            ? module_index + 1
                                            : module_index,
                                        source_file);
            }
            if (state_.restore_module_edits_.pending_source_files[index].has_value()) {
                shifted_pending.emplace(state_.restore_module_edits_.module_indices[index],
                                        *state_.restore_module_edits_.pending_source_files[index]);
            }
            state_.pending_module_sources_ = std::move(shifted_pending);
            return record_inverse
                     ? delete_module(state_.restore_module_edits_.module_indices[index])
                     : command;
        }
        case SchemaEditKind::move_declaration: {
            auto const declaration_it{
                state_.declarations_.find(state_.move_declaration_edits_.declarations[index])};
            if (!declaration_it.has_value()) {
                return std::unexpected{SchemaEditError{"Unknown declaration"}};
            }
            if (state_.move_declaration_edits_.module_indices[index] >=
                state_.manifest_.modules.size()) {
                return std::unexpected{SchemaEditError{"Unknown destination module index"}};
            }
            if (state_.move_declaration_edits_.module_indices[index] ==
                state_.declarations_.module_index[state_.declarations_.index(*declaration_it)]) {
                return std::nullopt;
            }
            if (state_.declarations_.source[state_.declarations_.index(*declaration_it)]
                    .has_value() &&
                state_.pending_module_sources_.contains(
                    state_.move_declaration_edits_.module_indices[index])) {
                return std::unexpected{SchemaEditError{
                    "Save a newly created module before moving source-backed declarations into "
                    "it"}};
            }

            auto const source_module{
                state_.manifest_
                    .modules[state_.declarations_
                                 .module_index[state_.declarations_.index(*declaration_it)]]};
            auto const destination_module{
                state_.manifest_.modules[state_.move_declaration_edits_.module_indices[index]]};
            if (destination_module.kind() != codegen::ModuleKind::normal) {
                return std::unexpected{SchemaEditError{"Destination module is incompatible"}};
            }
            auto const source_settings{store.module_settings(source_module)};
            auto const destination_settings{store.module_settings(destination_module)};

            auto restored_source{std::optional<SourceRange>{}};
            if (state_.move_declaration_edits_.restore_source_ownership[index]) {
                auto const tombstone{state_.source_tombstones_.find(
                    state_.move_declaration_edits_.declarations[index])};
                if (tombstone == state_.source_tombstones_.end()) {
                    return std::unexpected{
                        SchemaEditError{"Moved declaration has no source ownership to restore"}};
                }
                restored_source = tombstone->second;
            }

            auto const previous_manifest{state_.manifest_};
            auto const previous_declarations{state_.declarations_};
            auto const previous_tombstones{state_.source_tombstones_};
            // Snapshot for undo before state_.declarations_ is modified.
            // NOLINTNEXTLINE(performance-unnecessary-copy-initialization)
            auto const original_info{*declaration_it};
            auto const target{state_.types_.find(
                previous_declarations.identity[previous_declarations.index(original_info)])};
            auto const owned_types{state_.types_.types_for_declaration(
                payload_at(state_.manifest_, state_.declarations_, original_info))};
            if (!target && codegen::has_primary_semantic_type(
                               payload_at(state_.manifest_, state_.declarations_, original_info))) {
                return std::unexpected{
                    SchemaEditError{"Declaration is missing from the type graph"}};
            }
            auto const namespace_changed{
                store.module_settings_store
                    .namespace_name[store.module_settings_store.index(source_settings)] !=
                store.module_settings_store
                    .namespace_name[store.module_settings_store.index(destination_settings)]};
            if (namespace_changed) {
                for (auto const& [registered_name_id, cpp_type] : state_.manifest_.types.entries) {
                    auto const registered_name{std::string{
                        state_.manifest_.types.cpp_types.strings().view(registered_name_id)}};
                    static_cast<void>(cpp_type);
                    auto const registered{state_.types_.find_registered(registered_name_id)};
                    if (registered.has_value() &&
                        std::ranges::find(owned_types, *registered) != owned_types.end()) {
                        return std::unexpected{SchemaEditError{
                            "Moving a declaration registered as '@" + registered_name +
                            "' across namespaces requires source-aware types-registry "
                            "editing"}};
                    }
                }
            }
            try {
                auto const moving{
                    payload_at(state_.manifest_, state_.declarations_, original_info)};
                if (auto const soa{((moving).kind() == lispb::codegen::DeclarationKind::soa
                                        ? std::optional{moving}
                                        : std::nullopt)};
                    soa &&
                    std::ranges::any_of(
                        store.items(store.soas.members[store.soas.index(*soa)]),
                        [&](auto const id) {
                            auto const member{id};
                            return store.soa_members.nested_schema[store.soa_members.index(member)]
                                       .has_value() ||
                                   store.soa_members.fixed_schema[store.soa_members.index(member)]
                                       .has_value();
                        })) {
                    throw std::invalid_argument{
                        "Cannot move a SoA across modules while module-local nested/fixed schema "
                        "references exist"};
                }
                auto const rewritten{codegen::rewrite_type_references(
                    store,
                    moving,
                    [&](codegen::TypeReferenceRole const&,
                        codegen::TypeRefId const id,
                        codegen::TypeReferenceKind) {
                        auto const reference{store.type_refs.clone(id)};
                        auto const binding{state_.types_.find_reference(id, source_module)};
                        if (strings.view(store.type_refs.name[store.type_refs.index(reference)])
                                .starts_with('@') ||
                            !binding ||
                            std::ranges::find(owned_types, *binding) != owned_types.end()) {
                            return id;
                        }
                        auto const node{*binding};
                        store.type_refs.name[store.type_refs.index(reference)] =
                            state_.types_.module_name(node) ==
                                    store.module_settings_store
                                        .name[store.module_settings_store.index(
                                            destination_settings)]
                                ? state_.types_.name(node)
                                : strings.intern(strings.view(state_.types_.cpp_spelling(node)));
                        return reference;
                    })};
                set_payload(state_.manifest_, state_.declarations_, original_info, rewritten);
                for (auto const owned : owned_types) {
                    auto const& name{state_.types_.name(owned)};
                    auto const new_spelling{
                        store.module_settings_store.namespace_name[store.module_settings_store
                                                                       .index(destination_settings)]
                            ? strings.intern(
                                  text(store,
                                       *store.module_settings_store
                                            .namespace_name[store.module_settings_store.index(
                                                destination_settings)]) +
                                  "::" + text(store, name))
                            : name};
                    repair_semantic_references(
                        state_.manifest_,
                        state_.types_,
                        state_.declarations_,
                        owned,
                        new_spelling,
                        previous_declarations
                            .module_index[previous_declarations.index(original_info)],
                        previous_declarations.identity[previous_declarations.index(original_info)]
                            .name,
                        LocalSoaReferencePolicy::reject,
                        name,
                        state_.move_declaration_edits_.module_indices[index]);
                }
            } catch (std::exception const& error) {
                state_.manifest_ = previous_manifest;
                return std::unexpected{SchemaEditError{error.what()}};
            }
            auto const source_ids{declaration_ids(
                state_.manifest_,
                previous_declarations.module_index[previous_declarations.index(original_info)])};
            auto const destination_index{state_.move_declaration_edits_.module_indices[index]};
            auto const destination_ids{declaration_ids(state_.manifest_, destination_index)};
            std::vector<codegen::DeclarationHandle> source{source_ids.begin(), source_ids.end()};
            std::vector<codegen::DeclarationHandle> destination{destination_ids.begin(),
                                                                destination_ids.end()};
            auto const insertion{state_.move_declaration_edits_.insertion_indices[index].value_or(
                destination.size())};
            if (insertion > destination.size()) {
                state_.manifest_ = previous_manifest;
                return std::unexpected{SchemaEditError{"Invalid destination insertion index"}};
            }
            destination.insert(
                destination.begin() + static_cast<std::ptrdiff_t>(insertion),
                source[previous_declarations
                           .declaration_index[previous_declarations.index(original_info)]]);
            source.erase(source.begin() +
                         static_cast<std::ptrdiff_t>(
                             previous_declarations
                                 .declaration_index[previous_declarations.index(original_info)]));
            set_declarations(
                state_.manifest_,
                previous_declarations.module_index[previous_declarations.index(original_info)],
                source);
            set_declarations(state_.manifest_, destination_index, destination);

            remember_source_tombstone(original_info);
            state_.declarations_.module_index[state_.declarations_.index(*declaration_it)] =
                state_.move_declaration_edits_.module_indices[index];
            state_.declarations_.source[state_.declarations_.index(*declaration_it)] =
                restored_source;
            try {
                auto resolved{resolve_type_graph(state_.manifest_)};
                refresh_declaration_locations(state_.manifest_, resolved, state_.declarations_);
                validate_relocated_references(
                    state_.manifest_,
                    state_.types_,
                    resolved,
                    payload_at(previous_manifest, previous_declarations, original_info),
                    payload_at(state_.manifest_, state_.declarations_, *declaration_it),
                    previous_declarations.identity[previous_declarations.index(original_info)],
                    state_.declarations_.identity[state_.declarations_.index(*declaration_it)]);
                state_.types_ = std::move(resolved);
            } catch (std::exception const& error) {
                state_.manifest_ = previous_manifest;
                state_.declarations_ = previous_declarations;
                state_.source_tombstones_ = previous_tombstones;
                return std::unexpected{SchemaEditError{error.what()}};
            }
            return record_inverse
                     ? move_declaration(
                           state_.move_declaration_edits_.declarations[index],
                           previous_declarations
                               .module_index[previous_declarations.index(original_info)],
                           previous_declarations
                               .declaration_index[previous_declarations.index(original_info)],
                           previous_declarations.source[previous_declarations.index(original_info)]
                               .has_value())
                     : command;
        }
        case SchemaEditKind::set_enumerator_display_name:
        case SchemaEditKind::set_enumerator_name: {
            bool const rename{command.kind() == SchemaEditKind::set_enumerator_name};
            auto const declaration_id{
                rename ? state_.enumerator_name_edits_.declarations[index]
                       : state_.enumerator_display_name_edits_.declarations[index]};
            auto const info{state_.declarations_.find(declaration_id)};
            if (!info) {
                return std::unexpected{SchemaEditError{"Unknown declaration id"}};
            }
            auto schema{
                ((payload_at(state_.manifest_, state_.declarations_, *info)).kind() ==
                         lispb::codegen::DeclarationKind::enumeration
                     ? std::optional{payload_at(state_.manifest_, state_.declarations_, *info)}
                     : std::nullopt)};
            if (!schema) {
                return std::unexpected{
                    SchemaEditError{"Enumerator edits require an enum declaration"}};
            }
            auto const name{rename ? state_.enumerator_name_edits_.current_names[index]
                                   : state_.enumerator_display_name_edits_.names[index]};
            auto const current{store.items(store.enums.values[store.enums.index(*schema)])};
            std::vector<codegen::EnumeratorId> values{current.begin(), current.end()};
            auto const found{std::ranges::find_if(values, [&](auto const id) {
                return store.enumerators.name[store.enumerators.index(id)] == name;
            })};
            if (found == values.end()) {
                return std::unexpected{SchemaEditError{
                    "Enum '" + text(store, store.enums.name[store.enums.index(*schema)]) +
                    "' has no enumerator named '" + text(store, name) + "'"}};
            }
            schema = store.enums.clone(*schema);
            auto const value{store.enumerators.clone(*found)};
            if (rename) {
                auto const new_name{state_.enumerator_name_edits_.new_names[index]};
                if (name == new_name) {
                    return std::nullopt;
                }
                store.enumerators.name[store.enumerators.index(value)] = new_name;
                if (store.enums.count[store.enums.index(*schema)] == name) {
                    store.enums.count[store.enums.index(*schema)] = new_name;
                }
            } else {
                auto const new_display{state_.enumerator_display_name_edits_.display_names[index]};
                if (store.enumerators.display_name[store.enumerators.index(value)] == new_display ||
                    (store.enumerators.display_name[store.enumerators.index(value)] &&
                     new_display &&
                     strings.view(
                         *store.enumerators.display_name[store.enumerators.index(value)]) ==
                         strings.view(*new_display))) {
                    return std::nullopt;
                }
                store.enumerators.display_name[store.enumerators.index(value)] = new_display;
            }
            *found = value;
            store.enums.values[store.enums.index(*schema)] = store.range(values);
            // Use the same immutable replacement operation for editing and undo.
            return execute(replace_declaration(declaration_id, *schema), record_inverse);
        }
        case SchemaEditKind::rename_declaration: {
            auto const new_name{state_.rename_declaration_edits_.names[index]};
            auto const declaration_it{
                state_.declarations_.find(state_.rename_declaration_edits_.declarations[index])};
            if (!declaration_it.has_value()) {
                return std::unexpected{SchemaEditError{"Unknown declaration"}};
            }
            if (new_name ==
                state_.declarations_.identity[state_.declarations_.index(*declaration_it)].name) {
                return std::nullopt;
            }

            auto const target{state_.types_.find(
                state_.declarations_.identity[state_.declarations_.index(*declaration_it)])};
            auto const owned_types{state_.types_.types_for_declaration(
                payload_at(state_.manifest_, state_.declarations_, *declaration_it))};
            if (owned_types.empty()) {
                return std::unexpected{
                    SchemaEditError{"Declaration is missing from the type graph"}};
            }
            if (target.has_value() &&
                ((state_.types_.definition(*target)).kind() ==
                 lispb::schema::TypeDefinitionKind::soa) &&
                !(*this).declaration_handle(state_.rename_declaration_edits_.declarations[index],
                                            lispb::codegen::DeclarationKind::soa)) {
                return std::unexpected{
                    SchemaEditError{"Only struct SoA declarations can be renamed"}};
            }
            for (auto const& [registered_name_id, cpp_type] : state_.manifest_.types.entries) {
                auto const registered_name{std::string{
                    state_.manifest_.types.cpp_types.strings().view(registered_name_id)}};
                static_cast<void>(cpp_type);
                auto const registered{state_.types_.find_registered(registered_name_id)};
                if (registered.has_value() &&
                    std::ranges::find(owned_types, *registered) != owned_types.end()) {
                    return std::unexpected{SchemaEditError{
                        "Renaming a declaration registered as '@" + registered_name +
                        "' requires source-aware types-registry editing"}};
                }
            }

            auto const old_name{
                state_.declarations_.identity[state_.declarations_.index(*declaration_it)].name};
            auto const previous_manifest{state_.manifest_};
            auto const previous_declarations{state_.declarations_};
            auto const target_settings{store.module_settings(
                state_.manifest_
                    .modules[state_.declarations_
                                 .module_index[state_.declarations_.index(*declaration_it)]])};
            try {
                for (auto const owned : owned_types) {
                    auto const& node{owned};
                    auto const generated_name{
                        state_.types_.name(node) != old_name
                            ? strings.intern("F" + text(store, new_name) +
                                             text(store, state_.types_.name(node))
                                                 .substr(1 + strings.view(old_name).size()))
                            : new_name};
                    auto const spelling{
                        store.module_settings_store
                                .namespace_name[store.module_settings_store.index(target_settings)]
                            ? strings.intern(
                                  text(store,
                                       *store.module_settings_store
                                            .namespace_name[store.module_settings_store.index(
                                                target_settings)]) +
                                  "::" + text(store, generated_name))
                            : generated_name};
                    repair_semantic_references(
                        state_.manifest_,
                        state_.types_,
                        state_.declarations_,
                        owned,
                        spelling,
                        state_.declarations_
                            .module_index[state_.declarations_.index(*declaration_it)],
                        old_name,
                        LocalSoaReferencePolicy::rename,
                        generated_name,
                        state_.declarations_
                            .module_index[state_.declarations_.index(*declaration_it)]);
                }
                auto const renamed{[&] {
                    auto const value{store.clone(
                        payload_at(state_.manifest_, state_.declarations_, *declaration_it))};
                    switch (value.kind()) {
                        case codegen::DeclarationKind::enumeration:
                            store.enums.name[store.enums.index(value)] = new_name;
                            break;
                        case codegen::DeclarationKind::facade:
                            store.facades.name[store.facades.index(value)] = new_name;
                            break;
                        case codegen::DeclarationKind::fixed_point:
                            store.fixed_points.name[store.fixed_points.index(value)] = new_name;
                            break;
                        case codegen::DeclarationKind::homogeneous_layout:
                            store.homogeneous_layouts.name[store.homogeneous_layouts.index(value)] =
                                new_name;
                            break;
                        case codegen::DeclarationKind::integer_scalar:
                            store.integer_scalars.name[store.integer_scalars.index(value)] =
                                new_name;
                            break;
                        case codegen::DeclarationKind::integer_varint:
                            store.integer_varints.name[store.integer_varints.index(value)] =
                                new_name;
                            break;
                        case codegen::DeclarationKind::linear_quantized:
                            store.linear_quantizeds.name[store.linear_quantizeds.index(value)] =
                                new_name;
                            break;
                        case codegen::DeclarationKind::mini_float:
                            store.mini_floats.name[store.mini_floats.index(value)] = new_name;
                            break;
                        case codegen::DeclarationKind::optional_presence_bit:
                            store.optional_presence_bits
                                .name[store.optional_presence_bits.index(value)] = new_name;
                            break;
                        case codegen::DeclarationKind::optional_sentinel:
                            store.optional_sentinels.name[store.optional_sentinels.index(value)] =
                                new_name;
                            break;
                        case codegen::DeclarationKind::packed_value:
                            store.packed_values.name[store.packed_values.index(value)] = new_name;
                            break;
                        case codegen::DeclarationKind::record:
                            store.records.name[store.records.index(value)] = new_name;
                            break;
                        case codegen::DeclarationKind::soa:
                            store.soas.name[store.soas.index(value)] = new_name;
                            break;
                        case codegen::DeclarationKind::static_table:
                            store.static_tables.name[store.static_tables.index(value)] = new_name;
                            break;
                        case codegen::DeclarationKind::tagged_union:
                            store.tagged_unions.name[store.tagged_unions.index(value)] = new_name;
                            break;
                        case codegen::DeclarationKind::union_type:
                            store.unions.name[store.unions.index(value)] = new_name;
                            break;
                        case codegen::DeclarationKind::vector_soa:
                            store.vector_soas.name[store.vector_soas.index(value)] = new_name;
                            break;
                        default:
                            std::unreachable();
                    }
                    return value;
                }()};
                set_payload(state_.manifest_, state_.declarations_, *declaration_it, renamed);
                state_.declarations_.identity[state_.declarations_.index(*declaration_it)].name =
                    new_name;
                auto resolved{resolve_type_graph(state_.manifest_)};
                auto old_identity{
                    state_.declarations_.identity[state_.declarations_.index(*declaration_it)]};
                old_identity.name = old_name;
                validate_relocated_references(
                    state_.manifest_,
                    state_.types_,
                    resolved,
                    payload_at(previous_manifest, previous_declarations, *declaration_it),
                    payload_at(state_.manifest_, state_.declarations_, *declaration_it),
                    old_identity,
                    state_.declarations_.identity[state_.declarations_.index(*declaration_it)]);
                state_.types_ = std::move(resolved);
            } catch (std::exception const& error) {
                state_.manifest_ = previous_manifest;
                state_.declarations_ = previous_declarations;
                return std::unexpected{SchemaEditError{error.what()}};
            }
            return record_inverse
                     ? rename_declaration(state_.rename_declaration_edits_.declarations[index],
                                          strings.view(old_name))
                     : command;
        }
    }
    std::unreachable();
}

auto EditableSchemaDocument::read_state(CompilerStorage& storage,
                                        std::filesystem::path const& types_path,
                                        std::span<std::filesystem::path const> const module_paths)
    -> EditableDocumentState {
    auto registry{codegen::load_type_registry(storage, types_path)};
    codegen::Manifest manifest{
        codegen::manifest_schema_version, registry.types, storage.declarations, {}};
    auto sources{std::move(registry.sources)};

    std::vector<std::optional<SourceRange>> declaration_ranges;
    std::vector<std::optional<SourceRange>> module_ranges;
    auto module_index{std::size_t{}};
    for (auto const& path : module_paths) {
        auto source{codegen::load_module_source(storage, path)};
        auto const forms{storage.syntax.forms(source.forms)};
        sources.push_back(source.file);
        for (auto& module : source.modules) {
            manifest.modules.push_back(std::move(module));
        }
        for (auto const& form : forms) {
            if (module_index >= manifest.modules.size()) {
                throw std::logic_error{"Source contains more modules than the loaded manifest"};
            }
            auto const& module{manifest.modules[module_index++]};
            module_ranges.push_back(form_range(form));
            auto const declarations{declaration_ids(manifest, module_index - 1)};
            auto const expected_count{declarations.size()};
            if (expected_count == 0) {
                continue;
            }
            if (module.kind() == codegen::ModuleKind::normal) {

                auto declaration_index{std::size_t{}};
                for (auto const& child : storage.syntax.children(form)) {
                    if (declaration_index == declarations.size()) {
                        break;
                    }
                    auto const expected{declarations[declaration_index]};
                    if (storage.syntax.head(child) != codegen::declaration_head(expected)) {
                        continue;
                    }
                    if (storage.syntax.children(child).size() < 2 ||
                        storage.syntax.text(storage.syntax.children(child)[1]) !=
                            storage.strings.view(
                                codegen::declaration_name(storage.declarations, expected))) {
                        throw std::logic_error{"Source declaration does not match loaded module"};
                    }
                    declaration_ranges.push_back(form_range(child, expected));
                    ++declaration_index;
                }
                if (declaration_index != expected_count) {
                    throw std::logic_error{"Source declaration count does not match loaded module"};
                }
                continue;
            }
            throw std::logic_error{"Noncanonical module has declarations"};
        }
    }
    if (module_index != manifest.modules.size()) {
        throw std::logic_error{"Source contains fewer modules than the loaded manifest"};
    }

    EditableDocumentState result{std::move(manifest)};
    result.types_ = resolve_type_graph(result.manifest_);
    result.source_files_ = std::move(sources);
    result.types_path_ = types_path;
    result.module_paths_.assign(module_paths.begin(), module_paths.end());
    result.module_source_ranges_ = std::move(module_ranges);
    initialize_declarations(result, std::move(declaration_ranges));
    for (auto const& [name, range] : registry.declarations) {
        result.registered_type_sources_.emplace(name, SourceRange{range, {}});
    }
    return result;
}

auto load_editable_schema_document(std::filesystem::path const& types_path,
                                   std::span<std::filesystem::path const> const module_paths)
    -> EditableSchemaDocument {
    auto storage{std::make_unique<CompilerStorage>()};
    auto state{EditableSchemaDocument::read_state(*storage, types_path, module_paths)};
    return EditableSchemaDocument{std::move(storage), std::move(state)};
}

} // namespace lispb::schema
