#pragma once

#include <codegen/ast/expr.h>
#include <codegen/ast/function_definition_flags.h>
#include <codegen/ast/function_exception_specification.h>
#include <codegen/ast/function_formatting.h>
#include <codegen/ast/function_implementation.h>
#include <codegen/ast/function_kind.h>
#include <codegen/ast/function_receiver.h>
#include <codegen/ast/indices.h>
#include <codegen/ast/return_syntax.h>
#include <codegen/ast/template_id.h>
#include <lispb/indices.h>

#include <vector>

namespace lispb::codegen {

// Assemble locally, then publish an immutable function with CppIR::add_function().
struct FunctionDefinition {
    Expr name{};
    CppTypeId return_type{};
    ReturnSyntax return_syntax{ReturnSyntax::direct};
    FunctionKind kind{FunctionKind::ordinary};
    std::vector<FunctionParameterId> parameters;
    // Leave empty for a declaration without an implementation.
    FunctionImplementation implementation{};
    FunctionExceptionSpecification exception_specification{};
    FunctionReceiver receiver{FunctionReceiver::unqualified};
    FunctionDefinitionFlags flags{};
    StringId export_specifier{};
    TemplateId template_declaration{};
    Expr requires_clause{};
    FunctionFormatting formatting{};
    std::vector<TypeDependencyId> dependencies;
};

}
