#include "string_enum_parser.h"

#include <ioj/ascii.h>

#include <codegen/ast/binary_operator.h>
#include <codegen/ast/cpp_ir_builder.h>
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
namespace {

class StringEnumParserLowering : public CppIRBuilder {
    struct EntryView {
        std::string_view key;
        Expr value;
    };
    using Entries = std::vector<EntryView>;
    static constexpr std::size_t linear_limit{4};


  public:
    explicit StringEnumParserLowering(CppIR& ir)
        : CppIRBuilder{ir} {}
    auto lower_string_enum_parser(std::string_view const function_name,
                                  std::string_view const enum_type,
                                  std::string_view const parameter_name,
                                  std::span<StringEnumEntry const> const entries,
                                  bool const indexed) -> NodeId {
        auto const function_id{function(function_name, "auto")};
        set_function_return_type(
            function_id,
            ir_.types.template_type(ir_.types.std.optional, {ir_.types.named(enum_type)}),
            ReturnSyntax::trailing);
        set_function_parameters(
            function_id,
            {parameter(ir_.types.decorate(ir_.types.std.string_view, "", " const"),
                       parameter_name)});
        auto& flags{function_flags(function_id)};
        flags.set_define_in_header(true);
        flags.set_is_constexpr(true);
        flags.set_is_nodiscard(true);
        set_function_noexcept(function_id);
        auto const input{named(parameter_name)};
        set_function_body(function_id,
                          indexed && entries.size() <= linear_limit
                              ? indexed_parser_body(input, enum_type, entries)
                              : parser_body(input, entries));
        return header_function(function_id);
    }
  private:
    auto character_literal(unsigned char const character) -> Expr {
        auto const value{static_cast<char>(character)};
        if (ioj::needs_char_literal_escape(value)) {
            return literal(std::format("'\\{}'", value));
        }
        if (ioj::isprint(value)) {
            return literal(std::format("'{}'", value));
        }

        // Keep the byte value positive regardless of whether plain char is signed.
        return literal(std::format("U'\\x{:02x}'", character));
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
    auto comparisons(Expr const input, Entries const& entries, std::size_t const prefix_size = 0)
        -> Nodes {
        NodeListBuilder body{ir_};
        if (entries.empty()) {
            body.add(expr_stmt(static_cast_expr(ir_.types.std.void_, input)));
        }
        for (auto const entry : entries) {
            body.add(if_stmt(
                binary(BinaryOperator::equal, input, string_literal(entry.key.substr(prefix_size))),
                block({return_stmt(entry.value)})));
        }
        body.add(return_stmt(type_reference(ir_.types.named("std::nullopt", NameOrigin::global))));
        return body.build();
    }
    auto length_bucket(Expr const input,
                       std::size_t const length,
                       Entries const& entries,
                       std::size_t const prefix_size,
                       std::size_t position = 0) -> Nodes {
        if (length == 0 || entries.size() <= linear_limit) {
            return comparisons(input, entries, prefix_size);
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
            return comparisons(input, entries, prefix_size);
        }

        std::map<unsigned char, Entries> characters;
        for (auto const entry : entries) {
            characters[static_cast<unsigned char>(entry.key[prefix_size + position])].push_back(
                entry);
        }

        std::vector<SwitchCaseId> cases;
        for (auto const& [character, group] : characters) {
            cases.push_back(
                switch_case(character_literal(character),
                            block(length_bucket(input, length, group, prefix_size, position + 1))));
        }
        cases.push_back(switch_case({},
                                    block({return_stmt(type_reference(
                                        ir_.types.named("std::nullopt", NameOrigin::global)))})));
        auto const selected_character{static_cast_expr(
            ir_.types.std.unsigned_char, subscript(input, literal(std::to_string(position))))};
        return {switch_stmt(selected_character, cases)};
    }
    auto parser_body(Expr const input, std::span<StringEnumEntry const> const entries) -> Nodes {
        Entries all;
        all.reserve(entries.size());
        for (auto const& entry : entries) {
            all.push_back({entry.key, entry.value});
        }
        if (entries.size() <= linear_limit) {
            return comparisons(input, all);
        }

        NodeListBuilder body{ir_};
        auto const prefix{common_prefix(all)};
        auto suffix{input};
        if (!prefix.empty()) {
            body.add(
                if_stmt(unary(UnaryOperator::logical_not,
                              call(member_access(input, "starts_with"), {string_literal(prefix)})),
                        block({return_stmt(
                            type_reference(ir_.types.named("std::nullopt", NameOrigin::global)))})),
                2);
            body.add(variable("auto const",
                              "suffix",
                              call(member_access(input, "substr"),
                                   {literal(std::to_string(prefix.size()))})),
                     2);
            suffix = named("suffix");
        }

        std::map<std::size_t, Entries> lengths;
        for (auto const entry : all) {
            lengths[entry.key.size() - prefix.size()].push_back(entry);
        }
        std::vector<SwitchCaseId> cases;
        for (auto const& [length, group] : lengths) {
            cases.push_back(
                switch_case(literal(std::to_string(length)),
                            block(length_bucket(suffix, length, group, prefix.size()))));
        }
        cases.push_back(switch_case({},
                                    block({return_stmt(type_reference(
                                        ir_.types.named("std::nullopt", NameOrigin::global)))})));
        body.add(switch_stmt(call(member_access(suffix, "size")), cases));
        return body.build();
    }
    auto indexed_parser_body(Expr input,
                             std::string_view enum_type,
                             std::span<StringEnumEntry const> entries) -> Nodes {
        auto const lookup{
            template_id(type_reference(ir_.types.named(
                            "ioj::try_parse_enum",
                            NameOrigin::global,
                            {ir_.types.dependency("ioj::try_parse_enum", "ioj/parse_enum.h")})),
                        {type_reference(ir_.types.named(enum_type))})};
        std::vector<Expr> names;
        names.reserve(entries.size());
        for (auto const& entry : entries) {
            names.push_back(string_literal(entry.key));
        }
        auto const array_type{ir_.types.template_type(
            ir_.types.std.array,
            {ir_.types.std.string_view, ir_.types.intern(std::to_string(names.size()))})};
        return {
            variable(ir_.types.decorate(array_type, "", " constexpr"), "names", init_list(names)),
            return_stmt(call(lookup, {input, named("names")}))};
    }
};

} // namespace

auto lower_string_enum_parser(CppIR& ir,
                              std::string_view const function_name,
                              std::string_view const enum_type,
                              std::string_view const parameter_name,
                              std::span<StringEnumEntry const> const entries,
                              bool const indexed) -> NodeId {
    return StringEnumParserLowering{ir}.lower_string_enum_parser(
        function_name, enum_type, parameter_name, entries, indexed);
}

} // namespace lispb::codegen::detail
