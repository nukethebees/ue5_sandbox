#pragma once

#include <codegen/ast/cpp_declaration_id.h>
#include <codegen/ast/environment_id.h>
#include <codegen/ast/environment_kind.h>
#include <lispb/indices.h>

#include <cstddef>
#include <span>
#include <unordered_map>
#include <vector>

namespace lispb::codegen {

struct EnvironmentSymbol {
    CppDeclarationId declaration;
    EnvironmentId introduced_scope;
    bool ambiguous{};
};

enum class LookupStatus { found, missing, unknown };

struct SymbolLookup {
    LookupStatus status{LookupStatus::missing};
    EnvironmentSymbol symbol;
};

class Environments {
  public:
    using SymbolTable = std::unordered_map<IdentifierId, EnvironmentSymbol>;

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
                 IdentifierId name,
                 CppDeclarationId declaration,
                 EnvironmentId introduced_scope = {}) -> EnvironmentSymbol;
    auto named(EnvironmentId parent,
               EnvironmentKind kind,
               IdentifierId name,
               CppDeclarationId declaration = {}) -> EnvironmentId;
    void import_symbols(EnvironmentId destination, EnvironmentId source);

    // Search parents for the first component, then only its introduced scopes.
    // Missing and unknown results contain null handles.
    auto lookup(EnvironmentId environment, std::span<IdentifierId const> name) const
        -> SymbolLookup;
    auto lookup_local(EnvironmentId environment, IdentifierId name) const -> SymbolLookup;
    // Register names supplied by includes without claiming knowledge of their contents.
    auto reference(std::span<IdentifierId const> name, CppDeclarationId external)
        -> CppDeclarationId;
    // Ignore unknown contents when locating an already registered owner scope.
    // Return null when no such scope exists.
    auto find_environment(EnvironmentId environment, std::span<IdentifierId const> name) const
        -> EnvironmentId;
  private:
    auto index(EnvironmentId environment) const -> std::size_t;
    auto find_local(EnvironmentId environment, IdentifierId name) const -> EnvironmentSymbol;

    std::vector<EnvironmentId> parents_;
    std::vector<EnvironmentKind> kinds_;
    std::vector<SymbolTable> symbols_;
    std::vector<bool> complete_;
};

// Select one environment for a lexical emission region, then restore its caller's environment.
class EnvironmentGuard {
  public:
    EnvironmentGuard(EnvironmentId& current, EnvironmentId next);
    ~EnvironmentGuard();
    EnvironmentGuard(EnvironmentGuard const&) = delete;
    auto operator=(EnvironmentGuard const&) -> EnvironmentGuard& = delete;
  private:
    EnvironmentId& current_;
    EnvironmentId previous_;
};

}
