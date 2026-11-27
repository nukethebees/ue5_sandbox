#include "enum_accessor_lowering.h"

#include "lowering_context.h"

#include <codegen/ast/cpp_ir_builder.h>
#include <codegen/schema/declarations.h>
#include <lispb/schema/type_graph.h>

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <map>
#include <ranges>
#include <string>
#include <vector>

namespace lispb::codegen::detail {
namespace {
class EnumAccessorLowering : public CppIRBuilder {
  public:
    EnumAccessorLowering(LoweringContext& context,
                         std::span<EnumAccessorMember const> fields,
                         bool packed)
        : CppIRBuilder{context.ir}
        , packed_{packed} {
        for (auto const& field : fields) {
            auto const type{
                *context.semantic.find_reference(field.key.enumeration, context.module)};
            auto& group{groups_[type]};
            group.type = context.resolve(field.key.enumeration);
            auto const& domains{context.semantic.enum_domains()};
            auto const domain{domains.find(context.semantic.source_declaration(type))};
            auto const values{domains.values(domain)};
            auto const found{std::ranges::find_if(values, [&](auto value) {
                return context.declarations.enumerators.name[value] == field.key.enumerator;
            })};
            assert(found != values.end());
            auto const known{domains.code(domain, found - values.begin()).has_value()};
            for (auto const& previous : group.fields) {
                if (!known || std::ranges::contains(group.symbolic_keys, previous.key.enumerator)) {
                    alias_checks_.push_back(
                        static_assertion(binary(BinaryOperator::not_equal,
                                                scope_member(group.type, previous.key.enumerator),
                                                scope_member(group.type, field.key.enumerator)),
                                         store_text("Duplicate enum-accessor key")));
                }
            }
            if (!known) {
                group.symbolic_keys.push_back(field.key.enumerator);
            }
            group.fields.push_back(field);
        }
    }

    auto emit() -> Nodes {
        auto functions{node_list()};
        for (auto check : alias_checks_) {
            functions.add(check, 1);
        }
        functions.add(keyed_function(false), 2);
        functions.add(keyed_function(true), 2);
        for (auto const& [type, group] : groups_) {
            auto const value_type{group.fields.front().value_type};
            if (std::ranges::all_of(group.fields, [&](auto const& field) {
                    return type_spelling(field.value_type) == type_spelling(value_type);
                })) {
                functions.add(runtime_function(group, false), 2);
                functions.add(runtime_function(group, true), 2);
            }
        }
        return functions.build();
    }
  private:
    struct Group {
        CppTypeId type;
        std::vector<EnumAccessorMember> fields;
        std::vector<StringId> symbolic_keys;
    };

    auto matches(CppTypeId type) -> Expr {
        return reference(type_reference(template_type(std_types().same_as,
                                                      {intern_type("decltype(Key)"), type})),
                         std_types().bool_);
    }

    auto receiver(bool setter) -> Expr {
        return setter || packed_ ? named("self")
                                 : static_cast_expr(intern_type("decltype(self)&&"), named("self"));
    }

    auto forwarded_value() -> Expr {
        return static_cast_expr(intern_type("decltype(value)&&"), named("value"));
    }

    auto function(bool setter) -> FunctionDefinition {
        FunctionDefinition result{.name = name(setter ? "set" : "get"),
                                  .return_type =
                                      setter ? std_types().void_ : intern_type("decltype(auto)"),
                                  .parameters = {parameter(setter    ? "this auto&"
                                                           : packed_ ? "this auto const&"
                                                                     : "this auto&&",
                                                           "self")},
                                  .implementation = {.kind = FunctionImplementationKind::body},
                                  .flags = {.is_constexpr = true, .define_in_header = true}};
        return result;
    }

    auto access(EnumAccessorMember const& field, bool setter) -> BlockId {
        if (!packed_) {
            auto const member{member_access(receiver(setter), field.field)};
            return block({setter ? assignment(member, forwarded_value())
                                 : return_stmt(parenthesized(member))});
        }

        auto const read = [&](StringId element) {
            return call(member_name(named("self"), element), {});
        };
        auto const write = [&](StringId element, Expr value) {
            auto const setter_name{std::string{"set_"} + std::string{type_strings().view(element)}};
            return expr_stmt(call(member_name(named("self"), setter_name), {value}));
        };
        if (field.elements.empty()) {
            return block(
                {setter ? write(field.field, forwarded_value()) : return_stmt(read(field.field))});
        }

        Nodes body;
        std::vector<Syntax> elements;
        for (auto element : field.elements) {
            if (setter) {
                body.push_back(write(
                    element,
                    subscript(named("value"), literal(static_cast<std::uint64_t>(body.size())))));
            } else {
                elements.push_back(read(element));
            }
        }
        if (!setter) {
            body.push_back(return_stmt(init_list(elements, field.value_type)));
        }
        return block(body);
    }

    auto keyed_function(bool setter) -> NodeId {
        auto result{function(setter)};
        result.template_declaration = template_declaration(
            {value_template_parameter(intern_type("auto"), store_text("Key"))});
        if (setter) {
            result.parameters.push_back(parameter("auto&&", "value"));
        }
        auto const unsupported{
            static_assertion(literal(false), store_text("Unsupported enum accessor key"))};
        auto types{unsupported};
        for (auto const& [type, group] : groups_ | std::views::reverse) {
            auto values{unsupported};
            for (auto const& field : group.fields | std::views::reverse) {
                auto const equal{binary(BinaryOperator::equal,
                                        named("Key"),
                                        scope_member(group.type, field.key.enumerator))};
                values = if_stmt(equal, access(field, setter), block({values}), true);
            }
            if (groups_.size() == 1) {
                result.requires_clause = matches(group.type);
                result.implementation.body = {values};
            } else {
                types = if_stmt(matches(group.type), block({values}), block({types}), true);
            }
        }
        if (groups_.size() > 1) {
            result.implementation.body = {types};
        }
        return header_function(add_function(result));
    }

    auto runtime_function(Group const& group, bool setter) -> NodeId {
        auto result{function(setter)};
        result.parameters.push_back(parameter(group.type, "key"));
        if (setter) {
            result.parameters.push_back(parameter(group.fields.front().value_type, "value"));
        }
        std::vector<SwitchCaseId> cases;
        for (auto const& field : group.fields) {
            auto const key{scope_member(group.type, field.key.enumerator)};
            auto const callee{
                member_name(receiver(setter), setter ? "template set" : "template get")};
            auto const invoke{setter ? template_call(callee, {key}, {forwarded_value()})
                                     : template_call(callee, {key}, {})};
            cases.push_back(switch_case(key, block({return_stmt(invoke)}), false));
        }
        cases.push_back(
            switch_case({},
                        block({assertion(binary(BinaryOperator::logical_and,
                                                literal(false),
                                                string_literal("Unmapped enum accessor key"))),
                               expr_stmt(call(std_types().unreachable))}),
                        false));
        result.implementation.body = {switch_stmt(named("key"), cases)};
        return header_function(add_function(result));
    }
    std::map<schema::TypeId, Group> groups_;
    Nodes alias_checks_;
    bool packed_;
};
}

auto lower_enum_accessors(LoweringContext& context,
                          std::span<EnumAccessorMember const> members,
                          bool packed) -> Nodes {
    if (members.empty()) {
        return {};
    }
    return EnumAccessorLowering{context, members, packed}.emit();
}
}
