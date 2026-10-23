#include <codegen/ast/environments.h>

#include <cassert>
#include <stdexcept>

namespace lispb::codegen {

Environments::Environments() {
    clear();
}
void Environments::clear() {
    parents_.clear();
    kinds_.clear();
    symbols_.clear();
    bindings_complete_.clear();
    external_declaration_count_ = 0;
    parents_.emplace_back();
    kinds_.push_back(EnvironmentKind::global);
    symbols_.emplace_back();
    bindings_complete_.push_back(true);
}

auto Environments::index(EnvironmentId environment) const -> std::size_t {
    assert(!environment.is_null() && environment.value < parents_.size());
    return environment.value;
}
auto Environments::append(EnvironmentId parent, EnvironmentKind kind) -> EnvironmentId {
    (void)index(parent);
    assert(kind != EnvironmentKind::global);
    if (parents_.size() == EnvironmentId::null_value) {
        throw std::length_error{"C++ environment ID capacity exceeded"};
    }

    EnvironmentId const result{static_cast<std::uint16_t>(parents_.size())};
    parents_.push_back(parent);
    kinds_.push_back(kind);
    symbols_.emplace_back();
    bindings_complete_.push_back(true);
    return result;
}
auto Environments::parent(EnvironmentId environment) const -> EnvironmentId {
    return parents_[index(environment)];
}
auto Environments::kind(EnvironmentId environment) const -> EnvironmentKind {
    return kinds_[index(environment)];
}
auto Environments::symbols(EnvironmentId environment) const -> SymbolTable const& {
    return symbols_[index(environment)];
}
void Environments::mark_unknown(EnvironmentId environment) {
    bindings_complete_[index(environment)] = false;
}

auto Environments::declare(EnvironmentId environment,
                           IdentifierId name,
                           CppDeclarationId declaration,
                           EnvironmentId introduced_scope) -> SymbolBinding {
    return bind(environment,
                name,
                {.declaration = declaration,
                 .declaring_scope = environment,
                 .introduced_scope = introduced_scope});
}
auto Environments::bind(EnvironmentId environment, IdentifierId name, SymbolBinding symbol)
    -> SymbolBinding {
    assert(name.value != 0 && !symbol.declaration.is_null());
    auto& table{symbols_[index(environment)]};
    auto const [entry, inserted]{table.try_emplace(name, symbol)};
    if (!inserted && !symbol.introduced_scope.is_null()) {
        assert(entry->second.introduced_scope.is_null() ||
               entry->second.introduced_scope == symbol.introduced_scope);
        entry->second.introduced_scope = symbol.introduced_scope;
    }
    return entry->second;
}
auto Environments::find_local(EnvironmentId environment, IdentifierId name) const -> SymbolBinding {
    auto const& table{symbols(environment)};
    auto const found{table.find(name)};
    return found == table.end() ? SymbolBinding{} : found->second;
}
auto Environments::named(EnvironmentId parent,
                         EnvironmentKind kind,
                         IdentifierId name,
                         CppDeclarationId declaration) -> EnvironmentId {
    auto const existing{find_local(parent, name)};
    if (!existing.introduced_scope.is_null()) {
        return existing.introduced_scope;
    }

    auto const environment{append(parent, kind)};
    if (declaration.is_null()) {
        declaration = CppDeclarationId{environment};
    }
    declare(parent, name, declaration, environment);
    return environment;
}
void Environments::import_symbols(EnvironmentId destination, EnvironmentId source) {
    assert(destination != source);
    auto& table{symbols_[index(destination)]};
    for (auto const& [name, symbol] : symbols(source)) {
        auto const [entry, inserted]{table.emplace(name, symbol)};
        if (!inserted && !entry->second.same_declaration(symbol)) {
            entry->second.ambiguous = true;
        }
    }
    if (!bindings_complete_[index(source)]) {
        mark_unknown(destination);
    }
}

auto Environments::lookup_local(EnvironmentId environment, IdentifierId name) const
    -> SymbolLookup {
    auto const symbol{find_local(environment, name)};
    if (symbol.declaration.is_null()) {
        return {.status = bindings_complete_[index(environment)] ? LookupStatus::missing
                                                                 : LookupStatus::unknown};
    }
    if (symbol.ambiguous) {
        return {.status = LookupStatus::unknown};
    }
    return {.status = LookupStatus::found, .symbol = symbol};
}
auto Environments::lookup(EnvironmentId environment, std::span<IdentifierId const> name) const
    -> SymbolLookup {
    assert(!name.empty());
    SymbolLookup result;
    for (auto current{environment}; !current.is_null(); current = parent(current)) {
        result = lookup_local(current, name.front());
        if (result.status != LookupStatus::missing) {
            break;
        }
    }
    auto const count{name.size()};
    for (std::size_t component{1}; component < count && result.status == LookupStatus::found;
         ++component) {
        if (result.symbol.introduced_scope.is_null()) {
            return {};
        }
        result = lookup_local(result.symbol.introduced_scope, name[component]);
    }
    return result;
}
auto Environments::reference(std::span<IdentifierId const> name) -> SymbolBinding {
    assert(!name.empty());
    auto environment{global()};
    auto const count{name.size()};
    for (std::size_t component{}; component + 1 < count; ++component) {
        environment = named(environment, EnvironmentKind::external, name[component]);
    }
    auto const existing{find_local(environment, name.back())};
    if (!existing.declaration.is_null()) {
        return existing;
    }
    if (external_declaration_count_ == 0x01000000U) {
        throw std::length_error{"C++ external declaration ID capacity exceeded"};
    }

    auto const declaration{CppDeclarationId{ExternalDeclarationId{external_declaration_count_++}}};
    return declare(environment, name.back(), declaration);
}
auto Environments::find_environment(EnvironmentId environment,
                                    std::span<IdentifierId const> name) const -> EnvironmentId {
    assert(!name.empty());
    SymbolBinding result;
    for (auto current{environment}; !current.is_null(); current = parent(current)) {
        result = find_local(current, name.front());
        if (!result.declaration.is_null()) {
            break;
        }
    }
    auto const count{name.size()};
    for (std::size_t component{1}; component < count; ++component) {
        if (result.introduced_scope.is_null()) {
            return {};
        }
        result = find_local(result.introduced_scope, name[component]);
    }
    return result.introduced_scope;
}

}
