#include "validation.h"

#include "lowering.h"

#include <codegen/sexpr/sources.h>
#include <codegen/sexpr/syntax.h>
#include <kernel_codegen/operation_kind.h>
#include <kernel_codegen/scalar_type.h>
#include <kernel_codegen/storage_kind.h>
#include <kernel_codegen/variant_kind.h>
#include <lispb/diagnostics.h>
#include <lispb/indices.h>

#include <algorithm>
#include <compare>
#include <cstddef>
#include <cstdint>
#include <set>
#include <stdexcept>
#include <utility>
#include <vector>

namespace kernel_codegen::detail {
namespace {
enum class ParameterShape : std::uint8_t { scalar, read_view, write_view, owning_array };

struct SignatureKey {
    lispb::IdentifierId name;
    ScalarType type;
    std::vector<ParameterShape> parameters;
    auto operator<=>(SignatureKey const&) const = default;
};

void validate_portable_signatures(lispb::codegen::sexpr::Syntax& syntax,
                                  Document const& document,
                                  ModuleId const module,
                                  bool const standard) {
    auto const& expanded{document.expanded_variants};
    std::set<SignatureKey> signatures;
    auto const range{document.modules.expanded_variants(module)};
    auto const end{range.offset + range.count};
    for (auto variant_index{range.offset}; variant_index < end; ++variant_index) {
        auto const id{ExpandedVariantId{variant_index}};
        auto const operation{expanded.operation(id)};
        auto const overload_count{
            !standard && document.variants.kind(expanded.variant(id)) == VariantKind::in_place ? 2
                                                                                               : 1};
        for (int overload{}; overload < overload_count; ++overload) {
            SignatureKey signature{
                document.variants.name(expanded.variant(id)), expanded.type(id), {}};
            auto const operand_count{document.operations.operands(operation).size()};
            for (std::size_t index{}; index < operand_count; ++index) {
                auto const shape{expanded.storage(id)[index] == StorageKind::scalar
                                     ? ParameterShape::scalar
                                 : !is_target(document, id, index) ? ParameterShape::read_view
                                 : overload != 0                   ? ParameterShape::owning_array
                                                                   : ParameterShape::write_view};
                auto const count{
                    document.operands.is_component(document.operations.operands(operation)[index])
                        ? document.operations.components(operation).size()
                        : 1};
                signature.parameters.insert(signature.parameters.end(), count, shape);
            }
            if (document.variants.kind(expanded.variant(id)) == VariantKind::out_of_place) {
                auto const count{operation.kind() == OperationKind::component_map
                                     ? document.operations.components(operation).size()
                                     : 1};
                signature.parameters.insert(
                    signature.parameters.end(), count, ParameterShape::write_view);
            }
            if (!signatures.insert(std::move(signature)).second) {
                syntax.sources().diagnostics().fail(
                    syntax.sources(),
                    document.variants.span(expanded.variant(id)),
                    "Duplicate generated kernel signature: " +
                        document.text(document.variants.name(expanded.variant(id))));
            }
        }
    }
}
}

auto analyze(lispb::codegen::sexpr::Syntax& syntax,
             Document const& document,
             ModuleId const module,
             lispb::KernelProfile const profile) -> ModuleAnalysis {
    ModuleAnalysis result{module, {}, {}};
    auto const emissions{document.modules.emissions(module)};
    auto const emission{std::ranges::find_if(emissions, [&](auto const candidate) {
        return document.emissions.profile(candidate) == profile;
    })};
    if (emission == emissions.end()) {
        return result;
    }

    result.emission = *emission;
    validate_portable_signatures(
        syntax, document, module, profile == lispb::KernelProfile::standard);
    if (!document.emissions.tests(*emission)) {
        return result;
    }

    auto const range{document.modules.expanded_variants(module)};
    result.fixtures.reserve(range.count);
    auto const end{range.offset + range.count};
    for (auto variant_index{range.offset}; variant_index < end; ++variant_index) {
        auto const id{ExpandedVariantId{variant_index}};
        try {
            result.fixtures.push_back(make_reference_fixture(document, id, 31));
        } catch (std::invalid_argument const& error) {
            auto const operation{document.expanded_variants.operation(id)};
            syntax.sources().diagnostics().fail(
                syntax.sources(),
                document.expressions.span(document.operations.expression(operation)),
                error.what());
        }
    }

    return result;
}
}
