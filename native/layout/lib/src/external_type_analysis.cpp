#include <ioj/layout/external_type_analysis.hpp>

#include <ioj/layout/analyzer/analyzer.hpp>
#include <ioj/layout/analyzer/soa_analysis.hpp>

#include <codegen/schema.h>
#include <codegen/schema/declaration_schema.h>
#include <codegen/schema/soa_backend.h>
#include <lispb/schema/external_types.h>
#include <lispb/schema/soa_types.h>
#include <lispb/strings.h>

#include <algorithm>
#include <string>

namespace ioj::layout {
auto analyze_external_type(lispb::schema::TypeGraph const& types,
                           lispb::schema::TypeId const type,
                           ResolvedVariant const& variant,
                           AbiProfile const& abi,
                           std::uint64_t const capacity,
                           SoaAllocationStrategy const allocation_strategy)
    -> ExternalTypeAnalysis {
    auto const& node{type};
    auto const& external{types.definition(node)};
    PhysicalFactsResolver resolver{types, abi, &variant};
    ExternalTypeAnalysis result{};
    result.semantics_known =
        types.definitions()
            .externals.integer_semantics[types.definitions().externals.index(external)]
            .has_value() ||
        types.definitions()
            .externals.floating_semantics[types.definitions().externals.index(external)]
            .has_value();
    result.physical = resolver.resolve(type);

    if (result.physical.facts.has_value()) {
        return result;
    }
    auto const spelling{lispb::codegen::native_spelling(
        std::string{types.strings().view(types.cpp_spelling(node))})};
    auto const count{types.types().size()};
    for (std::size_t index{}; index < count; ++index) {
        auto const id{lispb::schema::TypeId{static_cast<std::uint32_t>(index)}};
        auto const& candidate{id};
        if (types.origin(candidate) != lispb::schema::TypeOrigin::declaration) {
            continue;
        }
        std::vector<Diagnostic> diagnostics;
        if (auto const soa{
                ((types.definition(candidate)).kind() == lispb::schema::TypeDefinitionKind::soa
                     ? std::optional{types.definition(candidate)}
                     : std::nullopt)};
            static_cast<bool>(soa) &&
            types.definitions().soas.backend[types.definitions().soas.index(*soa)] ==
                lispb::codegen::SoaBackend::standard_library) {
            diagnostics =
                Analyzer::analyze_soa(types, id, variant, abi, capacity, allocation_strategy)
                    .diagnostics;
        } else {
            diagnostics = resolver.resolve(id).diagnostics;
        }
        if (std::ranges::any_of(diagnostics, [&](auto const& diagnostic) {
                return diagnostic.missing_physical_type == spelling;
            })) {
            auto owner{types.identity(candidate)};
            owner.name = lispb::codegen::declaration_name(types.declarations(),
                                                          types.source_declaration(candidate));
            if (std::ranges::find(result.blocked_declarations, owner) ==
                result.blocked_declarations.end()) {
                result.blocked_declarations.push_back(owner);
            }
        }
    }
    return result;
}
}
