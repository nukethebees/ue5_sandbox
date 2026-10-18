#include "renderer.h"

#include "lowering.h"
#include "portable_renderer.h"
#include "test_renderer.h"

#include <codegen/ast/cpp_files.h>
#include <codegen/ast/cpp_ir.h>
#include <codegen/ast/indices.h>
#include <codegen/ast/modules.h>
#include <kernel_codegen/document.h>
#include <kernel_codegen/indices.h>
#include <kernel_codegen/operation_kind.h>
#include <kernel_codegen/scalar_type.h>
#include <kernel_codegen/storage_kind.h>
#include <kernel_codegen/variant_kind.h>
#include <lispb/indices.h>
#include <lispb/kernel_profile.h>
#include <lispb/strings.h>
#include <lispb/text_artifact.h>

#include <algorithm>
#include <compare>
#include <cstddef>
#include <cstdint>
#include <filesystem>
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

void validate_portable_signatures(Document const& document,
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
                throw std::invalid_argument{
                    "Duplicate generated kernel signature: " +
                    document.text(document.variants.name(expanded.variant(id)))};
            }
        }
    }
}

}

void validate(Document const& document, ModuleId const module, lispb::KernelProfile const profile) {
    auto const emissions{document.modules.emissions(module)};
    auto const emission{std::ranges::find_if(emissions, [&](auto const candidate) {
        return document.emissions.profile(candidate) == profile;
    })};
    if (emission == emissions.end()) {
        return;
    }
    validate_portable_signatures(document, module, profile == lispb::KernelProfile::standard);
}

auto render(lispb::codegen::CppIR& cpp,
            Document const& document,
            ModuleId const module,
            lispb::KernelProfile const profile) -> std::vector<lispb::TextArtifact> {
    auto const emissions{document.modules.emissions(module)};
    auto const emission{std::ranges::find_if(emissions, [&](auto const candidate) {
        return document.emissions.profile(candidate) == profile;
    })};
    if (emission == emissions.end()) {
        return {};
    }

    validate_portable_signatures(document, module, profile == lispb::KernelProfile::standard);
    auto const generated{lower_portable(cpp, module, *emission, document)};
    auto const generated_index{cpp.modules.index(generated)};
    std::vector<lispb::codegen::CppFileId> files{cpp.modules.header[generated_index],
                                                 cpp.modules.source[generated_index]};
    auto const tests{document.emissions.tests(*emission)};
    if (tests) {
        files.push_back(lower_standard_tests(cpp, *emission, module, document));
    }
    std::vector<lispb::TextArtifact> result;
    result.reserve(files.size());
    for (auto const file : files) {
        auto const index{cpp.files.index(file)};
        result.push_back(
            lispb::TextArtifact{std::filesystem::path{cpp.strings.view(cpp.files.path[index])},
                                cpp.render(file),
                                cpp.files.flags[index].format_generated()});
    }
    return result;
}

}
