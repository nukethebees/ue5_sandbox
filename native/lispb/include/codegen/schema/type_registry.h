#pragma once

#include <codegen/schema/registered_types.h>
#include <lispb/indices.h>

#include <map>
#include <optional>
#include <string_view>

namespace lispb::codegen {

struct TypeRegistry {
    explicit TypeRegistry(RegisteredTypes& storage)
        : cpp_types{storage.cpp_types}
        , declarations{storage.declarations}
        , registered{storage} {}
    TypeRegistry(TypeRegistry const&) = default;
    TypeRegistry(TypeRegistry&&) = default;
    // Document transactions copy bindings while retaining shared immutable metadata versions.
    auto operator=(TypeRegistry const& other) -> TypeRegistry&;
    auto operator=(TypeRegistry&& other) noexcept -> TypeRegistry&;

    auto find(std::string_view name) const -> std::optional<RegisteredTypeId>;
    auto find(IdentifierId name) const -> std::optional<RegisteredTypeId>;
    auto bind(IdentifierId name, RegisteredTypeId type) -> bool;
    auto add(std::string_view name, CppTypeId type, ExternalScalarId scalar = {}) -> bool;
    auto add(IdentifierId name, CppTypeId type, ExternalScalarId scalar = {}) -> bool;
    void clear();

    CppTypes& cpp_types;
    Declarations& declarations;
    RegisteredTypes& registered;
    std::map<IdentifierId, RegisteredTypeId> entries;
};

auto external_scalar_schema(TypeRegistry const& types, std::string_view spelling)
    -> std::optional<ExternalScalarId>;

}
