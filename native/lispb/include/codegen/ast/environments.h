#pragma once

#include <codegen/ast/environment_id.h>
#include <codegen/ast/environment_kind.h>
#include <codegen/ast/symbol_binding.h>
#include <codegen/ast/symbol_lookup.h>
#include <lispb/indices.h>

#include <cstddef>
#include <cstdint>
#include <span>
#include <unordered_map>
#include <vector>

namespace lispb::codegen {

class Environments {
  public:
    using SymbolTable = std::unordered_map<StringId, SymbolBinding>;

    Environments();
    void clear();
    auto global() const -> EnvironmentId { return EnvironmentId{0}; }
    auto size() const -> std::size_t { return parents_.size(); }

    // Require valid parent/row IDs. Only the global environment has a null parent.
    // Throw length_error when the 16-bit environment ID space is exhausted.
    auto append(EnvironmentId parent, EnvironmentKind kind) -> EnvironmentId;
    auto parent(EnvironmentId environment) const -> EnvironmentId;
    auto kind(EnvironmentId environment) const -> EnvironmentKind;
    auto symbols(EnvironmentId environment) const -> SymbolTable const&;
    void mark_unknown(EnvironmentId environment);

    // Repeated declarations retain the first declaration's identity.
    auto declare(EnvironmentId environment,
                 StringId name,
                 CppDeclarationId declaration,
                 EnvironmentId introduced_scope = {}) -> SymbolBinding;
    // Preserve declaration ownership for injected names and anonymous namespace imports.
    auto bind(EnvironmentId environment, StringId name, SymbolBinding symbol) -> SymbolBinding;
    auto named(EnvironmentId parent,
               EnvironmentKind kind,
               StringId name,
               CppDeclarationId declaration = {}) -> EnvironmentId;
    void import_symbols(EnvironmentId destination, EnvironmentId source);

    // Search parents for the first component, then only its introduced scopes.
    // Missing and unknown results contain null handles.
    auto lookup(EnvironmentId environment, std::span<StringId const> name) const -> SymbolLookup;
    auto lookup_local(EnvironmentId environment, StringId name) const -> SymbolLookup;
    // Register names supplied by includes without claiming knowledge of their contents.
    // Allocate scopes only for qualifiers; throw length_error if external IDs are exhausted.
    auto reference(std::span<StringId const> name) -> SymbolBinding;
    // Ignore unknown contents when locating an already registered owner scope.
    // Return null when no such scope exists.
    auto find_environment(EnvironmentId environment, std::span<StringId const> name) const
        -> EnvironmentId;
  private:
    auto index(EnvironmentId environment) const -> std::size_t;
    auto find_local(EnvironmentId environment, StringId name) const -> SymbolBinding;

    std::vector<EnvironmentId> parents_;
    std::vector<EnvironmentKind> kinds_;
    std::vector<SymbolTable> symbols_;
    std::vector<bool> bindings_complete_;
    std::uint32_t external_declaration_count_{};
};

}
