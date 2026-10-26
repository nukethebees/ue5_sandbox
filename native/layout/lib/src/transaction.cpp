#include <ioj/layout/transaction.hpp>

#include <ioj/layout/document_state.hpp>
#include <ioj/layout/editor_session.hpp>

#include <codegen/ast/cpp_types.h>
#include <codegen/schema/declaration_schema.h>
#include <codegen/schema/declarations.h>
#include <codegen/source_loader.h>
#include <lispb/compiler_storage.h>

#include <algorithm>
#include <cassert>
#include <charconv>
#include <set>
#include <stdexcept>
#include <type_traits>

namespace ioj::layout {
namespace {
using namespace lispb::codegen;

auto module_index(Manifest const& manifest, std::string_view name) -> std::size_t {
    auto const& store{manifest.declarations};
    auto const count{manifest.modules.size()};
    for (std::size_t index{}; index < count; ++index) {
        auto const settings{store.module_settings(manifest.modules[index])};
        if (store.strings().view(
                store.module_settings_store.name[store.module_settings_store.index(settings)]) ==
            name) {
            if (manifest.modules[index].kind() != ModuleKind::normal) {
                throw std::invalid_argument{"Expected a normal module"};
            }
            return index;
        }
    }
    throw std::invalid_argument{"Unknown module '" + std::string{name} + "'"};
}
auto declaration_index(Declarations const& store,
                       std::span<DeclarationHandle const> values,
                       std::string_view name) -> std::size_t {
    auto const count{values.size()};
    for (std::size_t index{}; index < count; ++index) {
        if (store.strings().view(declaration_name(store, values[index])) == name) {
            return index;
        }
    }
    throw std::invalid_argument{"Unknown declaration '" + std::string{name} + "'"};
}
auto declarations_of(Declarations const& store, ModuleHandle module)
    -> std::vector<DeclarationHandle> {
    auto const values{
        store.items(store.normal_modules.declarations[store.normal_modules.index(module)])};
    return {values.begin(), values.end()};
}
void publish_module(DocumentState& state,
                    Manifest& manifest,
                    std::size_t index,
                    std::vector<DeclarationHandle> const& values) {
    auto& store{state.storage().declarations};
    auto const original{manifest.modules[index]};
    auto const replacement{store.normal_modules.clone(original)};
    store.normal_modules.declarations[store.normal_modules.index(replacement)] =
        store.range(values);
    state.inherit_module(original, replacement);
    manifest.modules[index] = replacement;
}
auto new_type(Declarations& store, std::string_view name) -> TypeRefId {
    auto const result{store.type_refs.append()};
    store.type_refs.name[store.type_refs.index(result)] = store.strings().intern(name);
    return result;
}
auto renamed(Declarations& store, DeclarationHandle original, std::string_view name)
    -> DeclarationHandle {
    auto const result{store.clone(original)};
    auto const value{store.strings().intern(name)};
    auto const assign{
        [&value, &result](auto& schemas) { schemas.name[schemas.index(result)] = value; }};

    switch (result.kind()) {
        case DeclarationKind::enumeration:
            assign(store.enums);
            break;
        case DeclarationKind::integer_scalar:
            assign(store.integer_scalars);
            break;
        case DeclarationKind::linear_quantized:
            assign(store.linear_quantizeds);
            break;
        case DeclarationKind::integer_varint:
            assign(store.integer_varints);
            break;
        case DeclarationKind::fixed_point:
            assign(store.fixed_points);
            break;
        case DeclarationKind::mini_float:
            assign(store.mini_floats);
            break;
        case DeclarationKind::optional_sentinel:
            assign(store.optional_sentinels);
            break;
        case DeclarationKind::optional_presence_bit:
            assign(store.optional_presence_bits);
            break;
        case DeclarationKind::record:
            assign(store.records);
            break;
        case DeclarationKind::union_type:
            assign(store.unions);
            break;
        case DeclarationKind::soa:
            assign(store.soas);
            break;
        case DeclarationKind::static_table:
            assign(store.static_tables);
            break;
        case DeclarationKind::facade:
            assign(store.facades);
            break;
    }
    return result;
}
auto namespace_name(Declarations const& store, ModuleHandle module) -> std::string {
    auto const settings{store.module_settings(module)};
    auto const value{
        store.module_settings_store.namespace_name[store.module_settings_store.index(settings)]};
    return value ? std::string{store.strings().view(*value)} : std::string{};
}
auto module_name(Declarations const& store, ModuleHandle module) -> std::string {
    auto const settings{store.module_settings(module)};
    return std::string{store.strings().view(
        store.module_settings_store.name[store.module_settings_store.index(settings)])};
}

auto logical_reference(std::string_view spelling,
                       std::string_view current_module,
                       std::string_view target_module,
                       std::string_view target_name) -> bool {
    return spelling == "@" + std::string{target_module} + "/" + std::string{target_name} ||
           (current_module == target_module && spelling == "@" + std::string{target_name});
}
auto qualified(std::string_view space, std::string_view name) -> std::string {
    return space.empty() ? std::string{name} : std::string{space} + "::" + std::string{name};
}
auto refers_to(Manifest const& manifest,
               std::string_view current_namespace,
               std::string_view spelling,
               std::string_view target_namespace,
               std::string_view target_name) -> bool {
    if (spelling == "::" + qualified(target_namespace, target_name) ||
        (!target_namespace.empty() && spelling == qualified(target_namespace, target_name))) {
        return true;
    }
    if (spelling != target_name) {
        return false;
    }
    if (current_namespace == target_namespace) {
        return true;
    }
    if (!target_namespace.empty()) {
        return false;
    }

    // An unqualified global name is visible only when this namespace does not shadow it.
    auto const& store{manifest.declarations};
    for (auto const module : manifest.modules) {
        if (module.kind() != ModuleKind::normal ||
            namespace_name(store, module) != current_namespace) {
            continue;
        }
        for (auto const declaration : declarations_of(store, module)) {
            if (store.strings().view(declaration_name(store, declaration)) == target_name) {
                return false;
            }
            for (auto const name : generated_cpp_names(store, declaration)) {
                if (store.strings().view(name) == target_name) {
                    return false;
                }
            }
        }
    }
    return true;
}

auto references_declaration(Manifest const& manifest,
                            ModuleHandle current,
                            TypeRefId reference,
                            ModuleHandle target,
                            DeclarationHandle declaration) -> bool {
    auto const& store{manifest.declarations};
    auto spelling{store.strings().view(store.type_refs.name[store.type_refs.index(reference)])};
    auto const name{store.strings().view(declaration_name(store, declaration))};
    if (logical_reference(
            spelling, module_name(store, current), module_name(store, target), name)) {
        return true;
    }
    if (spelling.starts_with('@')) {
        if (auto const registered{manifest.types.find(spelling.substr(1))}) {
            spelling = manifest.types.cpp_types.spelling(
                manifest.types.registered.cpp_type[manifest.types.registered.index(*registered)]);
        }
    }
    return refers_to(
        manifest, namespace_name(store, current), spelling, namespace_name(store, target), name);
}

struct ReferencedDeclaration {
    ModuleHandle module;
    DeclarationHandle declaration;
};
auto referenced_declaration(Manifest const& manifest, ModuleHandle module, TypeRefId reference)
    -> std::optional<ReferencedDeclaration> {
    for (auto const target_module : manifest.modules) {
        if (target_module.kind() != ModuleKind::normal) {
            continue;
        }
        for (auto const target : declarations_of(manifest.declarations, target_module)) {
            if (references_declaration(manifest, module, reference, target_module, target)) {
                return ReferencedDeclaration{target_module, target};
            }
        }
    }
    return {};
}
auto packed_declaration(Declarations const& store, DeclarationHandle declaration) -> bool {
    return (declaration.kind() == DeclarationKind::record &&
            store.records.layout[declaration] == LayoutKind::packed) ||
           (declaration.kind() == DeclarationKind::union_type &&
            store.unions.layout[declaration] == LayoutKind::packed);
}

void repair_member_metadata(DocumentState& state,
                            Manifest const& manifest,
                            DeclarationHandle changed,
                            std::string_view before,
                            std::optional<std::string_view> after) {
    auto& store{manifest.declarations};
    auto& strings{store.strings()};
    auto const& children{store.child_occurrences};
    auto update = [&](auto owner,
                      std::string const& path,
                      std::string const& renamed,
                      std::string const& source,
                      std::string const& renamed_source) {
        if (after) {
            state.analysis_settings.rename_member(owner, path, renamed);
            state.variants.rename_member(strings,
                                         owner,
                                         strings.intern_retained(source),
                                         strings.intern_retained(renamed_source));
        } else {
            state.analysis_settings.erase_member(owner, path);
            state.variants.erase_member(strings, owner, strings.intern_retained(source));
        }
    };
    auto const join = [](std::string const& prefix, std::string_view name, char separator) {
        return prefix.empty() ? std::string{name} : prefix + separator + std::string{name};
    };
    for (auto const module : manifest.modules) {
        if (module.kind() != ModuleKind::normal) {
            continue;
        }
        for (auto const root : declarations_of(store, module)) {
            auto const owner{state.declaration_ids.at(root)};
            if (!packed_declaration(store, root)) {
                if (root == changed) {
                    update(owner,
                           std::string{before},
                           std::string{after.value_or(before)},
                           std::string{before},
                           std::string{after.value_or(before)});
                }
                continue;
            }
            std::vector<DeclarationHandle> active;
            auto walk = [&](auto&& self,
                            ReferencedDeclaration current,
                            std::string path,
                            std::string renamed_path,
                            std::string source,
                            std::string renamed_source,
                            bool affected) -> void {
                if (std::ranges::find(active, current.declaration) != active.end()) {
                    return;
                }
                auto const kind{current.declaration.kind()};
                if (kind != DeclarationKind::record && kind != DeclarationKind::union_type) {
                    return;
                }
                active.push_back(current.declaration);
                auto const range{kind == DeclarationKind::record
                                     ? store.records.members[current.declaration]
                                     : store.unions.alternatives[current.declaration]};
                for (auto const child : store.items(range)) {
                    auto const name{std::string{strings.view(children.name[child])}};
                    auto const matches{current.declaration == changed && name == before};
                    auto const renamed{matches && after ? std::string{*after} : name};
                    auto const child_source{join(source, name, '.')};
                    auto const renamed_child_source{join(renamed_source, renamed, '.')};
                    std::set<std::uint64_t> elements{0};
                    if (children.count[child]) {
                        auto const accesses{state.analysis_settings.access_sets.find(owner)};
                        auto const prefix{join(path, soa_namespace_name(name), '_') + "_"};
                        if (accesses != state.analysis_settings.access_sets.end()) {
                            for (auto const& [key, operation] : accesses->second) {
                                if (!key.starts_with(prefix)) {
                                    continue;
                                }
                                std::uint64_t element{};
                                auto const end{key.data() + key.size()};
                                auto const parsed{
                                    std::from_chars(key.data() + prefix.size(), end, element)};
                                if (parsed.ec == std::errc{} &&
                                    (parsed.ptr == end || *parsed.ptr == '_') &&
                                    element < *children.count[child]) {
                                    elements.insert(element);
                                }
                            }
                        }
                    }
                    for (auto const element : elements) {
                        auto const suffix{children.count[child] ? "_" + std::to_string(element)
                                                                : ""};
                        auto const child_path{join(path, soa_namespace_name(name), '_') + suffix};
                        auto const renamed_child_path{
                            join(renamed_path, soa_namespace_name(renamed), '_') + suffix};
                        if (affected || matches) {
                            update(owner,
                                   child_path,
                                   renamed_child_path,
                                   child_source,
                                   renamed_child_source);
                        }
                        if (children.kind[child] != ChildOccurrenceKind::reserved) {
                            if (auto const target{referenced_declaration(
                                    manifest, current.module, children.type[child])}) {
                                self(self,
                                     *target,
                                     child_path,
                                     renamed_child_path,
                                     child_source,
                                     renamed_child_source,
                                     affected || matches);
                            }
                        }
                    }
                }
                active.pop_back();
            };
            walk(walk, {module, root}, {}, {}, {}, {}, false);
        }
    }
}

void rename_discriminator_case(DocumentState& state,
                               Manifest& manifest,
                               ModuleHandle enum_module,
                               DeclarationHandle enumeration,
                               std::string_view before,
                               std::string_view after) {
    auto& store{manifest.declarations};
    auto& children{store.child_occurrences};
    std::vector<DeclarationHandle> mapped_unions;
    for (auto const module : manifest.modules) {
        if (module.kind() != ModuleKind::normal) {
            continue;
        }
        for (auto const declaration : declarations_of(store, module)) {
            if (declaration.kind() != DeclarationKind::record) {
                continue;
            }
            auto const members{
                store.items(store.records.members[store.records.index(declaration)])};
            for (auto const member : members) {
                auto const row{children.index(member)};
                auto const tag{children.tag[row]};
                if (!tag) {
                    continue;
                }
                auto const discriminator{std::ranges::find_if(
                    members, [&](auto id) { return children.name[children.index(id)] == *tag; })};
                if (discriminator == members.end() ||
                    !references_declaration(manifest,
                                            module,
                                            children.type[children.index(*discriminator)],
                                            enum_module,
                                            enumeration)) {
                    continue;
                }
                for (auto const target_module : manifest.modules) {
                    if (target_module.kind() != ModuleKind::normal) {
                        continue;
                    }
                    for (auto const target : declarations_of(store, target_module)) {
                        if (target.kind() == DeclarationKind::union_type &&
                            references_declaration(
                                manifest, module, children.type[row], target_module, target)) {
                            mapped_unions.push_back(target);
                        }
                    }
                }
            }
        }
    }
    auto const module_count{manifest.modules.size()};
    for (std::size_t index{}; index < module_count; ++index) {
        auto const module{manifest.modules[index]};
        if (module.kind() != ModuleKind::normal) {
            continue;
        }
        auto decls{declarations_of(store, module)};
        bool changed{};
        for (auto& declaration : decls) {
            if (std::ranges::find(mapped_unions, declaration) == mapped_unions.end()) {
                continue;
            }
            auto const range{
                store.items(store.unions.alternatives[store.unions.index(declaration)])};
            std::vector<ChildOccurrenceId> alternatives{range.begin(), range.end()};
            bool renamed_case{};
            for (auto& alternative : alternatives) {
                auto const row{children.index(alternative)};
                auto label{children.case_label[row]};
                if (!label) {
                    if (auto const target{
                            referenced_declaration(manifest, module, children.type[row])}) {
                        label = declaration_name(store, target->declaration);
                    } else {
                        auto name{store.strings().view(store.type_refs.name[children.type[row]])};
                        if (name.starts_with('@')) {
                            name.remove_prefix(1);
                        }
                        label = store.strings().intern(name);
                    }
                }
                if (store.strings().view(*label) == before) {
                    alternative = children.clone(alternative);
                    children.case_label[children.index(alternative)] =
                        store.strings().intern(after);
                    renamed_case = true;
                }
            }
            if (renamed_case) {
                auto const previous{declaration};
                declaration = store.unions.clone(previous);
                store.unions.alternatives[store.unions.index(declaration)] =
                    store.range(alternatives);
                state.inherit_declaration(previous, declaration);
                changed = true;
            }
        }
        if (changed) {
            publish_module(state, manifest, index, decls);
        }
    }
}
auto qualify_moved_references(Manifest const& manifest,
                              DeclarationHandle declaration,
                              ModuleHandle origin,
                              ModuleHandle destination) -> DeclarationHandle {
    auto& store{manifest.declarations};
    auto const space{namespace_name(store, origin)};
    auto const origin_name{module_name(store, origin)};
    std::vector<std::string> local_names;
    for (auto const module : manifest.modules) {
        if (module.kind() != ModuleKind::normal || namespace_name(store, module) != space) {
            continue;
        }
        for (auto const item : declarations_of(store, module)) {
            local_names.emplace_back(store.strings().view(declaration_name(store, item)));
            for (auto const name : generated_cpp_names(store, item)) {
                local_names.emplace_back(store.strings().view(name));
            }
        }
    }
    return rewrite_type_references(store, declaration, [&](auto const&, TypeRefId reference, auto) {
        auto const name{std::string{
            store.strings().view(store.type_refs.name[store.type_refs.index(reference)])}};
        auto const local_logical{
            name.starts_with('@') &&
            std::ranges::any_of(declarations_of(store, origin), [&](auto item) {
                return name.substr(1) == store.strings().view(declaration_name(store, item));
            })};
        if (!local_logical && (space == namespace_name(store, destination) ||
                               std::ranges::find(local_names, name) == local_names.end())) {
            return reference;
        }
        auto const result{store.type_refs.clone(reference)};
        store.type_refs.name[store.type_refs.index(result)] = store.strings().intern(
            local_logical ? "@" + origin_name + "/" + name.substr(1) : qualified(space, name));
        return result;
    });
}
void repair_references(DocumentState& state,
                       Manifest& manifest,
                       DeclarationHandle before,
                       ModuleHandle old_module,
                       DeclarationHandle after,
                       ModuleHandle new_module) {
    auto& store{state.storage().declarations};
    auto const old_namespace{namespace_name(store, old_module)};
    auto const new_namespace{namespace_name(store, new_module)};
    auto const old_module_name{module_name(store, old_module)};
    auto const new_module_name{module_name(store, new_module)};
    auto old_names{generated_cpp_names(store, before)};
    auto new_names{generated_cpp_names(store, after)};
    if (before.kind() != after.kind() || old_names.size() != new_names.size()) {
        old_names.clear();
        new_names.clear();
    }
    old_names.push_back(declaration_name(store, before));
    new_names.push_back(declaration_name(store, after));
    auto const module_count{manifest.modules.size()};
    for (std::size_t index{}; index < module_count; ++index) {
        auto const module{manifest.modules[index]};
        if (module.kind() != ModuleKind::normal) {
            continue;
        }
        auto values{declarations_of(store, module)};
        auto const current_namespace{namespace_name(store, module)};
        bool changed{};
        for (auto& declaration : values) {
            auto const previous{declaration};
            declaration = rewrite_type_references(
                store, declaration, [&](auto const&, TypeRefId reference, auto) {
                    auto const name{std::string{store.strings().view(
                        store.type_refs.name[store.type_refs.index(reference)])}};
                    if (logical_reference(name,
                                          module_name(store, module),
                                          old_module_name,
                                          store.strings().view(declaration_name(store, before)))) {
                        auto const replacement{store.type_refs.clone(reference)};
                        auto const new_name{
                            std::string{store.strings().view(declaration_name(store, after))}};
                        store.type_refs.name[store.type_refs.index(replacement)] =
                            store.strings().intern(module_name(store, module) == new_module_name
                                                       ? "@" + new_name
                                                       : "@" + new_module_name + "/" + new_name);
                        return replacement;
                    }
                    auto const count{old_names.size()};
                    for (std::size_t entry{}; entry < count; ++entry) {
                        auto const old_name{std::string{store.strings().view(old_names[entry])}};
                        if (refers_to(manifest, current_namespace, name, old_namespace, old_name)) {
                            auto const replacement{store.type_refs.clone(reference)};
                            auto const new_name{
                                std::string{store.strings().view(new_names[entry])}};
                            store.type_refs.name[store.type_refs.index(replacement)] =
                                store.strings().intern(current_namespace == new_namespace
                                                           ? new_name
                                                           : qualified(new_namespace, new_name));
                            return replacement;
                        }
                    }
                    return reference;
                });
            if (declaration != previous) {
                state.inherit_declaration(previous, declaration);
                changed = true;
            }
        }
        if (changed) {
            publish_module(state, manifest, index, values);
        }
    }
}
template <typename Id>
void reorder(std::vector<Id>& values, std::size_t from, std::size_t to) {
    if (to >= values.size()) {
        throw std::invalid_argument{"Child position is outside the declaration"};
    }
    auto const value{values[from]};
    values.erase(values.begin() + static_cast<std::ptrdiff_t>(from));
    values.insert(values.begin() + static_cast<std::ptrdiff_t>(to), value);
}
}

Transaction::Transaction(EditorSession& session, DocumentId owner)
    : session_{&session}
    , owner_{owner} {}
void Transaction::append(TransactionKind kind,
                         std::string_view module,
                         std::string_view declaration,
                         std::string_view member,
                         std::string_view text,
                         std::optional<std::uint64_t> number) {
    auto& state{session_->state(owner_)};
    auto& strings{session_->command_strings()};
    kinds_.push_back(kind);
    ids_.push_back(state.next_instruction_id);
    ++state.next_instruction_id.value;
    modules_.push_back(strings.intern(module));
    declarations_.push_back(strings.intern(declaration));
    members_.push_back(strings.intern(member));
    texts_.push_back(strings.intern(text));
    numbers_.push_back(number);
    variant_ids_.emplace_back();
    payload_offsets_.push_back(payload_names_.size());
    payload_counts_.push_back(0);
}
void Transaction::set_member_bits(std::string_view m,
                                  std::string_view d,
                                  std::string_view f,
                                  int bits) {
    append(TransactionKind::member_bits, m, d, f, {}, static_cast<std::uint64_t>(bits));
}
void
    Transaction::set_packed_storage(std::string_view m, std::string_view d, std::string_view type) {
    append(TransactionKind::packed_storage, m, d, {}, type);
}
void Transaction::set_member_type(std::string_view m,
                                  std::string_view d,
                                  std::string_view member,
                                  std::string_view type) {
    append(TransactionKind::member_type, m, d, member, type);
}
void Transaction::create_declaration(std::string_view m, std::string_view source) {
    append(TransactionKind::create, m, {}, {}, source);
}
void Transaction::replace_declaration(std::string_view m,
                                      std::string_view n,
                                      std::string_view source) {
    append(TransactionKind::replace, m, n, {}, source);
}
void Transaction::delete_declaration(std::string_view m, std::string_view n) {
    append(TransactionKind::erase, m, n, {}, {});
}
void
    Transaction::rename_declaration(std::string_view m, std::string_view n, std::string_view name) {
    append(TransactionKind::rename, m, n, {}, name);
}
void Transaction::duplicate_declaration(std::string_view m,
                                        std::string_view n,
                                        std::string_view name) {
    append(TransactionKind::duplicate, m, n, {}, name);
}
void Transaction::move_declaration(std::string_view m,
                                   std::string_view n,
                                   std::string_view destination) {
    append(TransactionKind::move, m, n, {}, destination);
}
void Transaction::create_module(std::string_view source, std::string_view path) {
    append(TransactionKind::create_module, {}, {}, path, source);
}
void Transaction::delete_module(std::string_view m) {
    append(TransactionKind::delete_module, m, {}, {}, {});
}
void Transaction::rename_module(std::string_view m, std::string_view name) {
    append(TransactionKind::rename_module, m, {}, {}, name);
}
void Transaction::add_member(std::string_view m,
                             std::string_view d,
                             std::string_view n,
                             std::string_view type) {
    append(TransactionKind::add_member, m, d, n, type);
}
void Transaction::delete_member(std::string_view m, std::string_view d, std::string_view n) {
    append(TransactionKind::delete_member, m, d, n, {});
}
void Transaction::rename_member(std::string_view m,
                                std::string_view d,
                                std::string_view n,
                                std::string_view name) {
    append(TransactionKind::rename_member, m, d, n, name);
}
void Transaction::reorder_member(std::string_view m,
                                 std::string_view d,
                                 std::string_view n,
                                 std::size_t position) {
    append(TransactionKind::reorder_member, m, d, n, {}, position);
}
void Transaction::set_member_count(std::string_view m,
                                   std::string_view d,
                                   std::string_view n,
                                   std::optional<std::uint64_t> count) {
    append(TransactionKind::member_count, m, d, n, {}, count);
}
void Transaction::add_enumerator(std::string_view m,
                                 std::string_view d,
                                 std::string_view n,
                                 std::string_view initializer) {
    append(TransactionKind::add_enumerator, m, d, n, initializer);
}
void Transaction::delete_enumerator(std::string_view m, std::string_view d, std::string_view n) {
    append(TransactionKind::delete_enumerator, m, d, n, {});
}
void Transaction::rename_enumerator(std::string_view m,
                                    std::string_view d,
                                    std::string_view n,
                                    std::string_view name) {
    append(TransactionKind::rename_enumerator, m, d, n, name);
}
void Transaction::set_enumerator_initializer(std::string_view m,
                                             std::string_view d,
                                             std::string_view n,
                                             std::string_view initializer) {
    append(TransactionKind::enumerator_initializer, m, d, n, initializer);
}
void Transaction::reorder_enumerator(std::string_view m,
                                     std::string_view d,
                                     std::string_view n,
                                     std::size_t position) {
    append(TransactionKind::reorder_enumerator, m, d, n, {}, position);
}
void Transaction::add_source(std::string_view path, std::string_view source) {
    append(TransactionKind::source_add, path, {}, {}, source);
}
void Transaction::replace_source(std::string_view path, std::string_view source) {
    append(TransactionKind::source_replace, path, {}, {}, source);
}
void Transaction::remove_source(std::string_view path) {
    append(TransactionKind::source_remove, path, {}, {}, {});
}
void Transaction::rename_source(std::string_view path, std::string_view destination) {
    append(TransactionKind::source_rename, path, {}, {}, destination);
}
void Transaction::create_variant(std::string_view name) {
    append(TransactionKind::variant_create, {}, {}, {}, name);
    numbers_.back() = session_->state(owner_).next_variant_id.value++;
}
void Transaction::duplicate_variant(VariantId id, std::string_view name) {
    append(TransactionKind::variant_duplicate, {}, {}, {}, name);
    variant_ids_.back() = id;
    numbers_.back() = session_->state(owner_).next_variant_id.value++;
}
void Transaction::rename_variant(VariantId id, std::string_view name) {
    append(TransactionKind::variant_rename, {}, {}, {}, name);
    variant_ids_.back() = id;
}
void Transaction::delete_variant(VariantId id) {
    append(TransactionKind::variant_delete, {}, {}, {}, {});
    variant_ids_.back() = id;
}
void Transaction::reset_variant(VariantId id) {
    append(TransactionKind::variant_reset, {}, {}, {}, {});
    variant_ids_.back() = id;
}
void Transaction::set_variant_packed_storage(VariantId id,
                                             std::string_view m,
                                             std::string_view d,
                                             std::optional<std::string> const& type) {
    append(TransactionKind::variant_packed_storage, m, d, {}, type.value_or(""));
    variant_ids_.back() = id;
}
void Transaction::set_variant_field_bits(VariantId id,
                                         std::string_view m,
                                         std::string_view d,
                                         std::string_view field,
                                         std::optional<std::uint32_t> bits) {
    append(TransactionKind::variant_field_bits, m, d, field, {}, bits);
    variant_ids_.back() = id;
}
void Transaction::set_variant_member_type(VariantId id,
                                          std::string_view m,
                                          std::string_view d,
                                          std::string_view member,
                                          std::optional<std::string> const& type) {
    append(TransactionKind::variant_member_type, m, d, member, type.value_or(""));
    variant_ids_.back() = id;
}
void Transaction::set_variant_capacity(VariantId id,
                                       std::string_view m,
                                       std::string_view d,
                                       std::optional<std::uint64_t> capacity) {
    append(TransactionKind::variant_capacity, m, d, {}, {}, capacity);
    variant_ids_.back() = id;
}

void Transaction::set_element_count(std::uint64_t value) {
    append(TransactionKind::element_count, {}, {}, {}, {}, value);
}
void Transaction::set_default_soa_capacity(std::uint64_t value) {
    append(TransactionKind::default_soa_capacity, {}, {}, {}, {}, value);
}
void Transaction::set_access_operation(AccessOperation value) {
    append(TransactionKind::access_operation, {}, {}, {}, {}, static_cast<std::uint64_t>(value));
}
void Transaction::set_access_multiplicity(std::uint64_t value) {
    append(TransactionKind::access_multiplicity, {}, {}, {}, {}, value);
}
void Transaction::set_abi_profile(AbiProfile const& profile) {
    append(TransactionKind::abi_profile, {}, {}, {}, serialize_abi_profile(profile));
}
void Transaction::set_comparison_abi_profile(std::optional<AbiProfile> const& profile) {
    append(TransactionKind::comparison_abi_profile,
           {},
           {},
           {},
           profile ? serialize_abi_profile(*profile) : std::string{});
}
void Transaction::set_access_set(std::string_view module,
                                 std::string_view declaration,
                                 AccessSet const& accesses) {
    append(TransactionKind::access_set, module, declaration, {}, {});
    auto& strings{session_->command_strings()};
    for (auto const& [name, operation] : accesses) {
        payload_names_.push_back(strings.intern(name));
        payload_weights_.push_back(static_cast<std::uint64_t>(operation));
        payload_values_.emplace_back();
    }
    payload_counts_.back() = accesses.size();
}
void Transaction::set_varint_distribution(std::string_view module,
                                          std::string_view declaration,
                                          std::span<IntegerVarintDistributionEntry const> entries) {
    append(TransactionKind::varint_distribution, module, declaration, {}, {});
    for (auto const& entry : entries) {
        payload_names_.emplace_back();
        payload_weights_.push_back(entry.weight);
        payload_values_.push_back(entry.value);
    }
    payload_counts_.back() = entries.size();
}
void Transaction::set_union_distribution(std::string_view module,
                                         std::string_view declaration,
                                         std::span<UnionDistributionEntry const> entries) {
    append(TransactionKind::union_distribution, module, declaration, {}, {});
    auto& strings{session_->command_strings()};
    for (auto const& entry : entries) {
        payload_names_.push_back(strings.intern(entry.alternative_name));
        payload_weights_.push_back(entry.weight);
        payload_values_.emplace_back();
    }
    payload_counts_.back() = entries.size();
}

void Transaction::append(Transaction const& transaction) {
    assert(session_ == transaction.session_ && owner_ == transaction.owner_);
    auto const payload_start{payload_names_.size()};
    kinds_.insert(kinds_.end(), transaction.kinds_.begin(), transaction.kinds_.end());
    ids_.insert(ids_.end(), transaction.ids_.begin(), transaction.ids_.end());
    modules_.insert(modules_.end(), transaction.modules_.begin(), transaction.modules_.end());
    declarations_.insert(
        declarations_.end(), transaction.declarations_.begin(), transaction.declarations_.end());
    members_.insert(members_.end(), transaction.members_.begin(), transaction.members_.end());
    texts_.insert(texts_.end(), transaction.texts_.begin(), transaction.texts_.end());
    numbers_.insert(numbers_.end(), transaction.numbers_.begin(), transaction.numbers_.end());
    variant_ids_.insert(
        variant_ids_.end(), transaction.variant_ids_.begin(), transaction.variant_ids_.end());
    for (auto const offset : transaction.payload_offsets_) {
        payload_offsets_.push_back(payload_start + offset);
    }
    payload_counts_.insert(payload_counts_.end(),
                           transaction.payload_counts_.begin(),
                           transaction.payload_counts_.end());
    payload_names_.insert(
        payload_names_.end(), transaction.payload_names_.begin(), transaction.payload_names_.end());
    payload_weights_.insert(payload_weights_.end(),
                            transaction.payload_weights_.begin(),
                            transaction.payload_weights_.end());
    payload_values_.insert(payload_values_.end(),
                           transaction.payload_values_.begin(),
                           transaction.payload_values_.end());
}
void Transaction::truncate(std::size_t count) {
    auto const payload_count{count == 0 ? 0
                                        : payload_offsets_[count - 1] + payload_counts_[count - 1]};
    kinds_.resize(count);
    ids_.resize(count);
    modules_.resize(count);
    declarations_.resize(count);
    members_.resize(count);
    texts_.resize(count);
    numbers_.resize(count);
    variant_ids_.resize(count);
    payload_offsets_.resize(count);
    payload_counts_.resize(count);
    payload_names_.resize(payload_count);
    payload_weights_.resize(payload_count);
    payload_values_.resize(payload_count);
}

void Transaction::apply(DocumentState& state, Manifest& manifest, std::size_t end) const {
    auto& store{state.storage().declarations};
    auto& strings{state.storage().strings};
    std::vector<std::string> removed_names;
    std::vector<std::string> removed_namespaces;
    auto record_removed =
        [&](DeclarationHandle declaration, ModuleHandle module, bool erase_variants = true) {
            if (erase_variants) {
                state.variants.erase_declaration(state.declaration_ids.at(declaration));
                state.analysis_settings.erase_declaration(state.declaration_ids.at(declaration));
            }
            auto names{generated_cpp_names(store, declaration)};
            names.push_back(declaration_name(store, declaration));
            for (auto const name : names) {
                removed_names.emplace_back(strings.view(name));
                removed_namespaces.push_back(namespace_name(store, module));
            }
        };
    auto const& durable_strings{session_->command_strings()};
    auto const instruction_count{end};
    assert(end <= kinds_.size());
    for (std::size_t instruction{}; instruction < instruction_count; ++instruction) {
        session_->capture_diagnostics(state.id);
        state.current_instruction_id = ids_[instruction];
        state.creation_slot = 0;
        state.source_creation_slot = 0;
        session_->set_instruction(ids_[instruction]);
        auto const kind{kinds_[instruction]};
        auto const text{std::string{durable_strings.view(texts_[instruction])}};
        auto const child_name{std::string{durable_strings.view(members_[instruction])}};
        auto const number{numbers_[instruction]};

        if (kind >= TransactionKind::element_count) {
            auto& settings{state.analysis_settings};
            auto assign = [&](auto& destination, auto value) {
                state.changed |= destination != value;
                destination = value;
            };
            switch (kind) {
                case TransactionKind::element_count:
                    assign(settings.element_count, *number);
                    break;
                case TransactionKind::default_soa_capacity:
                    assign(settings.default_soa_capacity, *number);
                    break;
                case TransactionKind::access_operation:
                    assign(settings.access_operation, static_cast<AccessOperation>(*number));
                    break;
                case TransactionKind::access_multiplicity:
                    assign(settings.access_multiplicity, *number);
                    break;
                case TransactionKind::abi_profile:
                case TransactionKind::comparison_abi_profile: {
                    if (kind == TransactionKind::comparison_abi_profile && text.empty()) {
                        state.changed |= settings.comparison_abi.has_value();
                        settings.comparison_abi.reset();
                    } else {
                        auto profile{parse_abi_profile(text)};
                        if (!profile) {
                            throw std::invalid_argument{"Invalid ABI profile: " +
                                                        profile.error().message};
                        }
                        if (kind == TransactionKind::abi_profile) {
                            state.changed |= serialize_abi_profile(settings.abi) != text;
                            settings.abi = std::move(*profile);
                        } else {
                            state.changed |=
                                !settings.comparison_abi ||
                                serialize_abi_profile(*settings.comparison_abi) != text;
                            settings.comparison_abi = std::move(*profile);
                        }
                    }
                    break;
                }
                default: {
                    auto const module{
                        module_index(manifest, durable_strings.view(modules_[instruction]))};
                    auto const decls{declarations_of(store, manifest.modules[module])};
                    auto const declaration{decls[declaration_index(
                        store, decls, durable_strings.view(declarations_[instruction]))]};
                    auto const identity{state.declaration_ids.at(declaration)};
                    auto const offset{payload_offsets_[instruction]};
                    auto const count{payload_counts_[instruction]};
                    if (kind == TransactionKind::access_set) {
                        AccessSet accesses;
                        for (std::size_t entry{}; entry < count; ++entry) {
                            accesses.emplace(
                                std::string{durable_strings.view(payload_names_[offset + entry])},
                                static_cast<AccessOperation>(payload_weights_[offset + entry]));
                        }
                        state.changed |= settings.access_sets[identity] != accesses;
                        if (accesses.empty()) {
                            settings.access_sets.erase(identity);
                        } else {
                            settings.access_sets.insert_or_assign(identity, std::move(accesses));
                        }
                    } else if (kind == TransactionKind::varint_distribution) {
                        auto& entries{settings.varint_distributions[identity]};
                        entries.clear();
                        for (std::size_t entry{}; entry < count; ++entry) {
                            entries.push_back({payload_values_[offset + entry],
                                               payload_weights_[offset + entry]});
                        }
                        state.changed = true;
                    } else {
                        assert(kind == TransactionKind::union_distribution);
                        auto& entries{settings.union_distributions[identity]};
                        entries.clear();
                        for (std::size_t entry{}; entry < count; ++entry) {
                            entries.push_back(
                                {std::string{durable_strings.view(payload_names_[offset + entry])},
                                 payload_weights_[offset + entry]});
                        }
                        state.changed = true;
                    }
                    break;
                }
            }
            continue;
        }

        if (kind == TransactionKind::source_add || kind == TransactionKind::source_replace ||
            kind == TransactionKind::source_remove || kind == TransactionKind::source_rename) {
            auto const before{manifest.modules};
            state.edit_source(kind == TransactionKind::source_add       ? "add"
                              : kind == TransactionKind::source_replace ? "replace"
                              : kind == TransactionKind::source_remove  ? "remove"
                                                                        : "rename",
                              durable_strings.view(modules_[instruction]),
                              text,
                              manifest);
            if (kind == TransactionKind::source_remove || kind == TransactionKind::source_replace) {
                for (auto const module : before) {
                    if (module.kind() == ModuleKind::normal &&
                        std::ranges::find(manifest.modules, module) == manifest.modules.end()) {
                        for (auto const declaration : declarations_of(store, module)) {
                            record_removed(
                                declaration, module, kind == TransactionKind::source_remove);
                        }
                    }
                }
            }
            continue;
        }
        if (kind == TransactionKind::variant_create) {
            state.variants.create(VariantId{static_cast<std::uint32_t>(*number)}, text);
            state.changed = true;
            continue;
        }
        if (kind >= TransactionKind::variant_duplicate &&
            kind <= TransactionKind::variant_capacity) {
            state.changed = true;
            auto const id{variant_ids_[instruction]};
            if (!state.variants.contains(id) ||
                (id == VariantId{} && kind != TransactionKind::variant_duplicate)) {
                throw std::invalid_argument{"Expected an editable variant"};
            }
            if (kind == TransactionKind::variant_duplicate) {
                state.variants.duplicate(id, VariantId{static_cast<std::uint32_t>(*number)}, text);
                continue;
            }
            if (kind == TransactionKind::variant_rename) {
                state.variants.rename(id, text);
                continue;
            }
            if (kind == TransactionKind::variant_delete) {
                state.variants.erase(id);
                continue;
            }
            if (kind == TransactionKind::variant_reset) {
                state.variants.reset(id);
                continue;
            }
            auto const module{module_index(manifest, durable_strings.view(modules_[instruction]))};
            auto const values{declarations_of(store, manifest.modules[module])};
            auto const declaration{values[declaration_index(
                store, values, durable_strings.view(declarations_[instruction]))]};
            auto const identity{state.declaration_ids.at(declaration)};
            if (kind == TransactionKind::variant_packed_storage ||
                kind == TransactionKind::variant_field_bits) {
                if (!packed_declaration(store, declaration)) {
                    throw std::invalid_argument{"Expected a packed record or union declaration"};
                }
                if (kind == TransactionKind::variant_field_bits) {
                    state.variants.set_field_width(
                        id,
                        identity,
                        strings.intern_retained(child_name),
                        number ? std::optional{static_cast<std::uint32_t>(*number)} : std::nullopt);
                } else {
                    state.variants.set_packed_storage(
                        id, identity, text.empty() ? std::nullopt : std::optional{text});
                }
            } else {
                if (declaration.kind() != DeclarationKind::soa) {
                    throw std::invalid_argument{"Expected a soa declaration"};
                }
                if (kind == TransactionKind::variant_capacity) {
                    state.variants.set_capacity(id, identity, number);
                } else {
                    auto const members{
                        store.items(store.soas.members[store.soas.index(declaration)])};
                    if (!std::ranges::any_of(members, [&](auto member) {
                            return strings.view(
                                       store.soa_members.name[store.soa_members.index(member)]) ==
                                   child_name;
                        })) {
                        throw std::invalid_argument{"Unknown member '" + child_name + "'"};
                    }
                    state.variants.set_column_type(id,
                                                   identity,
                                                   strings.intern_retained(child_name),
                                                   text.empty() ? std::nullopt
                                                                : std::optional{text});
                }
            }
            continue;
        }

        if (kind == TransactionKind::create_module) {
            auto const path{std::filesystem::path{child_name}.lexically_normal()};
            auto const file{state.storage().sources.add(path.generic_string(), text)};
            auto const parsed{parse_module_source(state.storage(), file)};
            if (parsed.modules.size() != 1 || parsed.modules.front().kind() != ModuleKind::normal) {
                throw std::invalid_argument{"Expected exactly one normal module"};
            }
            manifest.modules.push_back(parsed.modules.front());
            auto const destination{std::ranges::find_if(state.source_order, [&](auto source) {
                return state.storage().sources.filesystem_path(source).lexically_normal() == path;
            })};
            auto const source{destination == state.source_order.end() ? file : *destination};
            state.source_files.emplace(parsed.modules.front(), source);
            if (destination == state.source_order.end()) {
                state.source_order.push_back(file);
                state.render_origins.emplace(file, file);
                state.source_ids.emplace(
                    file, SourceId{state.current_instruction_id, ++state.source_creation_slot});
            }
            state.comments.capture(state.storage(), parsed.modules, parsed.file);
            for (auto declaration : declarations_of(store, parsed.modules.front())) {
                state.register_declaration(declaration);
            }
            continue;
        }
        auto const module{module_index(manifest, durable_strings.view(modules_[instruction]))};
        auto const original_module{manifest.modules[module]};
        if (kind == TransactionKind::delete_module) {
            for (auto declaration : declarations_of(store, original_module)) {
                record_removed(declaration, original_module);
            }
            manifest.modules.erase(manifest.modules.begin() + static_cast<std::ptrdiff_t>(module));
            continue;
        }
        if (kind == TransactionKind::rename_module) {
            auto const previous{store.module_settings(original_module)};
            auto const old_name{module_name(store, original_module)};
            if (strings.view(store.module_settings_store
                                 .name[store.module_settings_store.index(previous)]) == text) {
                continue;
            }
            auto const settings{store.module_settings_store.clone(previous)};
            store.module_settings_store.name[store.module_settings_store.index(settings)] =
                strings.intern(text);
            auto const replacement{store.normal_modules.clone(original_module)};
            store.normal_modules.settings[store.normal_modules.index(replacement)] = settings;
            manifest.modules[module] = replacement;
            state.inherit_module(original_module, replacement);
            auto const module_count{manifest.modules.size()};
            for (std::size_t target{}; target < module_count; ++target) {
                auto const target_module{manifest.modules[target]};
                if (target_module.kind() != ModuleKind::normal) {
                    continue;
                }
                auto decls{declarations_of(store, target_module)};
                bool changed{};
                for (auto& declaration : decls) {
                    auto const before{declaration};
                    declaration = rewrite_type_references(
                        store, declaration, [&](auto const&, TypeRefId reference, auto) {
                            auto const spelling{strings.view(
                                store.type_refs.name[store.type_refs.index(reference)])};
                            auto const prefix{"@" + old_name + "/"};
                            if (!spelling.starts_with(prefix)) {
                                return reference;
                            }
                            auto const updated{store.type_refs.clone(reference)};
                            store.type_refs.name[store.type_refs.index(updated)] = strings.intern(
                                "@" + text + "/" + std::string{spelling.substr(prefix.size())});
                            return updated;
                        });
                    if (before != declaration) {
                        state.inherit_declaration(before, declaration);
                        changed = true;
                    }
                }
                if (changed) {
                    publish_module(state, manifest, target, decls);
                }
            }
            continue;
        }
        auto values{declarations_of(store, original_module)};
        auto const index{kind == TransactionKind::create
                             ? values.size()
                             : declaration_index(store,
                                                 values,
                                                 durable_strings.view(declarations_[instruction]))};
        if (kind == TransactionKind::erase) {
            record_removed(values[index], original_module);
            values.erase(values.begin() + static_cast<std::ptrdiff_t>(index));
            publish_module(state, manifest, module, values);
            continue;
        }
        if (kind == TransactionKind::create || kind == TransactionKind::replace) {
            auto const file{state.storage().sources.add(
                "edit.lispb", "(module edit :header \"Edit.h\" " + text + ")")};
            auto const parsed{parse_module_source(state.storage(), file)};
            auto const created{declarations_of(store, parsed.modules.front())};
            if (created.size() != 1) {
                throw std::invalid_argument{"Expected exactly one declaration"};
            }
            state.comments.capture(state.storage(), parsed.modules, parsed.file);
            std::optional<DeclarationHandle> replaced;
            if (kind == TransactionKind::create) {
                values.push_back(created.front());
                state.register_declaration(created.front());
            } else {
                replaced = values[index];
                record_removed(*replaced, original_module, false);
                state.inherit_declaration(*replaced, created.front());
                values[index] = created.front();
            }
            publish_module(state, manifest, module, values);
            if (replaced &&
                declaration_name(store, *replaced) != declaration_name(store, created.front())) {
                repair_references(
                    state, manifest, *replaced, original_module, created.front(), original_module);
            }
            continue;
        }
        auto const previous{values[index]};
        if (kind == TransactionKind::move) {
            auto const destination{module_index(manifest, text)};
            if (destination == module) {
                continue;
            }
            record_removed(previous, original_module, false);
            auto const destination_module{manifest.modules[destination]};
            auto const moved{
                qualify_moved_references(manifest, previous, original_module, destination_module)};
            if (moved != previous) {
                state.inherit_declaration(previous, moved);
            }
            auto target{declarations_of(store, destination_module)};
            target.push_back(moved);
            values.erase(values.begin() + static_cast<std::ptrdiff_t>(index));
            publish_module(state, manifest, module, values);
            publish_module(state, manifest, destination, target);
            repair_references(
                state, manifest, previous, original_module, moved, destination_module);
            continue;
        }
        if (kind == TransactionKind::rename || kind == TransactionKind::duplicate) {
            if (kind == TransactionKind::rename &&
                strings.view(declaration_name(store, previous)) == text) {
                continue;
            }
            auto const replacement{renamed(store, previous, text)};
            if (kind == TransactionKind::rename) {
                state.inherit_declaration(previous, replacement);
                values[index] = replacement;
            } else {
                state.register_declaration(replacement);
                state.comments.inherit(previous, replacement);
                values.insert(values.begin() + static_cast<std::ptrdiff_t>(index + 1), replacement);
            }
            publish_module(state, manifest, module, values);
            if (kind == TransactionKind::rename) {
                repair_references(
                    state, manifest, previous, original_module, replacement, original_module);
            }
            continue;
        }
        auto replacement{previous};
        if (kind == TransactionKind::packed_storage) {
            if (!packed_declaration(store, previous)) {
                throw std::invalid_argument{"Expected a packed record or union declaration"};
            }
            auto edit_storage = [&](auto& schemas) {
                auto const row{schemas.index(previous)};
                auto const& old{schemas.cpp_projection[row]};
                if (old &&
                    ((!old->storage && text == "auto") ||
                     (old->storage &&
                      strings.view(store.type_refs.name[store.type_refs.index(*old->storage)]) ==
                          text))) {
                    return;
                }
                replacement = schemas.clone(previous);
                schemas.cpp_projection[schemas.index(replacement)] =
                    CppProjection{.storage = text == "auto" ? std::nullopt
                                                            : std::optional{new_type(store, text)}};
            };
            if (previous.kind() == DeclarationKind::record) {
                edit_storage(store.records);
            } else if (previous.kind() == DeclarationKind::union_type) {
                edit_storage(store.unions);
            } else {
                throw std::invalid_argument{"Expected a record or union declaration"};
            }
        } else if (kind == TransactionKind::add_enumerator ||
                   kind == TransactionKind::delete_enumerator ||
                   kind == TransactionKind::rename_enumerator ||
                   kind == TransactionKind::enumerator_initializer ||
                   kind == TransactionKind::reorder_enumerator) {
            if (previous.kind() != DeclarationKind::enumeration) {
                throw std::invalid_argument{"Expected an enum declaration"};
            }
            auto const range{store.items(store.enums.values[store.enums.index(previous)])};
            std::vector<EnumeratorId> children{range.begin(), range.end()};
            auto const found{std::ranges::find_if(children, [&](auto id) {
                return strings.view(store.enumerators.name[store.enumerators.index(id)]) ==
                       child_name;
            })};
            if (kind != TransactionKind::add_enumerator && found == children.end()) {
                throw std::invalid_argument{"Unknown enumerator '" + child_name + "'"};
            }
            auto const position{static_cast<std::size_t>(found - children.begin())};
            if (kind == TransactionKind::rename_enumerator && child_name == text) {
                continue;
            }
            if (kind == TransactionKind::reorder_enumerator && position == number) {
                continue;
            }
            if (kind == TransactionKind::enumerator_initializer) {
                auto const previous_initializer{
                    store.enumerators.initializer[store.enumerators.index(*found)]};
                if ((previous_initializer && strings.view(*previous_initializer) == text) ||
                    (!previous_initializer && text.empty())) {
                    continue;
                }
            }
            if (kind == TransactionKind::delete_enumerator) {
                children.erase(found);
            } else if (kind == TransactionKind::reorder_enumerator) {
                reorder(children, position, *number);
            } else {
                auto const child{kind == TransactionKind::add_enumerator
                                     ? store.enumerators.append()
                                     : store.enumerators.clone(*found)};
                auto const row{store.enumerators.index(child)};
                if (kind == TransactionKind::add_enumerator ||
                    kind == TransactionKind::rename_enumerator) {
                    store.enumerators.name[row] =
                        strings.intern(kind == TransactionKind::add_enumerator ? child_name : text);
                }
                if (kind == TransactionKind::add_enumerator ||
                    kind == TransactionKind::enumerator_initializer) {
                    store.enumerators.initializer[row] =
                        text.empty() ? std::nullopt : std::optional{strings.intern(text)};
                }
                if (kind == TransactionKind::add_enumerator) {
                    children.push_back(child);
                } else {
                    children[position] = child;
                }
            }
            replacement = store.enums.clone(previous);
            store.enums.values[store.enums.index(replacement)] = store.range(children);
        } else {
            assert(kind == TransactionKind::member_bits || kind == TransactionKind::member_type ||
                   kind == TransactionKind::add_member || kind == TransactionKind::delete_member ||
                   kind == TransactionKind::rename_member ||
                   kind == TransactionKind::reorder_member ||
                   kind == TransactionKind::member_count);
            if (previous.kind() != DeclarationKind::soa &&
                previous.kind() != DeclarationKind::record &&
                previous.kind() != DeclarationKind::union_type) {
                throw std::invalid_argument{"Expected a soa, record or union declaration"};
            }
            auto edit_members = [&](auto& columns, auto& parent, auto& ranges) {
                auto const range{store.items(ranges[parent.index(previous)])};
                using Id = std::remove_cvref_t<decltype(range.front())>;
                std::vector<Id> children{range.begin(), range.end()};
                auto const found{std::ranges::find_if(children, [&](auto id) {
                    return strings.view(columns.name[columns.index(id)]) == child_name;
                })};
                if (kind != TransactionKind::add_member && found == children.end()) {
                    throw std::invalid_argument{"Unknown member '" + child_name + "'"};
                }
                auto const position{static_cast<std::size_t>(found - children.begin())};
                if (kind == TransactionKind::rename_member && child_name == text) {
                    return;
                }
                if (kind == TransactionKind::reorder_member && position == number) {
                    return;
                }
                if (kind == TransactionKind::delete_member) {
                    children.erase(found);
                } else if (kind == TransactionKind::reorder_member) {
                    reorder(children, position, *number);
                } else {
                    if constexpr (std::is_same_v<Id, SoaMemberId>) {
                        if (kind == TransactionKind::member_type &&
                            columns.kind[columns.index(*found)] != SoaMemberKind::array) {
                            throw std::invalid_argument{"Expected an array member"};
                        }
                    }
                    if (kind == TransactionKind::member_type &&
                        strings.view(store.type_refs.name[store.type_refs.index(
                            columns.type[columns.index(*found)])]) == text) {
                        return;
                    }
                    auto const child{kind == TransactionKind::add_member ? columns.append()
                                                                         : columns.clone(*found)};
                    auto const row{columns.index(child)};
                    if (kind == TransactionKind::add_member ||
                        kind == TransactionKind::rename_member) {
                        columns.name[row] =
                            strings.intern(kind == TransactionKind::add_member ? child_name : text);
                    }
                    if (kind == TransactionKind::add_member ||
                        kind == TransactionKind::member_type) {
                        columns.type[row] = new_type(store, text);
                    }
                    if constexpr (std::is_same_v<Id, SoaMemberId>) {
                        if (kind == TransactionKind::member_bits ||
                            kind == TransactionKind::member_count) {
                            throw std::invalid_argument{"Array counts belong to record members"};
                        }
                        if (kind == TransactionKind::add_member ||
                            kind == TransactionKind::member_type) {
                            columns.kind[row] = SoaMemberKind::array;
                        }
                    } else {
                        if (kind == TransactionKind::member_bits &&
                            columns.bits[columns.index(*found)] == number) {
                            return;
                        }
                        if (kind == TransactionKind::member_count) {
                            columns.count[row] = number;
                        }
                        if (kind == TransactionKind::member_bits) {
                            columns.bits[row] = static_cast<std::uint32_t>(*number);
                        }
                    }
                    if (kind == TransactionKind::add_member) {
                        children.push_back(child);
                    } else {
                        children[position] = child;
                    }
                }
                if constexpr (std::is_same_v<Id, ChildOccurrenceId>) {
                    if (kind == TransactionKind::rename_member) {
                        for (auto& child : children) {
                            auto const tag{columns.tag[columns.index(child)]};
                            if (tag && strings.view(*tag) == child_name) {
                                child = columns.clone(child);
                                columns.tag[columns.index(child)] = strings.intern(text);
                            }
                        }
                    }
                }
                replacement = parent.clone(previous);
                ranges[parent.index(replacement)] = store.range(children);
            };
            if (previous.kind() == DeclarationKind::soa) {
                edit_members(store.soa_members, store.soas, store.soas.members);
            } else if (previous.kind() == DeclarationKind::union_type) {
                edit_members(store.child_occurrences, store.unions, store.unions.alternatives);
            } else {
                edit_members(store.child_occurrences, store.records, store.records.members);
            }
        }
        if (replacement != previous) {
            state.inherit_declaration(previous, replacement);
            if (kind == TransactionKind::rename_member) {
                repair_member_metadata(state, manifest, previous, child_name, text);
            }
            if (kind == TransactionKind::delete_member) {
                repair_member_metadata(state, manifest, previous, child_name, {});
            }
            if (kind == TransactionKind::rename_member) {
                state.comments.rename_child(replacement, "member", child_name, text);
            }
            if (kind == TransactionKind::rename_enumerator) {
                state.comments.rename_child(replacement, "value", child_name, text);
            }
            if (kind == TransactionKind::delete_member) {
                state.comments.remove_child(replacement, "member", child_name);
            }
            if (kind == TransactionKind::delete_enumerator) {
                state.comments.remove_child(replacement, "value", child_name);
            }
            values[index] = replacement;
            publish_module(state, manifest, module, values);
            if (kind == TransactionKind::rename_enumerator) {
                rename_discriminator_case(
                    state, manifest, manifest.modules[module], replacement, child_name, text);
            }
        }
    }
    // Do not reinterpret a removed authored type as an unrelated opaque C++ spelling.
    auto const removed_count{removed_names.size()};
    for (std::size_t index{}; index < removed_count; ++index) {
        auto const& name{removed_names[index]};
        auto const& space{removed_namespaces[index]};
        bool restored{};
        for (auto const module : manifest.modules) {
            if (module.kind() != ModuleKind::normal || namespace_name(store, module) != space) {
                continue;
            }
            for (auto const declaration : declarations_of(store, module)) {
                auto names{generated_cpp_names(store, declaration)};
                names.push_back(declaration_name(store, declaration));
                restored |=
                    std::ranges::any_of(names, [&](auto id) { return strings.view(id) == name; });
            }
        }
        if (restored) {
            continue;
        }
        for (auto const module : manifest.modules) {
            if (module.kind() != ModuleKind::normal) {
                continue;
            }
            auto const current_namespace{namespace_name(store, module)};
            for (auto const declaration : declarations_of(store, module)) {
                visit_type_references(
                    store, declaration, [&](auto const&, TypeRefId reference, auto) {
                        auto const spelling{std::string{
                            strings.view(store.type_refs.name[store.type_refs.index(reference)])}};
                        if (refers_to(manifest, current_namespace, spelling, space, name)) {
                            throw std::invalid_argument{"Deleted declaration '" + name +
                                                        "' is still referenced"};
                        }
                    });
            }
        }
    }
}
}
