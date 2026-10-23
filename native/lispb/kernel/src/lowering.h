#pragma once

#include <codegen/ast/indices.h>
#include <kernel_codegen/document.h>
#include <kernel_codegen/indices.h>

#include <cstddef>
#include <span>
#include <string>
#include <vector>

namespace lispb::codegen {
class CppIR;
}

namespace kernel_codegen::detail {

auto lower_module(lispb::codegen::CppIR& cpp,
                  ModuleId module,
                  EmissionId emission,
                  Document const& document) -> lispb::codegen::ModuleId;

auto is_target(Document const& document, ExpandedVariantId id, std::size_t operand_index) -> bool;
auto operand_names(Document const& document, ExpandedVariantId id, std::size_t operand_index)
    -> std::vector<std::string>;
auto output_names(Document const& document, ExpandedVariantId id) -> std::vector<std::string>;
auto raw_name(Document const& document, ExpandedVariantId id) -> std::string;
auto contains_constant(Expressions const& expressions, ExpressionId expression) -> bool;
auto array_names(Document const& document, ExpandedVariantId id) -> std::vector<std::string>;
auto count_source(Document const& document, ExpandedVariantId id) -> std::string;
auto standard_type(ScalarType type) -> std::string;

}
