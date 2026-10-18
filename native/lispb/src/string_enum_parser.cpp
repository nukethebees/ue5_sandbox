#include "string_enum_parser.h"

#include <ioj/ascii.h>

#include <codegen/ast/binary_operator.h>
#include <codegen/ast/cpp_ir.h>
#include <codegen/ast/cpp_types.h>
#include <codegen/ast/function_flags.h>
#include <codegen/ast/node_list_builder.h>
#include <codegen/ast/return_syntax.h>
#include <codegen/ast/unary_operator.h>
#include <lispb/strings.h>

#include <algorithm>
#include <format>
#include <map>
#include <vector>

namespace lispb::codegen::detail {
namespace string_enum_parser {
struct EntryView {
    std::string_view key;
    std::string_view value;
};
using Entries = std::vector<EntryView>;
inline constexpr std::size_t linear_limit{4};

auto character_literal(CppIR& ir, unsigned char const character) -> Expr {
    auto const value{static_cast<char>(character)};
    if (ioj::needs_char_literal_escape(value)) {
        return ir.literal(std::format("'\\{}'", value));
    }
    if (ioj::isprint(value)) {
        return ir.literal(std::format("'{}'", value));
    }

    // Keep the byte value positive regardless of whether plain char is signed.
    return ir.literal(std::format("U'\\x{:02x}'", character));
}

auto common_prefix(Entries const& entries) -> std::string_view {
    auto prefix{entries.front().key};
    for (auto const entry : entries) {
        auto const count{std::min(prefix.size(), entry.key.size())};
        std::size_t matched{};
        while (matched < count && prefix[matched] == entry.key[matched]) {
            ++matched;
        }
        prefix = prefix.substr(0, matched);
    }
    return prefix;
}

auto comparisons(CppIR& ir,
                 Expr const input,
                 Entries const& entries,
                 std::size_t const prefix_size = 0) -> Nodes {
    NodeListBuilder body{ir};
    if (entries.empty()) {
        body.add(ir.expression_statement(ir.static_cast_expr(ir.types.intern("void"), input)));
    }
    for (auto const entry : entries) {
        body.add(ir.if_statement(ir.binary(BinaryOperator::equal,
                                           input,
                                           ir.string_literal(entry.key.substr(prefix_size))),
                                 ir.block({ir.return_statement(ir.named(entry.value))})));
    }
    body.add(ir.return_statement(ir.named("std::nullopt")));
    return body.build();
}

auto length_bucket(CppIR& ir,
                   Expr const input,
                   std::size_t const length,
                   Entries const& entries,
                   std::size_t const prefix_size,
                   std::size_t position = 0) -> Nodes {
    if (length == 0 || entries.size() <= linear_limit) {
        return comparisons(ir, input, entries, prefix_size);
    }

    for (; position < length; ++position) {
        auto const character{entries.front().key[prefix_size + position]};
        if (std::ranges::any_of(entries, [&](EntryView const entry) {
                return entry.key[prefix_size + position] != character;
            })) {
            break;
        }
    }
    if (position == length) {
        return comparisons(ir, input, entries, prefix_size);
    }

    std::map<unsigned char, Entries> characters;
    for (auto const entry : entries) {
        characters[static_cast<unsigned char>(entry.key[prefix_size + position])].push_back(entry);
    }

    std::vector<SwitchCaseId> cases;
    for (auto const& [character, group] : characters) {
        cases.push_back(ir.switch_case(
            character_literal(ir, character),
            ir.block(length_bucket(ir, input, length, group, prefix_size, position + 1))));
    }
    cases.push_back(ir.switch_case({}, ir.block({ir.return_statement(ir.named("std::nullopt"))})));
    auto const selected_character{
        ir.static_cast_expr(ir.types.intern("unsigned char"),
                            ir.subscript(input, ir.literal(std::to_string(position))))};
    return {ir.switch_statement(selected_character, cases)};
}

auto parser_body(CppIR& ir, Expr const input, std::span<StringEnumEntry const> const entries)
    -> Nodes {
    Entries all;
    all.reserve(entries.size());
    for (auto const& entry : entries) {
        all.push_back({entry.key, entry.value});
    }
    if (entries.size() <= linear_limit) {
        return comparisons(ir, input, all);
    }

    NodeListBuilder body{ir};
    auto const prefix{common_prefix(all)};
    auto suffix{input};
    if (!prefix.empty()) {
        body.add(ir.if_statement(ir.unary(UnaryOperator::logical_not,
                                          ir.call(ir.member_access(input, "starts_with"),
                                                  {ir.string_literal(prefix)})),
                                 ir.block({ir.return_statement(ir.named("std::nullopt"))})),
                 2);
        body.add(ir.variable(ir.types.intern("auto const"),
                             ir.strings.intern("suffix"),
                             ir.call(ir.member_access(input, "substr"),
                                     {ir.literal(std::to_string(prefix.size()))})),
                 2);
        suffix = ir.named("suffix");
    }

    std::map<std::size_t, Entries> lengths;
    for (auto const entry : all) {
        lengths[entry.key.size() - prefix.size()].push_back(entry);
    }
    std::vector<SwitchCaseId> cases;
    for (auto const& [length, group] : lengths) {
        cases.push_back(
            ir.switch_case(ir.literal(std::to_string(length)),
                           ir.block(length_bucket(ir, suffix, length, group, prefix.size()))));
    }
    cases.push_back(ir.switch_case({}, ir.block({ir.return_statement(ir.named("std::nullopt"))})));
    body.add(ir.switch_statement(ir.call(ir.member_access(suffix, "size")), cases));
    return body.build();
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
