#pragma once

#include <codegen/ast/function_definition.h>
#include <codegen/ast/function_flags.h>

#include <span>

namespace lispb::codegen {

struct FunctionView {
    Expr name{};
    CppTypeId return_type{};
    ReturnSyntax return_syntax{ReturnSyntax::direct};
    FunctionKind kind{FunctionKind::ordinary};
    std::span<FunctionParameterId const> parameters;
    FunctionImplementationView implementation{};
    FunctionExceptionSpecification exception_specification{};
    FunctionReceiver receiver{FunctionReceiver::unqualified};
    FunctionFlags flags{};
    StringId export_specifier{};
    TemplateId template_declaration{};
    Expr requires_clause{};
    FunctionFormatting formatting{};
    std::span<TypeDependencyId const> dependencies;
};

}
