#pragma once

#include <ioj/layout/abi_profile.hpp>
#include <ioj/layout/type_facts.hpp>

#include <lispb/schema/type_graph.h>
#include <lispb/schema/type_identity.h>

#include <optional>
#include <string>
#include <vector>

namespace ioj::layout {
struct ExternalDependency {
    std::string cpp_spelling;
    std::vector<lispb::schema::TypeId> types;
    std::vector<std::string> registered_names;
    std::vector<lispb::schema::TypeUseId> uses;
    std::vector<lispb::schema::TypeIdentity> declarations;
    std::vector<std::string> modules;
    bool semantics_known{};
    std::optional<TypeFacts> physical_facts{};
};

auto external_dependencies(lispb::schema::TypeGraph const& types, AbiProfile const* abi = nullptr)
    -> std::vector<ExternalDependency>;
}
