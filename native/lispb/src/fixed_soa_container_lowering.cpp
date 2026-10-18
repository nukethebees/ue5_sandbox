#include "fixed_soa_internal.h"
#include "lowering_context.h"
#include "lowering_utils.h"
#include "soa_internal.h"

#include <codegen/ast/access.h>
#include <codegen/ast/binary_operator.h>
#include <codegen/ast/cpp_ir.h>
#include <codegen/ast/cpp_types.h>
#include <codegen/ast/expr.h>
#include <codegen/ast/function_body_layout.h>
#include <codegen/ast/function_disposition_placement.h>
#include <codegen/ast/function_flags.h>
#include <codegen/ast/function_formatting.h>
#include <codegen/ast/function_opening_brace_placement.h>
#include <codegen/ast/function_receiver.h>
#include <codegen/ast/function_requires_placement.h>
#include <codegen/ast/function_template_placement.h>
#include <codegen/ast/indices.h>
#include <codegen/ast/node_id.h>
#include <codegen/ast/node_list_builder.h>
#include <codegen/ast/nodes.h>
#include <codegen/ast/return_syntax.h>
#include <codegen/ast/template_id.h>
#include <codegen/ast/template_parameter_id.h>
#include <codegen/ast/unary_operator.h>
#include <codegen/lowering/indices.h>
#include <codegen/schema/declarations.h>
#include <codegen/schema/fixed_soa_schemas.h>
#include <codegen/schema/function_schemas.h>
#include <codegen/schema/soa_schemas.h>
#include <lispb/indices.h>
#include <lispb/strings.h>

#include <algorithm>
#include <cstddef>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace lispb::codegen::detail {
namespace {

auto unique_fixed_leaf_types(LoweringContext& context, FixedLayoutId layout)
    -> std::vector<CppTypeId> {
    auto const& layout_store{context.ir.lowering.fixed_layouts};
    auto const& leaf_store{context.ir.lowering.fixed_leaves};
    std::vector<CppTypeId> result;
    for (auto const leaf :
         context.ir.lowering.items(layout_store.leaves[layout_store.index(layout)])) {
        auto const type{leaf_store.type[leaf_store.index(leaf)]};
        if (std::ranges::find(result, type) == result.end()) {
            result.push_back(type);
        }
    }
    return result;
}

auto conjunction(CppIR& ir, Expr accumulated, Expr value) -> Expr {
    return accumulated.value ? ir.binary(BinaryOperator::logical_and, accumulated, value, true)
                             : value;
}

auto fixed_trait(CppIR& ir, std::span<CppTypeId const> types, std::string_view trait) -> Expr {
    auto const name{ir.named(trait, {ir.types.dependency(trait, "type_traits")})};
    Expr result{};
    for (auto const type : types) {
        result = conjunction(ir, result, ir.template_id(name, {ir.type_reference(type)}));
    }
    return result;
}

auto fixed_uninitialised_storage_trait(CppIR& ir, std::span<CppTypeId const> types) -> Expr {
    auto const copyable{
        ir.named("std::is_trivially_copyable_v",
                 {ir.types.dependency("std::is_trivially_copyable_v", "type_traits")})};
    auto const destructible{
        ir.named("std::is_trivially_destructible_v",
                 {ir.types.dependency("std::is_trivially_destructible_v", "type_traits")})};
    Expr result{};
    for (auto const type : types) {
        auto const argument{ir.type_reference(type)};
        result = conjunction(ir, result, ir.template_id(copyable, {argument}));
        result = conjunction(ir, result, ir.template_id(destructible, {argument}));
    }
    return result;
}

auto capability_member(LoweringContext& context,
                       std::string_view name,
                       Expr expression,
                       TemplateId template_parameters = {}) -> NodeId {
    auto& ir{context.ir};
    return ir.constexpr_member(
        ir.types.intern("bool"), ir.strings.intern(name), expression, true, template_parameters);
}

auto compact_function_formatting() -> FunctionFormatting {
    return FunctionFormatting{
        .body_layout = FunctionBodyLayout::compact,
        .requires_placement = FunctionRequiresPlacement::trailing_same_line,
        .template_placement = FunctionTemplatePlacement::same_line,
    };
}

auto expanded_constrained_function_formatting() -> FunctionFormatting {
    return FunctionFormatting{
        .opening_brace_placement = FunctionOpeningBracePlacement::separate_line,
    };
}

auto expanded_prefixed_constraint_formatting() -> FunctionFormatting {
    return FunctionFormatting{
        .requires_placement = FunctionRequiresPlacement::before_signature,
    };
}

auto compact_prefixed_constraint_formatting() -> FunctionFormatting {
    return FunctionFormatting{
        .body_layout = FunctionBodyLayout::compact,
        .requires_placement = FunctionRequiresPlacement::before_signature,
    };
}

auto fixed_function(LoweringContext& context,
                    std::string_view name,
                    CppTypeId return_type,
                    std::vector<FunctionParameterId> parameters,
                    std::string body,
                    FunctionReceiver receiver = FunctionReceiver::unqualified,
                    ReturnSyntax return_syntax = ReturnSyntax::direct,
                    TemplateId function_template = {},
                    Expr requires_clause = {},
                    FunctionFormatting formatting = {}) -> FunctionId {
    auto& ir{context.ir};
    auto const created_function{ir.function(name, return_type)};
    ir.set_function_return_type(created_function, return_type, return_syntax);
    ir.set_function_receiver(created_function, receiver);
    ir.set_function_parameters(created_function, parameters);
    ir.set_function_body(created_function, {ir.raw(body)});
    ir.function_flags(created_function).set_is_inline(true);
    ir.function_flags(created_function).set_define_in_header(true);
    ir.set_function_template(created_function, function_template);
    ir.set_function_requires_clause(created_function, requires_clause);
    ir.function_formatting(created_function) = formatting;
    return created_function;
}

auto add_compact_function(LoweringContext& context,
                          NodeListBuilder& nodes,
                          std::string_view name,
                          CppTypeId return_type,
                          std::vector<FunctionParameterId> parameters,
                          std::string body,
                          FunctionReceiver receiver = FunctionReceiver::unqualified,
                          ReturnSyntax return_syntax = ReturnSyntax::direct,
                          TemplateId function_template = {},
                          Expr requirement = {},
                          int trailing_new_lines = 1) -> FunctionId {
    auto const function{fixed_function(context,
                                       name,
                                       return_type,
                                       std::move(parameters),
                                       std::move(body),
                                       receiver,
                                       return_syntax,
                                       function_template,
                                       requirement,
                                       compact_function_formatting())};
    nodes.add(context.ir.header_function(function), trailing_new_lines);
    return function;
}

auto fixed_container_prelude_nodes(LoweringContext& context, DeclarationHandle schema) -> Nodes {
    auto& ir{context.ir};
    NodeListBuilder result{ir};
    result.add(ir.using_declaration(ir.strings.intern("size_type"), ir.types.intern("int32")), 1)
        .add(
            ir.using_declaration(
                ir.strings.intern("View"),
                ir.types.intern(
                    context
                        .optional_text(context.declarations.soas
                                           .view_name[context.declarations.soas.index(schema)])
                        .value_or(context.text(context.declarations.soas
                                                   .name[context.declarations.soas.index(schema)]) +
                                  "View"))),
            1)
        .add(ir.using_declaration(
            ir.strings.intern("ConstView"),
            ir.types.intern(
                context
                    .optional_text(context.declarations.soas
                                       .const_view_name[context.declarations.soas.index(schema)])
                    .value_or(context.text(context.declarations.soas
                                               .name[context.declarations.soas.index(schema)]) +
                              "ConstView"))));
    if (context.declarations.soas.equivalent_type[context.declarations.soas.index(schema)]
            .has_value()) {
        result.new_lines().add(ir.using_declaration(
            ir.strings.intern("equivalent_type"),
            context.resolve(*context.declarations.soas
                                 .equivalent_type[context.declarations.soas.index(schema)])));
    }
    result.new_lines(2).add(ir.constexpr_member(ir.types.intern("size_type"),
                                                ir.strings.intern("capacity_value"),
                                                ir.named("Capacity")),
                            2);
    return result.build();
}

auto fixed_container_capability_nodes(LoweringContext& context, FixedLayoutId layout) -> Nodes {
    auto& layout_store{context.ir.lowering.fixed_layouts};
    auto const layout_store_index{layout_store.index(layout)};
    auto& leaf_store{context.ir.lowering.fixed_leaves};
    auto& ir{context.ir};
    auto const leaf_types{unique_fixed_leaf_types(context, layout)};
    std::vector<TemplateParameterId> template_parameters;
    Expr constructible{};
    auto const trait{ir.named("std::is_constructible_v",
                              {ir.types.dependency("std::is_constructible_v", "type_traits")})};
    auto const leaves{context.ir.lowering.items(layout_store.leaves[layout_store_index])};
    auto const count{leaves.size()};
    for (std::size_t index{}; index < count; ++index) {
        auto const argument_type{"TArg" + std::to_string(index)};
        template_parameters.push_back(ir.type_template_parameter(ir.strings.intern(argument_type)));
        auto const leaf_type{leaf_store.type[leaf_store.index(leaves[index])]};
        constructible =
            conjunction(ir,
                        constructible,
                        ir.template_id(trait,
                                       {ir.type_reference(leaf_type),
                                        ir.type_reference(ir.types.intern(argument_type + "&&"))}));
    }

    NodeListBuilder result{ir};
    result
        .add(capability_member(context,
                               "supports_copy_construction",
                               fixed_trait(ir, leaf_types, "std::is_copy_constructible_v")),
             1)
        .add(capability_member(context,
                               "supports_move_construction",
                               fixed_trait(ir, leaf_types, "std::is_move_constructible_v")),
             1)
        .add(capability_member(context,
                               "supports_nothrow_move_construction",
                               fixed_trait(ir, leaf_types, "std::is_nothrow_move_constructible_v")),
             1)
        .add(capability_member(context,
                               "supports_default_construction",
                               fixed_trait(ir, leaf_types, "std::is_default_constructible_v")),
             1)
        .add(capability_member(context,
                               "supports_uninitialised_storage",
                               fixed_uninitialised_storage_trait(ir, leaf_types)),
             1)
        .add(capability_member(context,
                               "supports_element_construction_from",
                               constructible,
                               ir.template_declaration(template_parameters)),
             2);
    return result.build();
}

auto fixed_container_lifecycle_nodes(LoweringContext& context,
                                     FixedLayoutId,
                                     std::string const& name) -> Nodes {
    auto& ir{context.ir};
    auto const identifier{ir.strings.intern(name)};
    auto const copy_constructible{ir.named("supports_copy_construction")};
    auto const move_constructible{ir.named("supports_move_construction")};
    auto const nothrow_move{ir.named("supports_nothrow_move_construction")};
    auto const const_reference{ir.types.intern(name + " const&")};
    auto const move_reference{ir.types.intern(name + "&&")};
    auto const reference{ir.types.intern(name + "&")};

    auto const default_constructor{ir.constructor(identifier)};
    ir.set_function_noexcept(default_constructor);
    ir.set_function_defaulted(default_constructor);

    auto const copy_constructor{ir.constructor(identifier)};
    ir.set_function_parameters(copy_constructor, {ir.parameter(const_reference, "other")});
    ir.set_function_body(copy_constructor,
                         {ir.raw("for (size_type i{}; i < other.size_; ++i) {\n"
                                 "    storage_.copy_construct_at(size_, other.storage_, i);\n"
                                 "    ++size_;\n"
                                 "}")});
    ir.function_flags(copy_constructor).set_is_inline(true);
    ir.function_flags(copy_constructor).set_define_in_header(true);
    ir.set_function_requires_clause(copy_constructor, copy_constructible);
    ir.function_formatting(copy_constructor) = expanded_constrained_function_formatting();

    auto const deleted_copy_constructor{ir.constructor(identifier)};
    ir.set_function_parameters(deleted_copy_constructor, {ir.parameter(const_reference)});
    ir.set_function_deleted(deleted_copy_constructor);
    ir.set_function_requires_clause(
        deleted_copy_constructor,
        ir.parenthesized(ir.unary(UnaryOperator::logical_not, copy_constructible)));
    ir.function_formatting(deleted_copy_constructor).disposition_placement =
        FunctionDispositionPlacement::next_line;

    auto const move_constructor{ir.constructor(identifier)};
    ir.set_function_parameters(move_constructor, {ir.parameter(move_reference, "other")});
    ir.set_function_body(move_constructor,
                         {ir.raw("for (size_type i{}; i < other.size_; ++i) {\n"
                                 "    storage_.move_construct_at(size_, other.storage_, i);\n"
                                 "    ++size_;\n"
                                 "}\n"
                                 "other.reset();")});
    ir.function_flags(move_constructor).set_is_inline(true);
    ir.function_flags(move_constructor).set_define_in_header(true);
    ir.set_function_conditional_noexcept(move_constructor, nothrow_move);
    ir.set_function_requires_clause(move_constructor, move_constructible);
    ir.function_formatting(move_constructor) = expanded_constrained_function_formatting();

    auto const deleted_move_constructor{ir.constructor(identifier)};
    ir.set_function_parameters(deleted_move_constructor, {ir.parameter(move_reference)});
    ir.set_function_deleted(deleted_move_constructor);
    ir.set_function_requires_clause(
        deleted_move_constructor,
        ir.parenthesized(ir.unary(UnaryOperator::logical_not, move_constructible)));
    ir.function_formatting(deleted_move_constructor).disposition_placement =
        FunctionDispositionPlacement::next_line;

    auto const destructor{ir.destructor(identifier)};
    ir.set_function_body(destructor, {ir.raw("reset();")});
    ir.function_flags(destructor).set_is_inline(true);
    ir.function_flags(destructor).set_define_in_header(true);
    ir.function_formatting(destructor) = compact_function_formatting();

    auto const copy_assignment{fixed_function(context,
                                              "operator=",
                                              reference,
                                              {ir.parameter(const_reference, "other")},
                                              "if (this != std::addressof(other)) {\n"
                                              "    reset();\n"
                                              "    append_from(other.get_const_view());\n"
                                              "}\n"
                                              "return *this;",
                                              FunctionReceiver::unqualified,
                                              ReturnSyntax::trailing,
                                              {},
                                              copy_constructible,
                                              expanded_constrained_function_formatting())};
    auto const deleted_copy_assignment{ir.function("operator=", reference)};
    ir.set_function_return_type(deleted_copy_assignment, reference, ReturnSyntax::trailing);
    ir.set_function_parameters(deleted_copy_assignment, {ir.parameter(const_reference)});
    ir.set_function_deleted(deleted_copy_assignment);
    ir.set_function_requires_clause(
        deleted_copy_assignment,
        ir.parenthesized(ir.unary(UnaryOperator::logical_not, copy_constructible)));
    ir.function_formatting(deleted_copy_assignment).disposition_placement =
        FunctionDispositionPlacement::next_line;

    auto const move_assignment{
        fixed_function(context,
                       "operator=",
                       reference,
                       {ir.parameter(move_reference, "other")},
                       "if (this != std::addressof(other)) {\n"
                       "    reset();\n"
                       "    for (size_type i{}; i < other.size_; ++i) {\n"
                       "        storage_.move_construct_at(size_, other.storage_, i);\n"
                       "        ++size_;\n"
                       "    }\n"
                       "    other.reset();\n"
                       "}\n"
                       "return *this;",
                       FunctionReceiver::unqualified,
                       ReturnSyntax::trailing,
                       {},
                       move_constructible,
                       expanded_constrained_function_formatting())};
    ir.set_function_conditional_noexcept(move_assignment, nothrow_move);
    auto const deleted_move_assignment{ir.function("operator=", reference)};
    ir.set_function_return_type(deleted_move_assignment, reference, ReturnSyntax::trailing);
    ir.set_function_parameters(deleted_move_assignment, {ir.parameter(move_reference)});
    ir.set_function_deleted(deleted_move_assignment);
    ir.set_function_requires_clause(
        deleted_move_assignment,
        ir.parenthesized(ir.unary(UnaryOperator::logical_not, move_constructible)));
    ir.function_formatting(deleted_move_assignment).disposition_placement =
        FunctionDispositionPlacement::next_line;

    return {ir.declaration(default_constructor),      ir.lines(1),
            ir.header_function(copy_constructor),     ir.lines(1),
            ir.declaration(deleted_copy_constructor), ir.lines(1),
            ir.header_function(move_constructor),     ir.lines(1),
            ir.declaration(deleted_move_constructor), ir.lines(1),
            ir.header_function(destructor),           ir.lines(2),
            ir.header_function(copy_assignment),      ir.lines(1),
            ir.declaration(deleted_copy_assignment),  ir.lines(1),
            ir.header_function(move_assignment),      ir.lines(1),
            ir.declaration(deleted_move_assignment),  ir.lines(2)};
}

auto fixed_container_access_nodes(LoweringContext& context, DeclarationHandle schema) -> Nodes {
    auto& ir{context.ir};
    NodeListBuilder result{ir};
    result.add(ir.header_function([&] {
        auto const created_function{ir.function("capacity", "size_type")};
        ir.set_function_body(created_function, {ir.raw("return capacity_value;")});
        ir.set_function_return_type(
            created_function, ir.types.intern("size_type"), ReturnSyntax::trailing);
        ir.set_function_noexcept(created_function);
        ir.set_function_receiver(created_function, FunctionReceiver::static_);
        ir.function_flags(created_function).set_is_constexpr(true);
        ir.function_flags(created_function).set_is_inline(true);
        ir.function_flags(created_function).set_define_in_header(true);
        ir.function_formatting(created_function) = compact_function_formatting();
        return created_function;
    }()),
               1);
    ir.set_function_noexcept(add_compact_function(context,
                                                  result,
                                                  "num",
                                                  ir.types.intern("size_type"),
                                                  {},
                                                  "return size_;",
                                                  FunctionReceiver::const_,
                                                  ReturnSyntax::trailing));
    ir.set_function_noexcept(add_compact_function(context,
                                                  result,
                                                  "is_empty",
                                                  ir.types.intern("bool"),
                                                  {},
                                                  "return size_ == 0;",
                                                  FunctionReceiver::const_,
                                                  ReturnSyntax::trailing));
    ir.set_function_noexcept(add_compact_function(context,
                                                  result,
                                                  "is_full",
                                                  ir.types.intern("bool"),
                                                  {},
                                                  "return size_ == capacity();",
                                                  FunctionReceiver::const_,
                                                  ReturnSyntax::trailing,
                                                  {},
                                                  {},
                                                  2));
    add_compact_function(context,
                         result,
                         "get_view",
                         ir.types.intern("View"),
                         {},
                         "return storage_.get_view(0, size_);",
                         FunctionReceiver::unqualified,
                         ReturnSyntax::trailing);
    add_compact_function(
        context,
        result,
        "get_view",
        ir.types.intern("View"),
        {ir.parameter("size_type const", "offset"), ir.parameter("size_type const", "count")},
        "check_view_range(offset, count); return storage_.get_view(offset, count);",
        FunctionReceiver::unqualified,
        ReturnSyntax::trailing);
    add_compact_function(context,
                         result,
                         "get_view",
                         ir.types.intern("ConstView"),
                         {},
                         "return get_const_view();",
                         FunctionReceiver::const_,
                         ReturnSyntax::trailing);
    add_compact_function(
        context,
        result,
        "get_view",
        ir.types.intern("ConstView"),
        {ir.parameter("size_type const", "offset"), ir.parameter("size_type const", "count")},
        "return get_const_view(offset, count);",
        FunctionReceiver::const_,
        ReturnSyntax::trailing);
    add_compact_function(context,
                         result,
                         "get_const_view",
                         ir.types.intern("ConstView"),
                         {},
                         "return storage_.get_const_view(0, size_);",
                         FunctionReceiver::const_,
                         ReturnSyntax::trailing);
    add_compact_function(
        context,
        result,
        "get_const_view",
        ir.types.intern("ConstView"),
        {ir.parameter("size_type const", "offset"), ir.parameter("size_type const", "count")},
        "check_view_range(offset, count); return storage_.get_const_view(offset, count);",
        FunctionReceiver::const_,
        ReturnSyntax::trailing);
    add_compact_function(
        context,
        result,
        "slice",
        ir.types.intern("View"),
        {ir.parameter("size_type const", "offset"), ir.parameter("size_type const", "count")},
        "return get_view(offset, count);",
        FunctionReceiver::unqualified,
        ReturnSyntax::trailing);
    add_compact_function(context,
                         result,
                         "left",
                         ir.types.intern("View"),
                         {ir.parameter("size_type const", "count")},
                         "return slice(0, count);",
                         FunctionReceiver::unqualified,
                         ReturnSyntax::trailing);
    add_compact_function(context,
                         result,
                         "right",
                         ir.types.intern("View"),
                         {ir.parameter("size_type const", "count")},
                         "return slice(size_ - count, count);",
                         FunctionReceiver::unqualified,
                         ReturnSyntax::trailing);
    add_compact_function(
        context,
        result,
        "slice",
        ir.types.intern("ConstView"),
        {ir.parameter("size_type const", "offset"), ir.parameter("size_type const", "count")},
        "return get_const_view(offset, count);",
        FunctionReceiver::const_,
        ReturnSyntax::trailing);
    add_compact_function(context,
                         result,
                         "left",
                         ir.types.intern("ConstView"),
                         {ir.parameter("size_type const", "count")},
                         "return slice(0, count);",
                         FunctionReceiver::const_,
                         ReturnSyntax::trailing);
    add_compact_function(context,
                         result,
                         "right",
                         ir.types.intern("ConstView"),
                         {ir.parameter("size_type const", "count")},
                         "return slice(size_ - count, count);",
                         FunctionReceiver::const_,
                         ReturnSyntax::trailing,
                         {},
                         {},
                         2);
    add_compact_function(
        context,
        result,
        "apply_arrays",
        ir.types.intern("decltype(auto)"),
        {ir.parameter("TFunc&&", "func")},
        "auto view{get_view()}; return view.apply_arrays(std::forward<TFunc>(func));",
        FunctionReceiver::unqualified,
        ReturnSyntax::trailing,
        ir.template_declaration({ir.type_template_parameter(ir.strings.intern("TFunc"))}));
    add_compact_function(
        context,
        result,
        "apply_arrays",
        ir.types.intern("decltype(auto)"),
        {ir.parameter("TFunc&&", "func")},
        "auto view{get_const_view()}; return view.apply_arrays(std::forward<TFunc>(func));",
        FunctionReceiver::const_,
        ReturnSyntax::trailing,
        ir.template_declaration({ir.type_template_parameter(ir.strings.intern("TFunc"))}),
        {},
        2);
    if (context.declarations.soas.equivalent_type[context.declarations.soas.index(schema)]
            .has_value()) {
        result.add(ir.header_function(fixed_function(context,
                                                     "operator[]",
                                                     ir.types.intern("equivalent_type"),
                                                     {ir.parameter("size_type const", "index")},
                                                     "return get_const_view()[index];",
                                                     FunctionReceiver::const_,
                                                     ReturnSyntax::trailing)),
                   1);
        result.add(ir.header_function(fixed_function(context,
                                                     "at",
                                                     ir.types.intern("equivalent_type"),
                                                     {ir.parameter("size_type const", "index")},
                                                     "check_index(index);\nreturn (*this)[index];",
                                                     FunctionReceiver::const_,
                                                     ReturnSyntax::trailing)));
    }
    result.new_lines(2);
    return result.build();
}

auto fixed_container_set_nodes(LoweringContext& context,
                               DeclarationHandle schema,
                               CppTypeId equivalent_set_type) -> Nodes {
    auto& ir{context.ir};
    auto const members{resolve_members(context, schema)};
    std::vector<FunctionId> setters;
    if (auto set{soa_set_function(context, schema, members, false)}; set.has_value()) {
        setters.push_back(std::move(*set));
    } else {
        for (auto const function_id : context.declarations.items(
                 context.declarations.soas
                     .mutable_view_functions[context.declarations.soas.index(schema)])) {
            auto const function{function_id};
            if (context.declarations.strings().view(
                    context.declarations.functions
                        .name[context.declarations.functions.index(function)]) == "set") {
                setters.push_back(lower_schema_function(context, function));
            }
        }
    }

    NodeListBuilder result{ir};
    for (auto& setter : setters) {
        std::vector<std::string> arguments;
        arguments.reserve(ir.function_parameters(setter).size());
        for (auto const parameter : ir.function_parameters(setter)) {
            arguments.emplace_back(ir.strings.view(ir.parameter_name(parameter)));
        }
        ir.set_function_body(setter,
                             {ir.expression_statement(ir.raw_expression(
                                 "get_view().set(" + join(arguments, ", ") + ")"))});
        switch (ir.function_receiver(setter)) {
            case FunctionReceiver::const_:
                ir.set_function_receiver(setter, FunctionReceiver::unqualified);
                break;
            case FunctionReceiver::const_lvalue:
                ir.set_function_receiver(setter, FunctionReceiver::lvalue);
                break;
            case FunctionReceiver::const_rvalue:
                ir.set_function_receiver(setter, FunctionReceiver::rvalue);
                break;
            default:
                break;
        }
        ir.function_formatting(setter) = compact_function_formatting();
        result.add(ir.header_function(setter), 1);
    }
    if (equivalent_set_type.value != 0) {
        result.add(ir.header_function([&] {
            auto const created_function{ir.function("set", "void")};
            ir.set_function_parameters(
                created_function,
                {ir.parameter("int32 const", "i"),
                 ir.parameter(qualify(ir, equivalent_set_type, " const"), "value")});
            ir.set_function_body(created_function,
                                 {ir.expression_statement(
                                     ir.call(ir.member_access(ir.call(ir.named("get_view")), "set"),
                                             {ir.named("i"), ir.named("value")}))});
            ir.function_flags(created_function).set_is_inline(true);
            ir.function_flags(created_function).set_define_in_header(true);
            ir.function_formatting(created_function) = compact_function_formatting();
            return created_function;
        }()),
                   1);
    }
    if (!setters.empty()) {
        result.new_lines();
    }
    return result.build();
}

auto fixed_container_construction_nodes(LoweringContext& context, FixedLayoutId layout) -> Nodes {
    auto& layout_store{context.ir.lowering.fixed_layouts};
    auto const layout_store_index{layout_store.index(layout)};
    auto& ir{context.ir};
    std::vector<TemplateParameterId> template_parameters;
    std::vector<FunctionParameterId> parameters;
    std::vector<std::string> forwarded;
    std::vector<Expr> argument_types;
    for (std::size_t index{0};
         index < context.ir.lowering.items(layout_store.leaves[layout_store_index]).size();
         ++index) {
        auto const argument{fixed_leaf_argument(
            context, context.ir.lowering.items(layout_store.leaves[layout_store_index])[index])};
        auto const index_text{std::to_string(index)};
        template_parameters.push_back(
            ir.type_template_parameter(ir.strings.intern("TArg" + index_text)));
        argument_types.push_back(ir.type_reference(ir.types.intern("TArg" + index_text)));
        parameters.push_back(ir.parameter("TArg" + index_text + "&&", "new_" + argument));
        forwarded.push_back("std::forward<TArg" + index_text + ">(new_" + argument + ")");
    }
    auto const function_template{ir.template_declaration(template_parameters)};
    auto const forwarded_values{join(forwarded, ", ")};
    auto const constructible{
        ir.template_id(ir.named("supports_element_construction_from"), argument_types)};
    NodeListBuilder result{ir};
    result
        .add(ir.header_function(fixed_function(context,
                                               "emplace_back",
                                               ir.types.intern("size_type"),
                                               parameters,
                                               "check_has_sufficient_capacity(1);\n"
                                               "auto const index{size_};\n"
                                               "storage_.construct_at(index, " +
                                                   forwarded_values +
                                                   ");\n"
                                                   "++size_;\n"
                                                   "return index;",
                                               FunctionReceiver::unqualified,
                                               ReturnSyntax::trailing,
                                               function_template,
                                               constructible,
                                               expanded_prefixed_constraint_formatting())),
             1)
        .add(ir.header_function(fixed_function(context,
                                               "add",
                                               ir.types.intern("size_type"),
                                               parameters,
                                               "return emplace_back(" + forwarded_values + ");",
                                               FunctionReceiver::unqualified,
                                               ReturnSyntax::trailing,
                                               function_template,
                                               constructible,
                                               compact_prefixed_constraint_formatting())),
             2);
    add_compact_function(context,
                         result,
                         "add_defaulted",
                         ir.types.intern("void"),
                         {ir.parameter("size_type const", "count", ir.store_text("1"))},
                         "check_has_sufficient_capacity(count); for (size_type i{}; i < count; "
                         "++i) { storage_.default_construct_at(size_); ++size_; }",
                         FunctionReceiver::unqualified,
                         ReturnSyntax::direct,
                         {},
                         ir.named("supports_default_construction"));
    add_compact_function(
        context,
        result,
        "set_num",
        ir.types.intern("void"),
        {ir.parameter("size_type const", "new_size")},
        "check(new_size >= 0); check(new_size <= capacity()); if (new_size < size_) { "
        "destroy_from(new_size); return; } add_defaulted(new_size - size_);",
        FunctionReceiver::unqualified,
        ReturnSyntax::direct,
        {},
        ir.named("supports_default_construction"));
    add_compact_function(
        context,
        result,
        "set_num",
        ir.types.intern("void"),
        {ir.parameter("size_type const", "new_size"), ir.parameter("EAllowShrinking const")},
        "set_num(new_size);",
        FunctionReceiver::unqualified,
        ReturnSyntax::direct,
        {},
        ir.named("supports_default_construction"));
    add_compact_function(context,
                         result,
                         "capacity_view",
                         ir.types.intern("View"),
                         {},
                         "return storage_.get_view(0, capacity());",
                         FunctionReceiver::unqualified,
                         ReturnSyntax::trailing,
                         {},
                         ir.named("supports_uninitialised_storage"));
    add_compact_function(context,
                         result,
                         "set_num_uninitialised",
                         ir.types.intern("void"),
                         {ir.parameter("size_type const", "new_size")},
                         "check(new_size >= 0); check(new_size <= capacity()); size_ = new_size;",
                         FunctionReceiver::unqualified,
                         ReturnSyntax::direct,
                         {},
                         ir.named("supports_uninitialised_storage"));
    add_compact_function(context,
                         result,
                         "add_uninitialised",
                         ir.types.intern("void"),
                         {ir.parameter("size_type const", "count")},
                         "check_has_sufficient_capacity(count); size_ += count;",
                         FunctionReceiver::unqualified,
                         ReturnSyntax::direct,
                         {},
                         ir.named("supports_uninitialised_storage"),
                         2);
    return result.build();
}

auto fixed_container_mutation_nodes(LoweringContext& context) -> Nodes {
    auto& ir{context.ir};
    NodeListBuilder result{ir};
    add_compact_function(context,
                         result,
                         "pop",
                         ir.types.intern("void"),
                         {},
                         "check(!is_empty()); --size_; storage_.destroy_at(size_);");
    ir.set_function_noexcept(add_compact_function(
        context, result, "reset", ir.types.intern("void"), {}, "destroy_from(0);"));
    ir.set_function_noexcept(
        add_compact_function(context, result, "empty", ir.types.intern("void"), {}, "reset();"));
    add_compact_function(context,
                         result,
                         "reserve",
                         ir.types.intern("void"),
                         {ir.parameter("size_type const", "requested_capacity")},
                         "check(requested_capacity >= 0); check(requested_capacity <= capacity());",
                         FunctionReceiver::const_);
    result.add(ir.header_function(fixed_function(
                   context,
                   "remove_at_swap",
                   ir.types.intern("void"),
                   {ir.parameter("size_type const", "index"),
                    ir.parameter("size_type const", "count"),
                    ir.parameter("EAllowShrinking const")},
                   "check(index >= 0); check(count >= 0); check(index + count <= size_);\nif "
                   "(count == 0) { return; }\nauto const available_tail{size_ - (index + "
                   "count)};\nauto const move_count{count < available_tail ? count : "
                   "available_tail};\nauto const source_begin{size_ - move_count};\nfor "
                   "(size_type i{}; i < move_count; ++i) { storage_.move_assign_at(index + i, "
                   "storage_, source_begin + i); }\ndestroy_from(size_ - count);")),
               1);
    add_compact_function(
        context,
        result,
        "copy_element",
        ir.types.intern("void"),
        {ir.parameter("size_type const", "dst_index"),
         ir.parameter("Other const&", "other"),
         ir.parameter("size_type const", "source_index")},
        "check_index(dst_index); auto const source{other.get_const_view()}; "
        "check(source_index >= 0); check(source_index < source.num()); "
        "storage_.copy_assign_from_view_at(dst_index, source, source_index);",
        FunctionReceiver::unqualified,
        ReturnSyntax::direct,
        ir.template_declaration({ir.type_template_parameter(ir.strings.intern("Other"))}));
    add_compact_function(
        context,
        result,
        "copy_elements",
        ir.types.intern("void"),
        {ir.parameter("size_type const", "dst_index"),
         ir.parameter("Other const&", "other"),
         ir.parameter("size_type const", "source_index"),
         ir.parameter("size_type const", "count")},
        "check(dst_index >= 0); check(source_index >= 0); check(count >= 0); check(dst_index + "
        "count <= size_); auto const source{other.get_const_view()}; check(source_index + count <= "
        "source.num()); for (size_type i{}; i < count; ++i) { "
        "storage_.copy_assign_from_view_at(dst_index + i, source, source_index + i); }",
        FunctionReceiver::unqualified,
        ReturnSyntax::direct,
        ir.template_declaration({ir.type_template_parameter(ir.strings.intern("Other"))}));
    add_compact_function(
        context,
        result,
        "append_from",
        ir.types.intern("void"),
        {ir.parameter("Other const&", "other")},
        "auto const source{other.get_const_view()}; auto const count{source.num()}; "
        "check_has_sufficient_capacity(count); for (size_type i{}; i < count; ++i) { "
        "storage_.construct_from_view_at(size_, source, i); ++size_; }",
        FunctionReceiver::unqualified,
        ReturnSyntax::direct,
        ir.template_declaration({ir.type_template_parameter(ir.strings.intern("Other"))}),
        {},
        2);
    return result.build();
}

auto fixed_container_private_nodes(LoweringContext& context, DeclarationHandle schema) -> Nodes {
    auto& ir{context.ir};
    NodeListBuilder result{ir};
    result.add(ir.access(Access::private_), 1);
    add_compact_function(context,
                         result,
                         "check_index",
                         ir.types.intern("void"),
                         {ir.parameter("size_type const", "index")},
                         "check(index >= 0); check(index < size_);",
                         FunctionReceiver::const_);
    add_compact_function(
        context,
        result,
        "check_view_range",
        ir.types.intern("void"),
        {ir.parameter("size_type const", "offset"), ir.parameter("size_type const", "count")},
        "check(offset >= 0); check(count >= 0); check(offset + count <= size_);",
        FunctionReceiver::const_);
    add_compact_function(context,
                         result,
                         "check_has_sufficient_capacity",
                         ir.types.intern("void"),
                         {ir.parameter("size_type const", "count")},
                         "check(count >= 0); check(count <= capacity() - size_);",
                         FunctionReceiver::const_);
    ir.set_function_noexcept(add_compact_function(
        context,
        result,
        "destroy_from",
        ir.types.intern("void"),
        {ir.parameter("size_type const", "first_index")},
        "for (size_type i{size_}; i > first_index; --i) { storage_.destroy_at(i - "
        "1); } size_ = first_index;",
        FunctionReceiver::unqualified,
        ReturnSyntax::direct,
        {},
        {},
        2));
    result
        .add(ir.instance_member(
                 ir.types.intern(
                     context.text(context.declarations.fixed_soas
                                      .storage_name[context.declarations.fixed_soas.index(
                                          *context.declarations.soas
                                               .fixed[context.declarations.soas.index(schema)])]) +
                     "<Capacity>"),
                 ir.strings.intern("storage_")),
             1)
        .add(ir.instance_member(
            ir.types.intern("size_type"), ir.strings.intern("size_"), ir.init_list({})));
    return result.build();
}

auto fixed_container_dependencies(LoweringContext& context, FixedLayoutId layout)
    -> std::vector<TypeDependencyId> {
    auto& layout_store{context.ir.lowering.fixed_layouts};
    auto& leaf_store{context.ir.lowering.fixed_leaves};
    auto& ir{context.ir};
    std::vector<TypeDependencyId> result{
        ir.types.dependency("ioj::TFixedStorage", "ioj/fixed_storage.h"),
        ir.types.dependency("TArrayView", "Containers/ArrayView.h"),
        ir.types.dependency("std::forward", "utility"),
        ir.types.dependency("MoveTemp", "Templates/UnrealTemplate.h"),
        ir.types.dependency("EAllowShrinking", "Containers/AllowShrinking.h"),
        ir.types.dependency("check", "CoreMinimal.h"),
        ir.types.dependency("std::addressof", "memory"),
        ir.types.dependency("std::is_constructible_v", "type_traits")};
    for (auto const leaf :
         context.ir.lowering.items(layout_store.leaves[layout_store.index(layout)])) {
        result.insert(result.end(),
                      ir.types.dependencies(leaf_store.type[leaf_store.index(leaf)]).begin(),
                      ir.types.dependencies(leaf_store.type[leaf_store.index(leaf)]).end());
    }
    return result;
}

} // namespace

auto fixed_container_node(LoweringContext& context,
                          FixedLayoutId layout,
                          std::string const& name,
                          CppTypeId const equivalent_set_type) -> NodeId {
    auto& layout_store{context.ir.lowering.fixed_layouts};
    auto& ir{context.ir};
    auto const& schema{layout_store.schema[layout_store.index(layout)]};
    NodeListBuilder children{ir};
    children.append(fixed_container_prelude_nodes(context, schema))
        .append(fixed_container_capability_nodes(context, layout))
        .append(fixed_container_lifecycle_nodes(context, layout, name))
        .append(fixed_container_access_nodes(context, schema))
        .append(fixed_container_set_nodes(context, schema, equivalent_set_type))
        .append(fixed_container_construction_nodes(context, layout))
        .append(fixed_container_mutation_nodes(context))
        .append(fixed_container_private_nodes(context, schema));

    auto const created_record{
        ir.record(ir.strings.intern(name), children.build(), RecordKind::struct_)};
    ir.set_record_template(created_record,
                           ir.template_declaration({ir.value_template_parameter(
                               ir.types.intern("int32"), ir.strings.intern("Capacity"))}));
    ir.set_record_requires_clause(
        created_record,
        ir.parenthesized(
            ir.binary(BinaryOperator::greater_equal, ir.named("Capacity"), ir.literal("0"))));
    ir.set_record_dependencies(created_record, fixed_container_dependencies(context, layout));
    return created_record;
}

} // namespace lispb::codegen::detail
