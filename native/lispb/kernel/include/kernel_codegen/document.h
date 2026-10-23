#pragma once
#include <kernel_codegen/emissions.h>
#include <kernel_codegen/expanded_variants.h>
#include <kernel_codegen/expressions.h>
#include <kernel_codegen/modules.h>
#include <kernel_codegen/operands.h>
#include <kernel_codegen/operations.h>
#include <kernel_codegen/type_sets.h>
#include <kernel_codegen/variants.h>
#include <lispb/strings.h>

#include <string>
namespace kernel_codegen::detail {
struct Document {
    explicit Document(lispb::Strings& owner)
        : strings{owner}
        , expressions{owner} {}
    lispb::Strings& strings;
    Expressions expressions;
    Operands operands;
    Variants variants;
    TypeSets type_sets;
    Operations operations;
    Emissions emissions;
    Modules modules;
    ExpandedVariants expanded_variants;

    auto text(lispb::StringId const id) const -> std::string {
        return std::string{strings.view(id)};
    }
    // Operands are appended contiguously while parsing their owning operation.
    auto operand_index(OperationId operation, OperandId operand) const -> std::size_t;
    void clear();
};
}
