#pragma once
#include <codegen/ast/expr.h>
#include <codegen/ast/function_exception_id.h>
#include <codegen/ast/function_flags.h>
#include <codegen/ast/function_formatting.h>
#include <codegen/ast/function_implementation_id.h>
#include <codegen/ast/function_kind.h>
#include <codegen/ast/function_receiver.h>
#include <codegen/ast/indices.h>
#include <codegen/ast/ir/range.h>
#include <codegen/ast/return_syntax.h>
#include <codegen/ast/template_id.h>
#include <lispb/indices.h>

#include <vector>

namespace lispb::codegen::ir {

struct Functions {
    std::vector<Expr> name;
    std::vector<CppTypeId> return_type;
    std::vector<ReturnSyntax> return_syntax;
    std::vector<FunctionKind> kind;
    std::vector<Range> parameters;
    std::vector<FunctionImplementationId> implementation;
    std::vector<FunctionExceptionId> exception_specification;
    std::vector<FunctionReceiver> receiver;
    std::vector<FunctionFlags> flags;
    std::vector<IdentifierId> export_specifier;
    std::vector<TemplateId> template_declaration;
    std::vector<Expr> requires_clause;
    std::vector<FunctionFormatting> formatting;
    std::vector<Range> dependencies;
};

} // namespace lispb::codegen::ir
