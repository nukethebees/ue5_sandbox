#include "string_enum_parser.h"

#include <codegen/ast/binary_operator.h>
#include <codegen/ast/cpp_ir.h>
#include <codegen/ast/cpp_types.h>
#include <codegen/ast/function_flags.h>
#include <codegen/ast/node_list_builder.h>
#include <codegen/ast/return_syntax.h>
#include <lispb/strings.h>

#include <format>
#include <map>
#include <vector>

namespace lispb::codegen::detail {
namespace string_enum_parser {
using Entries = std::vector<StringEnumEntry const*>;
inline constexpr std::size_t linear_limit{4};

auto character_literal(CppIR& ir, unsigned char const character) -> Expr {
    if (character == '\'' || character == '\\') {
        return ir.literal(std::format("'\\{}'", static_cast<char>(character)));
    }
    if (character >= ' ' && character <= '~') {
        return ir.literal(std::format("'{}'", static_cast<char>(character)));
    }

    // Keep the byte value positive regardless of whether plain char is signed.
    return ir.literal(std::format("U'\\x{:02x}'", character));
}

auto comparisons(CppIR& ir, Expr const input, Entries const& entries) -> Nodes {
    NodeListBuilder body{ir};
    if (entries.empty()) {
        body.add(ir.expression_statement(ir.static_cast_expr(ir.types.intern("void"), input)));
    }
    for (auto const* const entry : entries) {
        body.add(
            ir.if_statement(ir.binary(BinaryOperator::equal, input, ir.string_literal(entry->key)),
                            ir.block({ir.return_statement(ir.named(entry->value))})));
    }
    body.add(ir.return_statement(ir.named("std::nullopt")));
    return body.build();
}

auto length_bucket(CppIR& ir, Expr const input, std::size_t const length, Entries const& entries)
    -> Nodes {
    if (length == 0 || entries.size() <= linear_limit) {
        return comparisons(ir, input, entries);
    }

    std::map<unsigned char, Entries> leading_characters;
    for (auto const* const entry : entries) {
        leading_characters[static_cast<unsigned char>(entry->key.front())].push_back(entry);
    }
    if (leading_characters.size() == 1) {
        return comparisons(ir, input, entries);
    }

    std::vector<SwitchCaseId> cases;
    for (auto const& [character, group] : leading_characters) {
        cases.push_back(ir.switch_case(character_literal(ir, character),
                                       ir.block(comparisons(ir, input, group))));
    }
    cases.push_back(ir.switch_case({}, ir.block({ir.return_statement(ir.named("std::nullopt"))})));
    auto const first_character{ir.static_cast_expr(ir.types.intern("unsigned char"),
                                                   ir.call(ir.member_access(input, "front")))};
    return {ir.switch_statement(first_character, cases)};
}

auto parser_body(CppIR& ir, Expr const input, std::span<StringEnumEntry const> const entries)
    -> Nodes {
    Entries all;
    all.reserve(entries.size());
    for (auto const& entry : entries) {
        all.push_back(&entry);
    }
    if (entries.size() <= linear_limit) {
        return comparisons(ir, input, all);
    }

    std::map<std::size_t, Entries> lengths;
    for (auto const* const entry : all) {
        lengths[entry->key.size()].push_back(entry);
    }
    std::vector<SwitchCaseId> cases;
    for (auto const& [length, group] : lengths) {
        cases.push_back(ir.switch_case(ir.literal(std::to_string(length)),
                                       ir.block(length_bucket(ir, input, length, group))));
    }
    cases.push_back(ir.switch_case({}, ir.block({ir.return_statement(ir.named("std::nullopt"))})));
    return {ir.switch_statement(ir.call(ir.member_access(input, "size")), cases)};
}
}

auto lower_string_enum_parser(CppIR& ir,
                              std::string_view const function_name,
                              std::string_view const enum_type,
                              std::string_view const parameter_name,
                              std::span<StringEnumEntry const> const entries) -> NodeId {
    auto const function{ir.function(ir.strings.intern(function_name), ir.types.intern("auto"))};
    ir.set_function_return_type(function,
                                ir.types.intern("std::optional<" + std::string{enum_type} + ">",
                                                {ir.types.dependency("std::optional", "optional")}),
                                ReturnSyntax::trailing);
    ir.set_function_parameters(
        function,
        {ir.parameter(ir.types.intern("std::string_view const",
                                      {ir.types.dependency("std::string_view", "string_view")}),
                      ir.strings.intern(parameter_name))});
    auto& flags{ir.function_flags(function)};
    flags.set_define_in_header(true);
    flags.set_is_constexpr(true);
    flags.set_is_nodiscard(true);
    ir.set_function_noexcept(function);
    ir.set_function_body(function,
                         string_enum_parser::parser_body(ir, ir.named(parameter_name), entries));
    return ir.header_function(function);
}
}
