#include "string_enum_parser.h"

#include "lowering_context.h"
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
        auto const input{named(parameter_name)};
        FunctionDefinition const function{
            .name = this->name(function_name),
            .return_type = template_type(std_types().optional, {named_type(enum_type)}),
            .return_syntax = ReturnSyntax::trailing,
            .parameters = {parameter(decorate_type(std_types().string_view, "", " const"),
                                     parameter_name)},
            .implementation = {.kind = FunctionImplementationKind::body,
                               .body = indexed && entries.size() <= linear_limit
                                         ? indexed_parser_body(input, enum_type, entries)
                                         : parser_body(input, entries)},
            .exception_specification = {.kind = FunctionExceptionKind::noexcept_},
            .flags = {.is_constexpr = true, .is_nodiscard = true, .define_in_header = true}};
        return header_function(add_function(function));
    }
  private:
    auto character_literal(unsigned char const character) -> Expr {
        auto const value{static_cast<char>(character)};
        if (ioj::needs_char_literal_escape(value)) {
            return literal(std_types().char_, std::format("'\\{}'", value));
        }
        if (ioj::isprint(value)) {
            return literal(std_types().char_, std::format("'{}'", value));
        }

        // Keep the byte value positive regardless of whether plain char is signed.
        return literal(intern_type("char32_t"), std::format("U'\\x{:02x}'", character));
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
        auto body{node_list()};
        if (entries.empty()) {
            body.add(expr_stmt(static_cast_expr(std_types().void_, input)));
        }
        for (auto const entry : entries) {
            body.add(if_stmt(
                binary(BinaryOperator::equal, input, string_literal(entry.key.substr(prefix_size))),
                block({return_stmt(entry.value)})));
        }
        body.add(return_stmt(type_reference(std_types().nullopt)));
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
        cases.push_back(switch_case({}, block({return_stmt(type_reference(std_types().nullopt))})));
        auto const selected_character{
            static_cast_expr(std_types().unsigned_char, subscript(input, literal(position)))};
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

        auto body{node_list()};
        auto const prefix{common_prefix(all)};
        auto suffix{input};
        if (!prefix.empty()) {
            body.add(if_stmt(unary(UnaryOperator::logical_not,
                                   member_call(input, "starts_with", {string_literal(prefix)})),
                             block({return_stmt(type_reference(std_types().nullopt))})),
                     2);
            body.add(variable("auto const",
                              "suffix",
                              member_call(input, "substr", {literal(prefix.size())})),
                     2);
            suffix = named("suffix");
        }

        std::map<std::size_t, Entries> lengths;
        for (auto const entry : all) {
            lengths[entry.key.size() - prefix.size()].push_back(entry);
        }
        std::vector<SwitchCaseId> cases;
        for (auto const& [length, group] : lengths) {
            cases.push_back(switch_case(
                literal(length), block(length_bucket(suffix, length, group, prefix.size()))));
        }
        cases.push_back(switch_case({}, block({return_stmt(type_reference(std_types().nullopt))})));
        body.add(switch_stmt(member_call(suffix, "size"), cases));
        return body.build();
    }
    auto indexed_parser_body(Expr input,
                             std::string_view enum_type,
                             std::span<StringEnumEntry const> entries) -> Nodes {
        auto const lookup{template_id(
            type_reference(named_type("ioj::try_parse_enum",
                                      NameOrigin::global,
                                      {dependency("ioj::try_parse_enum", "ioj/parse_enum.h")})),
            {type_reference(named_type(enum_type))})};
        std::vector<Syntax> names;
        names.reserve(entries.size());
        for (auto const& entry : entries) {
            names.push_back(string_literal(entry.key));
        }
        auto const array_type{
            template_type(std_types().array,
                          {std_types().string_view, intern_type(std::to_string(names.size()))})};
        return {variable(decorate_type(array_type, "", " constexpr"), "names", init_list(names)),
                return_stmt(call(lookup, {input, named("names")}))};
    }
};

} // namespace

auto lower_string_enum_parser(LoweringContext& ctx,
                              std::string_view const function_name,
                              std::string_view const enum_type,
                              std::string_view const parameter_name,
                              std::span<StringEnumEntry const> const entries,
                              bool const indexed) -> NodeId {
    return StringEnumParserLowering{ctx.ir}.lower_string_enum_parser(
        function_name, enum_type, parameter_name, entries, indexed);
}

} // namespace lispb::codegen::detail
