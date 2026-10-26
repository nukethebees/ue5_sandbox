#include "declaration_emission.h"
#include "lowering.h"
#include "lowering_context.h"
#include "packed_value_internal.h"

#include <codegen/ast/cpp_ir.h>
#include <codegen/ast/cpp_types.h>
#include <codegen/schema/child_occurrences.h>
#include <codegen/schema/declaration_schema.h>
#include <codegen/schema/declarations.h>
#include <codegen/schema/enumerator_schemas.h>
#include <codegen/schema/packed_named_code_schemas.h>
#include <lispb/schema/bit_layout.h>
#include <lispb/schema/enum_storage_requirement.h>
#include <lispb/schema/integer_domain.h>
#include <lispb/schema/type_graph.h>

#include <algorithm>
#include <cstdint>
#include <format>
#include <limits>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace lispb::codegen::detail {
namespace {

auto integer_type(std::uint32_t bits, bool signedness = false) -> std::string {
    auto const width{bits <= 8 ? 8 : bits <= 16 ? 16 : bits <= 32 ? 32 : 64};
    return std::format("std::{}{}_t", signedness ? "int" : "uint", width);
}

auto literal(PackedIntegerValue value) -> std::string {
    if (value.negative && value.magnitude == (std::uint64_t{1} << 63)) {
        return "(-9223372036854775807LL - 1)";
    }
    return std::format(
        "{}{}{}", value.negative ? "-" : "", value.magnitude, value.negative ? "LL" : "ULL");
}

auto mask(std::uint32_t bits) -> std::uint64_t {
    return bits == 64 ? (std::numeric_limits<std::uint64_t>::max)()
                      : (std::uint64_t{1} << bits) - 1;
}

auto composite(schema::TypeDefinitionKind kind) -> bool {
    return kind == schema::TypeDefinitionKind::record ||
           kind == schema::TypeDefinitionKind::union_type;
}

auto value_type(LoweringContext& context, schema::TypeId type, std::uint32_t bits) -> std::string {
    auto const& graph{context.semantic};
    auto const definition{graph.definition(type)};
    auto const kind{definition.kind()};
    auto const& definitions{graph.definitions()};
    if (composite(kind) && schema::is_packed_projection(graph, type)) {
        return context.text(graph.cpp_spelling(type));
    }
    if (kind == schema::TypeDefinitionKind::enumeration) {
        auto const declaration{graph.source_declaration(type)};
        if (context.declarations.enums.cpp_projection[declaration]) {
            return context.text(graph.cpp_spelling(type));
        }
    }
    if (context.text(graph.cpp_spelling(type)) == "bool") {
        return "bool";
    }
    if (auto const domain{schema::integer_domain(graph, type)}) {
        auto const row{definitions.integer_scalars.index(*domain)};
        return integer_type(bits, definitions.integer_scalars.signedness[row] != 0);
    }
    if (kind == schema::TypeDefinitionKind::fixed_point) {
        return integer_type(
            bits,
            definitions.fixed_points.signedness[definitions.fixed_points.index(definition)] != 0);
    }
    if (packed_signed_width(context.text(graph.cpp_spelling(type)))) {
        return integer_type(bits, true);
    }
    return integer_type(bits);
}

auto consecutive(PackedIntegerValue left, PackedIntegerValue right) -> bool {
    if (left.negative) {
        return left.magnitude == 1 ? right.magnitude == 0
                                   : right.negative && right.magnitude == left.magnitude - 1;
    }
    return !right.negative && left.magnitude != (std::numeric_limits<std::uint64_t>::max)() &&
           right.magnitude == left.magnitude + 1;
}

auto accessor_name(LoweringContext& context, schema::BitPlacement const& placement) -> std::string {
    switch (context.semantic.definition(placement.type).kind()) {
        case schema::TypeDefinitionKind::fixed_point:
            return placement.path + "_raw";
        case schema::TypeDefinitionKind::linear_quantized:
        case schema::TypeDefinitionKind::mini_float:
        case schema::TypeDefinitionKind::optional_sentinel:
        case schema::TypeDefinitionKind::optional_presence_bit:
            return placement.path + "_encoded";
        default:
            return placement.path;
    }
}

void append_dependencies(LoweringContext& context,
                         schema::ChildOccurrenceId child,
                         std::vector<TypeDependencyId>& dependencies) {
    auto const& children{context.semantic.definitions().children};
    auto const occurrence{children.semantic_type[children.index(child)]};
    auto const& occurrences{context.semantic.occurrences()};
    auto const type{occurrences.cpp_type[occurrences.index(occurrence)]};
    if (type.value == 0) {
        return;
    }
    auto const required{context.ir.types.dependencies(type)};
    dependencies.insert(dependencies.end(), required.begin(), required.end());
}

class PackedEmitter {
  public:
    PackedEmitter(LoweringContext& context, schema::TypeId type)
        : context_{context}
        , ir_{context.ir}
        , graph_{context.semantic}
        , type_{type}
        , layout_{graph_.bit_layout(type)}
        , name_{context.text(graph_.name(type))}
        , scalar_{!composite(graph_.definition(type).kind())} {
        if (scalar_) {
            layout_.placements.push_back({.type = type_,
                                          .path = "value",
                                          .bits = *layout_.encoded_bits,
                                          .extent = *layout_.encoded_bits});
        }
        field_metadata_.resize(layout_.placements.size());
    }

    auto emit() -> Nodes {
        auto const owner_namespace{context_.text(graph_.namespace_name(type_))};
        auto const detail_namespace{(owner_namespace.empty() ? "" : owner_namespace + "::") +
                                    "detail::"};
        Nodes detail_members;
        auto default_value{ir_.types.intern("0")};
        auto const base{
            ir_.types.named("ioj::PackedValueBase",
                            {ir_.types.dependency("ioj::PackedValueBase", "ioj/packed_value.h")})};
        auto const storage_type{type(integer_type(layout_.storage_bits))};
        if (layout_.default_raw_value.value_or(0) != 0) {
            auto const default_name{soa_namespace_name(name_) + "_default_raw_value"};
            auto const default_symbol{type(detail_namespace + default_name)};
            detail_members.push_back(
                ir_.variable(ir_.types.decorate(storage_type, "inline constexpr ", ""),
                             text(default_name),
                             number(*layout_.default_raw_value)));
            default_value = default_symbol;
        }
        auto const descriptor{ir_.types.template_type(
            ir_.types.named("ioj::PackedStorage",
                            {ir_.types.dependency("ioj::PackedStorage", "ioj/packed_value.h")}),
            {storage_type, ir_.types.intern(std::to_string(*layout_.encoded_bits))})};
        auto const storage{ir_.types.compose({{.type = descriptor},
                                              {.text = text("{")},
                                              {.type = default_value},
                                              {.text = text("}")}})};
        auto const base_type{ir_.types.template_type(base, {type(name_), storage})};
        auto const base_name{name_ + "Base"};
        auto const base_alias{type(detail_namespace + base_name)};
        detail_members.push_back(ir_.using_declaration(text(base_name), base_type));
        metadata_.push_back(ir_.line_comment("Storage"));
        metadata_.push_back(ir_.using_declaration(text("Base"), base_alias));
        metadata_.push_back(ir_.using_declaration(
            text("packed_integer_tag"),
            ir_.types.named(scalar_ ? "ioj::packed::scalar_tag" : "ioj::packed::integer_tag",
                            {ir_.types.dependency("ioj::PackedStorage", "ioj/packed_value.h")})));
        packed_members();
        Nodes members{ir_.access()};
        members.push_back(ir_.lines(1));
        append(members, metadata_);
        auto const count{layout_.placements.size()};
        for (std::size_t index{}; index < count; ++index) {
            if (!field_metadata_[index].empty()) {
                members.push_back(ir_.lines(2));
                members.push_back(ir_.line_comment(layout_.placements[index].path));
                members.push_back(ir_.lines(1));
                append(members, field_metadata_[index]);
            }
        }
        for (auto const* group : {&constructors_, &operators_, &functions_}) {
            if (!group->empty()) {
                members.push_back(ir_.lines(2));
                append(members, *group);
            }
        }
        if (!private_functions_.empty()) {
            members.push_back(ir_.lines(2));
            members.push_back(ir_.access(Access::private_));
            members.push_back(ir_.lines(1));
            append(members, private_functions_);
        }
        auto const record{ir_.record(text(name_), members)};
        ir_.set_record_bases(record, {base_alias});
        std::vector<StringId> inherited;
        for (auto const name : {"storage_type",
                                "storage",
                                "raw_value",
                                "from_raw",
                                "from_canonical_raw",
                                "is_valid",
                                "extract",
                                "insert",
                                "value_"}) {
            inherited.push_back(text(name));
        }
        ir_.set_scope_bindings(record, inherited);
        auto const declaration{graph_.source_declaration(type_)};
        auto const kind{graph_.definition(type_).kind()};
        StringId export_specifier{};
        if (kind == schema::TypeDefinitionKind::record) {
            export_specifier = context_.declarations.records.export_specifier[declaration];
        } else if (kind == schema::TypeDefinitionKind::union_type) {
            export_specifier = context_.declarations.unions.export_specifier[declaration];
        }
        ir_.set_record_export_specifier(record, export_specifier);
        ir_.set_record_dependencies(record, dependencies_);
        return {ir_.forward_declaration(text(name_), RecordKind::struct_, {}, export_specifier),
                ir_.namespace_node(text("detail"), detail_members),
                record,
                ir_.static_assertion(template_call(
                    "ioj::valid_packed_value_layout", "ioj/packed_value.h", {type(name_)}, {}))};
    }
  private:
    using Predicate = std::optional<Expr>;
    struct Validation {
        bool input_required{};
        std::optional<std::string> stored_helper;
    };

    void append(Nodes& target, Nodes const& source) {
        bool first{true};
        for (auto const node : source) {
            if (!first) {
                target.push_back(ir_.lines(1));
            }
            target.push_back(node);
            first = false;
        }
    }
    auto text(std::string_view value) -> StringId { return ir_.store_text(value); }
    auto type(std::string_view value) -> CppTypeId {
        return ir_.types.named(
            value,
            value.starts_with("std::")
                ? std::initializer_list<TypeDependencyId>{ir_.types.dependency(value, "cstdint")}
                : std::initializer_list<TypeDependencyId>{});
    }
    auto number(std::uint64_t value) -> Expr {
        return ir_.literal(
            std::to_string(value) +
            (value > static_cast<std::uint64_t>((std::numeric_limits<std::int64_t>::max)()) ? "ULL"
                                                                                            : ""));
    }
    auto integer(PackedIntegerValue value) -> Expr { return ir_.literal(literal(value)); }
    auto value() -> Expr { return ir_.named("value"); }
    auto raw() -> Expr { return ir_.named("value_"); }
    auto external(std::string_view name, std::string_view header) -> Expr {
        return ir_.type_reference(ir_.types.named(name, {ir_.types.dependency(name, header)}));
    }
    auto invoke(std::string_view name, std::initializer_list<Expr> args = {}) -> Expr {
        return ir_.call(ir_.named(name), args);
    }
    auto library_call(std::string_view name,
                      std::string_view header,
                      std::initializer_list<Expr> args) -> Expr {
        return ir_.call(external(name, header), args);
    }
    auto template_call(std::string_view name,
                       std::string_view header,
                       std::initializer_list<CppTypeId> types,
                       std::initializer_list<Expr> args) -> Expr {
        std::vector<Expr> arguments;
        for (auto const item : types) {
            arguments.push_back(ir_.type_reference(item));
        }
        return ir_.call(ir_.template_id(external(name, header), arguments), args);
    }
    auto extract(std::string const& field, std::optional<Expr> source = {}) -> Expr {
        return source ? template_call("ioj::packed_extract",
                                      "ioj/packed_value.h",
                                      {descriptor_type(field)},
                                      {*source})
                      : ir_.call(ir_.template_id(ir_.named("extract"),
                                                 {ir_.type_reference(descriptor_type(field))}),
                                 {});
    }
    auto insert_member(std::string const& field, Expr source, std::optional<Expr> target = {})
        -> NodeId {
        return ir_.expr_stmt(ir_.call(
            ir_.template_id(target ? ir_.member_access(*target, "insert") : ir_.named("insert"),
                            {ir_.type_reference(descriptor_type(field))}),
            {source}));
    }
    auto insert(std::string const& field, Expr target, Expr source) -> Expr {
        return template_call(
            "ioj::packed_insert", "ioj/packed_value.h", {descriptor_type(field)}, {target, source});
    }
    auto pack(std::string const& field, Expr source) -> Expr {
        return template_call(
            "ioj::packed_pack", "ioj/packed_value.h", {descriptor_type(field)}, {source});
    }
    auto descriptor_type(std::string const& name) -> CppTypeId {
        return ir_.types.decorate(ir_.types.intern(name), "decltype(", ")");
    }
    auto descriptor_value(std::string const& name) -> CppTypeId {
        return ir_.types.decorate(descriptor_type(name), "", "::value_type");
    }
    auto cast(CppTypeId target, Expr source) -> Expr {
        return ir_.static_cast_expr(target, source);
    }
    auto cast(std::string const& target, Expr source) -> Expr {
        return ir_.static_cast_expr(type(target), source);
    }
    auto member_call(Expr target,
                     std::string_view member,
                     std::initializer_list<Expr> arguments = {}) -> Expr {
        return ir_.call(ir_.member_access(target, member), arguments);
    }
    auto binary(BinaryOperator op, Expr left, Expr right) -> Expr {
        return ir_.binary(op, left, right);
    }
    auto all(std::span<Predicate const> predicates) -> Predicate {
        Predicate result;
        for (auto const predicate : predicates) {
            if (predicate) {
                result =
                    result ? binary(BinaryOperator::logical_and, *result, *predicate) : *predicate;
            }
        }
        return result;
    }
    auto all(std::initializer_list<Predicate> predicates) -> Predicate {
        return all(std::span<Predicate const>{predicates});
    }
    auto any(std::span<Predicate const> predicates) -> Predicate {
        Predicate result;
        for (auto const predicate : predicates) {
            if (!predicate) {
                return {};
            }
            result = result ? binary(BinaryOperator::logical_or, *result, *predicate) : *predicate;
        }
        return result.value_or(ir_.literal("false"));
    }
    auto boolean(Predicate predicate) -> Expr { return predicate.value_or(ir_.literal("true")); }
    auto assertion(Expr condition) -> NodeId {
        return ir_.expr_stmt(
            ir_.call(ir_.named("assert", {ir_.types.dependency("assert", "cassert")}),
                     {ir_.parenthesized(condition)}));
    }
    void assert_if(Nodes& body, Predicate predicate) {
        if (predicate) {
            body.push_back(assertion(*predicate));
        }
    }
    void alias(Nodes& nodes, std::string const& name, std::string const& target) {
        nodes.push_back(ir_.using_declaration(text(name), type(target)));
    }
    auto constant(Nodes& nodes, std::string const& name, CppTypeId target, Expr initializer)
        -> Expr {
        if (constants_.insert(name).second) {
            nodes.push_back(ir_.constexpr_member(target, text(name), initializer));
        }
        return ir_.named(name);
    }
    auto
        constant(Nodes& nodes, std::string const& name, std::string const& target, Expr initializer)
            -> Expr {
        return constant(nodes, name, type(target), initializer);
    }
    void field_layout(Nodes& metadata,
                      std::string const& name,
                      schema::TypeId logical,
                      std::uint32_t bits,
                      CppTypeId decoded,
                      std::uint32_t offset = 0) {
        auto const& definitions{graph_.definitions()};
        auto const definition{graph_.definition(logical)};
        std::optional<PackedIntegerValue> minimum;
        std::optional<PackedIntegerValue> maximum;
        if (auto const domain{schema::integer_domain(graph_, logical)}) {
            auto const row{definitions.integer_scalars.index(*domain)};
            minimum = definitions.integer_scalars.minimum_value[row];
            maximum = definitions.integer_scalars.maximum_value[row];
        } else if (definition.kind() == schema::TypeDefinitionKind::enumeration) {
            auto const domain{definitions.enums.domain[definitions.enums.index(definition)]};
            if (auto const range{graph_.enum_domains().contiguous_value_range(domain)}) {
                minimum = PackedIntegerValue{range->min};
                maximum = PackedIntegerValue{range->max};
            }
        } else if (definition.kind() == schema::TypeDefinitionKind::fixed_point) {
            auto const row{definitions.fixed_points.index(definition)};
            minimum = definitions.fixed_points.minimum_raw_value[row];
            maximum = definitions.fixed_points.maximum_raw_value[row];
        } else if (definition.kind() == schema::TypeDefinitionKind::linear_quantized) {
            maximum = PackedIntegerValue{
                mask(bits) - definitions.linear_quantized
                                 .reserved_codes[definitions.linear_quantized.index(definition)]};
        }
        auto initializer{ir_.init_list({})};
        auto const signedness{value_type(context_, logical, bits).starts_with("std::int")};
        auto const physical_min{PackedIntegerValue::from_parts(
            signedness, signedness ? std::uint64_t{1} << (bits - 1) : 0)};
        auto const physical_max{PackedIntegerValue{mask(bits - (signedness ? 1 : 0))}};
        auto const lower{minimum.value_or(physical_min)};
        auto const upper{maximum.value_or(physical_max)};
        if (lower != physical_min || upper != physical_max) {
            initializer = ir_.init_list(
                {ir_.init_list({cast(decoded, integer(lower)), cast(decoded, integer(upper))})});
        }
        auto const packed{ir_.types.named(
            "ioj::PackedField", {ir_.types.dependency("ioj::PackedField", "ioj/packed_value.h")})};
        auto const descriptor{ir_.types.template_type(
            packed,
            {type("storage_type"),
             decoded,
             ir_.types.intern(std::to_string(offset)),
             ir_.types.intern(logical == type_ && !composite(graph_.definition(type_).kind())
                                  ? "storage.encoded_bits"
                                  : std::to_string(bits))})};
        constant(metadata, name, descriptor, initializer);
    }
    auto range_bound(std::string const& layout, std::string_view bound) -> Expr {
        return ir_.member_access(ir_.member_access(ir_.named(layout), "range"), bound);
    }
    auto parameter(CppTypeId alias, std::string_view name) -> FunctionParameterId {
        return ir_.parameter(ir_.types.decorate(alias, "", " const"), name);
    }
    auto parameter(std::string const& alias, std::string_view name) -> FunctionParameterId {
        return parameter(type(alias), name);
    }
    auto function(std::string const& name,
                  CppTypeId result,
                  FunctionReceiver receiver,
                  std::span<FunctionParameterId const> parameters,
                  Nodes const& body,
                  bool constexpr_ = true) -> NodeId {
        auto const fn{ir_.function(name, result)};
        ir_.set_function_receiver(fn, receiver);
        ir_.set_function_noexcept(fn);
        ir_.set_function_parameters(fn, parameters);
        ir_.set_function_return_type(fn,
                                     result,
                                     ir_.types.spelling(result) == "void" ? ReturnSyntax::direct
                                                                          : ReturnSyntax::trailing);
        ir_.function_flags(fn).set_is_constexpr(constexpr_);
        ir_.function_flags(fn).set_is_nodiscard(ir_.types.spelling(result) != "void");
        ir_.function_flags(fn).set_define_in_header(true);
        ir_.set_function_body(fn, body);
        return ir_.header_function(fn);
    }
    auto function(std::string const& name,
                  CppTypeId result,
                  FunctionReceiver receiver,
                  std::initializer_list<FunctionParameterId> parameters,
                  Nodes const& body,
                  bool constexpr_ = true) -> NodeId {
        return function(name,
                        result,
                        receiver,
                        std::span<FunctionParameterId const>{parameters},
                        body,
                        constexpr_);
    }
    auto function(std::string const& name,
                  std::string const& result,
                  FunctionReceiver receiver,
                  std::span<FunctionParameterId const> parameters,
                  Nodes const& body,
                  bool constexpr_ = true) -> NodeId {
        return function(name, type(result), receiver, parameters, body, constexpr_);
    }
    auto function(std::string const& name,
                  std::string const& result,
                  FunctionReceiver receiver,
                  std::initializer_list<FunctionParameterId> parameters,
                  Nodes const& body,
                  bool constexpr_ = true) -> NodeId {
        return function(name,
                        type(result),
                        receiver,
                        std::span<FunctionParameterId const>{parameters},
                        body,
                        constexpr_);
    }
    auto default_constructor(bool available) -> NodeId {
        auto const fn{ir_.constructor(text(name_))};
        ir_.function_flags(fn).set_is_constexpr(true);
        ir_.function_flags(fn).set_define_in_header(true);
        ir_.set_function_noexcept(fn);
        if (available) {
            ir_.set_function_defaulted(fn);
        } else {
            ir_.set_function_deleted(fn);
        }
        return ir_.header_function(fn);
    }
    void comparison(bool defaulted) {
        auto const owner{ir_.types.decorate(type(name_), "", " const&")};
        for (auto const equality : {false, true}) {
            if (equality && defaulted) {
                continue;
            }
            auto const decoded{!defaulted && !equality};
            auto const fn{ir_.function(
                equality ? "operator==" : "operator<=>",
                equality ? type("bool")
                         : ir_.types.intern("auto", {ir_.types.dependency("auto", "compare")}))};
            ir_.set_function_receiver(fn, FunctionReceiver::const_);
            ir_.set_function_noexcept(fn);
            ir_.function_flags(fn).set_is_constexpr(true);
            ir_.function_flags(fn).set_define_in_header(true);
            ir_.function_flags(fn).set_is_nodiscard(true);
            ir_.set_function_parameters(fn, {ir_.parameter(owner, decoded ? "other" : "")});
            if (decoded) {
                ir_.set_function_body(
                    fn,
                    {ir_.return_stmt(binary(BinaryOperator::three_way,
                                            invoke("value"),
                                            member_call(ir_.named("other"), "value")))});
            } else {
                ir_.set_function_defaulted(fn);
            }
            operators_.push_back(ir_.header_function(fn));
        }
    }
    auto scalar_domain(schema::TypeId scalar,
                       std::uint32_t bits,
                       Expr expression,
                       std::string const& prefix,
                       CppTypeId decoded,
                       Nodes& metadata,
                       bool field_range = false) -> Predicate {
        auto const& definitions{graph_.definitions()};
        auto const definition{graph_.definition(scalar)};
        auto const qualify = [&](std::string_view suffix) {
            return prefix.empty() ? std::string{suffix} : prefix + "_" + std::string{suffix};
        };
        auto const bound = [&](std::string_view suffix, PackedIntegerValue value) {
            return constant(metadata, qualify(suffix), decoded, cast(decoded, integer(value)));
        };
        auto const layout{prefix.empty() ? "value_layout" : prefix + "_layout"};
        if (auto const domain{schema::integer_domain(graph_, scalar)}) {
            auto const& scalars{definitions.integer_scalars};
            auto const row{scalars.index(*domain)};
            auto const signedness{scalars.signedness[row] != 0};
            auto const representable_minimum{PackedIntegerValue::from_parts(
                signedness, signedness ? std::uint64_t{1} << (bits - 1) : 0)};
            auto const representable_maximum{PackedIntegerValue{mask(bits - (signedness ? 1 : 0))}};
            auto const minimum{range_bound(layout, "min")};
            auto const maximum{range_bound(layout, "max")};
            auto lower{scalars.minimum_value[row]};
            auto upper{scalars.maximum_value[row]};
            auto const& codes{context_.declarations.packed_named_codes};
            auto const named{context_.declarations.items(scalars.named_codes[row])};
            for (std::size_t pass{}; pass < named.size(); ++pass) {
                for (auto const id : named) {
                    if (!codes.sentinel[id]) {
                        continue;
                    }
                    if (consecutive(codes.value[id], lower)) {
                        lower = codes.value[id];
                    }
                    if (consecutive(upper, codes.value[id])) {
                        upper = codes.value[id];
                    }
                }
            }
            if (!packed_integer_less(representable_minimum, lower) &&
                !packed_integer_less(upper, representable_maximum)) {
                return {};
            }
            std::vector<Predicate> bounds;
            if (packed_integer_less(representable_minimum, scalars.minimum_value[row])) {
                bounds.push_back(binary(BinaryOperator::greater_equal, expression, minimum));
            }
            if (packed_integer_less(scalars.maximum_value[row], representable_maximum)) {
                bounds.push_back(binary(BinaryOperator::less_equal, expression, maximum));
            }
            std::vector<Predicate> alternatives{all(bounds)};
            for (auto const id : named) {
                if (codes.sentinel[id]) {
                    auto const name{qualify(context_.text(codes.name[id]))};
                    if (!field_range && !prefix.empty()) {
                        bound(context_.text(codes.name[id]), codes.value[id]);
                    }
                    alternatives.push_back(
                        binary(BinaryOperator::equal, expression, ir_.named(name)));
                }
            }
            return any(alternatives);
        }
        switch (definition.kind()) {
            case schema::TypeDefinitionKind::enumeration: {
                auto const domain{definitions.enums.domain[definitions.enums.index(definition)]};
                auto const& domains{graph_.enum_domains()};
                if (auto const range{domains.contiguous_value_range(domain)}) {
                    std::vector<Predicate> checks;
                    if (range->min != 0) {
                        checks.push_back(binary(
                            BinaryOperator::greater_equal, expression, range_bound(layout, "min")));
                    }
                    if (range->max != mask(bits)) {
                        checks.push_back(binary(
                            BinaryOperator::less_equal, expression, range_bound(layout, "max")));
                    }
                    return all(checks);
                }
                auto const enumerators{domains.values(domain)};
                auto const count_value{domains.count_value[domains.index(domain)]};
                std::vector<Predicate> checks;
                auto const count{enumerators.size()};
                for (std::size_t ordinal{}; ordinal < count; ++ordinal) {
                    if (enumerators[ordinal] == count_value) {
                        continue;
                    }
                    auto const enumerator{context_.text(
                        context_.declarations.enumerators.name[enumerators[ordinal]])};
                    auto const code{domains.code(domain, ordinal)};
                    auto const declaration{graph_.source_declaration(scalar)};
                    auto const projected{
                        context_.declarations.enums.cpp_projection[declaration].has_value()};
                    auto const enumerator_value{
                        projected ? ir_.named(context_.text(graph_.cpp_spelling(scalar)) +
                                              "::" + enumerator)
                                  : cast(decoded,
                                         integer(PackedIntegerValue::from_parts(code->negative,
                                                                                code->magnitude)))};
                    checks.push_back(binary(BinaryOperator::equal, expression, enumerator_value));
                }
                return any(checks);
            }
            case schema::TypeDefinitionKind::fixed_point: {
                auto const& fixed{definitions.fixed_points};
                auto const row{fixed.index(definition)};
                std::vector<Predicate> checks;
                if (fixed.minimum_raw_value[row]) {
                    auto const minimum{range_bound(layout, "min")};
                    checks.push_back(binary(BinaryOperator::greater_equal, expression, minimum));
                }
                if (fixed.maximum_raw_value[row]) {
                    auto const maximum{range_bound(layout, "max")};
                    checks.push_back(binary(BinaryOperator::less_equal, expression, maximum));
                }
                return all(checks);
            }
            case schema::TypeDefinitionKind::linear_quantized: {
                auto const& quantized{definitions.linear_quantized};
                auto const reserved{quantized.reserved_codes[quantized.index(definition)]};
                auto const maximum{range_bound(layout, "max")};
                return reserved ? Predicate{binary(BinaryOperator::less_equal, expression, maximum)}
                                : Predicate{};
            }
            case schema::TypeDefinitionKind::optional_sentinel:
            case schema::TypeDefinitionKind::optional_presence_bit: {
                auto const sentinel{definition.kind() ==
                                    schema::TypeDefinitionKind::optional_sentinel};
                auto const source_occurrence{
                    sentinel ? definitions.optional_sentinels
                                   .source[definitions.optional_sentinels.index(definition)]
                             : definitions.optional_presence_bits
                                   .source[definitions.optional_presence_bits.index(definition)]};
                auto const source{
                    graph_.occurrences().type[graph_.occurrences().index(source_occurrence)]};
                auto const payload_bits{
                    sentinel
                        ? bits
                        : definitions.optional_presence_bits
                              .payload_bits[definitions.optional_presence_bits.index(definition)]};
                auto const payload{qualify("payload")};
                field_layout(metadata,
                             payload + "_layout",
                             source,
                             payload_bits,
                             type(value_type(context_, source, payload_bits)));
                auto const payload_alias{descriptor_value(payload + "_layout")};
                auto const decoded_source{
                    extract(payload + "_layout", cast("storage_type", expression))};
                auto const source_check{scalar_domain(
                    source, payload_bits, decoded_source, payload, payload_alias, metadata)};
                Expr absent;
                if (sentinel) {
                    absent = binary(
                        BinaryOperator::equal,
                        expression,
                        bound(
                            "sentinel",
                            definitions.optional_sentinels
                                .sentinel_value[definitions.optional_sentinels.index(definition)]));
                } else {
                    auto const presence{
                        binary(BinaryOperator::shift_left,
                               ir_.init_list({number(1)}, type("storage_type")),
                               ir_.member_access(ir_.named(payload + "_layout"), "bits"))};
                    absent = binary(BinaryOperator::equal,
                                    binary(BinaryOperator::bitwise_and,
                                           cast("storage_type", expression),
                                           presence),
                                    number(0));
                }
                return source_check
                         ? Predicate{binary(BinaryOperator::logical_or, absent, *source_check)}
                         : Predicate{};
            }
            default:
                return {};
        }
    }

    auto domain_implies_width(schema::TypeId logical, std::uint32_t bits) const -> bool {
        auto const& definitions{graph_.definitions()};
        if (auto const domain{schema::integer_domain(graph_, logical)}) {
            auto const& scalars{definitions.integer_scalars};
            auto const row{scalars.index(*domain)};
            auto const signedness{scalars.signedness[row] != 0};
            auto const minimum{PackedIntegerValue::from_parts(
                signedness, signedness ? std::uint64_t{1} << (bits - 1) : 0)};
            auto const maximum{PackedIntegerValue{mask(bits - (signedness ? 1 : 0))}};
            // The stored-domain predicate omits representable endpoints.
            return packed_integer_less(scalars.maximum_value[row], maximum) &&
                   (!signedness || packed_integer_less(minimum, scalars.minimum_value[row]));
        }
        auto const definition{graph_.definition(logical)};
        if (definition.kind() == schema::TypeDefinitionKind::enumeration) {
            auto const domain{definitions.enums.domain[definitions.enums.index(definition)]};
            auto const& domains{graph_.enum_domains()};
            auto const enumerators{domains.values(domain)};
            auto const count_value{domains.count_value[domains.index(domain)]};
            auto const count{enumerators.size()};
            for (std::size_t ordinal{}; ordinal < count; ++ordinal) {
                if (enumerators[ordinal] == count_value) {
                    continue;
                }
                auto const code{domains.code(domain, ordinal)};
                if (!code || code->negative || code->magnitude > mask(bits)) {
                    return false;
                }
            }
            return true;
        }
        if (definition.kind() == schema::TypeDefinitionKind::fixed_point) {
            auto const& fixed{definitions.fixed_points};
            auto const row{fixed.index(definition)};
            return fixed.maximum_raw_value[row].has_value() &&
                   (fixed.signedness[row] == 0 || fixed.minimum_raw_value[row].has_value());
        }
        return definition.kind() == schema::TypeDefinitionKind::linear_quantized;
    }
    auto representation(schema::TypeId logical,
                        std::string const& decoded,
                        std::string const& field,
                        std::uint32_t bits,
                        bool checked_domain) -> Predicate {
        if (graph_.definition(logical).kind() == schema::TypeDefinitionKind::enumeration) {
            auto const declaration{graph_.source_declaration(logical)};
            auto const& enums{context_.declarations.enums};
            if (auto const& projection{enums.cpp_projection[declaration]}) {
                std::optional<std::uint32_t> underlying_bits;
                auto const explicit_type{projection->storage ? projection->storage
                                                             : enums.underlying_type[declaration]};
                if (explicit_type) {
                    auto const occurrence{graph_.resolved(graph_.module(logical), *explicit_type)};
                    auto const& occurrences{graph_.occurrences()};
                    auto const underlying{occurrences.cpp_type[occurrences.index(occurrence)]};
                    if (auto const width{packed_unsigned_width(ir_.types.spelling(underlying))}) {
                        underlying_bits = static_cast<std::uint32_t>(*width);
                    }
                } else {
                    auto const& domains{graph_.enum_domains()};
                    if (auto const requirement{schema::derive_enum_storage_requirement(
                            domains, domains.find(declaration), enums.bit_width[declaration])}) {
                        underlying_bits = requirement->bit_width;
                    }
                }
                if ((underlying_bits && *underlying_bits <= bits) ||
                    (checked_domain && domain_implies_width(logical, bits))) {
                    return {};
                }
                return binary(BinaryOperator::less_equal,
                              library_call("std::to_underlying", "utility", {value()}),
                              ir_.member_access(ir_.named(field), "value_mask"));
            }
        }
        if (decoded == "bool" || bits == 8 || bits == 16 || bits == 32 || bits == 64) {
            return {};
        }
        if (checked_domain && domain_implies_width(logical, bits)) {
            return {};
        }
        return decoded.starts_with("std::int")
                 ? binary(BinaryOperator::equal, extract(field, pack(field, value())), value())
                 : binary(BinaryOperator::less_equal,
                          cast("storage_type", value()),
                          ir_.member_access(ir_.named(field), "value_mask"));
    }
    auto validator(std::string const& name, CppTypeId alias, Predicate domain, Predicate width)
        -> Validation {
        Validation result{.input_required = domain.has_value() || width.has_value()};
        if (!result.input_required && composite(graph_.definition(type_).kind())) {
            return result;
        }
        if (domain) {
            result.stored_helper = width ? name + "_domain" : name;
            if (width) {
                private_functions_.push_back(function(*result.stored_helper,
                                                      "bool",
                                                      FunctionReceiver::static_,
                                                      {parameter(alias, "value")},
                                                      {ir_.return_stmt(*domain)}));
                domain = invoke(*result.stored_helper, {value()});
            }
        }
        functions_.push_back(function(name,
                                      "bool",
                                      FunctionReceiver::static_,
                                      {parameter(alias, result.input_required ? "value" : "")},
                                      {ir_.return_stmt(boolean(all({domain, width})))}));
        return result;
    }

    auto field_name(schema::BitPlacement const& placement) -> std::string {
        return scalar_ ? "value" : accessor_name(context_, placement);
    }
    auto aggregate(schema::BitPlacement const& placement) const -> bool {
        return composite(graph_.definition(placement.type).kind());
    }
    auto addressable(schema::BitPlacement const& placement) const -> bool {
        return !placement.reserved && placement.bits != 0 &&
               (!aggregate(placement) || schema::is_packed_projection(graph_, placement.type));
    }
    auto access_type(schema::BitPlacement const& placement) -> CppTypeId {
        if (scalar_) {
            return type("value_type");
        }
        return aggregate(placement) && schema::is_packed_projection(graph_, placement.type)
                 ? type(context_.text(graph_.cpp_spelling(placement.type)))
                 : descriptor_value(field_name(placement) + "_layout");
    }
    auto record_invalid(schema::TypeId record) const -> std::optional<std::uint64_t> {
        auto const definition{graph_.definition(record)};
        if (definition.kind() != schema::TypeDefinitionKind::record) {
            return {};
        }
        auto const& records{graph_.definitions().records};
        return records.invalid_raw_value[records.index(definition)];
    }
    void integer_constants(schema::BitPlacement const& placement,
                           std::string const& accessor,
                           Nodes& metadata) {
        auto const domain{schema::integer_domain(graph_, placement.type)};
        if (!domain) {
            return;
        }
        auto const& scalars{graph_.definitions().integer_scalars};
        auto const row{scalars.index(*domain)};
        auto const& codes{context_.declarations.packed_named_codes};
        auto const decoded{access_type(placement)};
        for (auto const code : context_.declarations.items(scalars.named_codes[row])) {
            constant(metadata,
                     (scalar_ ? "" : accessor + "_") + context_.text(codes.name[code]),
                     decoded,
                     cast(decoded, integer(codes.value[code])));
        }
    }
    void packed_members() {
        if (auto const invalid{record_invalid(type_)}) {
            constant(metadata_,
                     "invalid_value",
                     "storage_type",
                     layout_.default_raw_value == invalid
                         ? ir_.member_access(ir_.named("storage"), "default_raw_value")
                         : number(*invalid));
        }
        auto const count{layout_.placements.size()};
        for (std::size_t index{}; index < count; ++index) {
            auto const& placement{layout_.placements[index]};
            if (placement.reserved || placement.bits == 0) {
                continue;
            }
            auto& metadata{field_metadata_[index]};
            auto const accessor{field_name(placement)};
            auto const decoded{addressable(placement)
                                   ? value_type(context_, placement.type, placement.bits)
                                   : "storage_type"};
            if (!scalar_ && addressable(placement) &&
                (aggregate(placement) || graph_.definition(placement.type).kind() ==
                                             schema::TypeDefinitionKind::enumeration)) {
                append_dependencies(context_, placement.child, dependencies_);
            }
            if (scalar_) {
                alias(metadata, "value_type", decoded);
            }
            field_layout(metadata,
                         accessor + "_layout",
                         placement.type,
                         placement.bits,
                         type(scalar_                ? "value_type"
                              : aggregate(placement) ? "storage_type"
                                                     : decoded),
                         placement.offset);
            integer_constants(placement, accessor, metadata);
            if (!aggregate(placement)) {
                auto const domain{scalar_domain(placement.type,
                                                placement.bits,
                                                value(),
                                                scalar_ ? "" : accessor,
                                                access_type(placement),
                                                metadata,
                                                !scalar_)};
                checks_[accessor] = validator("is_valid_" + accessor,
                                              access_type(placement),
                                              domain,
                                              representation(placement.type,
                                                             decoded,
                                                             accessor + "_layout",
                                                             placement.bits,
                                                             domain.has_value()));
            }
        }
        constructors_.push_back(
            default_constructor(scalar_ || layout_.default_raw_value.has_value()));
        if (!std::ranges::any_of(layout_.placements,
                                 [](auto const& placement) { return placement.alternative; })) {
            constructor(std::nullopt);
        }
        comparison(!scalar_ ||
                   !value_type(context_, type_, *layout_.encoded_bits).starts_with("std::int"));
        std::set<std::pair<std::optional<std::uint32_t>, schema::ChildOccurrenceId>> alternatives;
        for (std::size_t index{}; index < count; ++index) {
            auto const& placement{layout_.placements[index]};
            if (placement.alternative &&
                alternatives.emplace(placement.parent, placement.child).second) {
                constructor(static_cast<std::uint32_t>(index));
            }
        }
        auto validity{valid_children(std::nullopt, type_, std::nullopt)};
        if (record_invalid(type_)) {
            validity = all(
                {binary(BinaryOperator::not_equal, raw(), ir_.named("invalid_value")), validity});
        }
        if (validity) {
            functions_.push_back(function(
                "is_valid", "bool", FunctionReceiver::const_, {}, {ir_.return_stmt(*validity)}));
        }
        for (std::size_t index{}; index < count; ++index) {
            if (addressable(layout_.placements[index])) {
                accessor(static_cast<std::uint32_t>(index));
            }
        }
    }
    auto guard_code(schema::BitGuard const& guard) -> Expr {
        auto const declaration{graph_.source_declaration(guard.enumeration)};
        if (!context_.declarations.enums.cpp_projection[declaration]) {
            return cast(descriptor_value(guard.path + "_layout"), number(guard.value));
        }
        auto const definition{graph_.definition(guard.enumeration)};
        auto const& enums{graph_.definitions().enums};
        auto const domain{enums.domain[enums.index(definition)]};
        auto const& domains{graph_.enum_domains()};
        auto const values{domains.values(domain)};
        auto const count{values.size()};
        for (std::size_t ordinal{}; ordinal < count; ++ordinal) {
            auto const code{domains.code(domain, ordinal)};
            if (code && !code->negative && code->magnitude == guard.value) {
                return ir_.named(
                    context_.text(graph_.cpp_spelling(guard.enumeration)) +
                    "::" + context_.text(context_.declarations.enumerators.name[values[ordinal]]));
            }
        }
        std::unreachable();
    }
    auto guard_check(schema::BitGuard const& guard, std::optional<Expr> source = {}) -> Expr {
        return binary(
            BinaryOperator::equal, extract(guard.path + "_layout", source), guard_code(guard));
    }
    auto value_check(schema::BitPlacement const& placement, Expr expression, bool stored = false)
        -> Predicate {
        if (aggregate(placement)) {
            return member_call(expression, "is_valid");
        }
        auto const accessor{field_name(placement)};
        auto const& check{checks_.at(accessor)};
        if (stored) {
            return check.stored_helper ? Predicate{invoke(*check.stored_helper, {expression})}
                                       : Predicate{};
        }
        return check.input_required ? Predicate{invoke("is_valid_" + accessor, {expression})}
                                    : Predicate{};
    }
    auto valid_children(std::optional<std::uint32_t> parent,
                        schema::TypeId child_type,
                        std::optional<Expr> source) -> Predicate {
        auto const union_{graph_.definition(child_type).kind() ==
                          schema::TypeDefinitionKind::union_type};
        std::vector<Predicate> checks;
        std::map<schema::ChildOccurrenceId, std::vector<Predicate>> alternatives;
        auto const count{layout_.placements.size()};
        for (std::size_t index{}; index < count; ++index) {
            auto const& placement{layout_.placements[index]};
            if (placement.parent != parent || placement.reserved) {
                continue;
            }
            auto check{
                aggregate(placement)
                    ? valid_children(static_cast<std::uint32_t>(index), placement.type, source)
                    : value_check(
                          placement, extract(field_name(placement) + "_layout", source), true)};
            for (auto const& guard : placement.guards) {
                if (parent && std::ranges::any_of(layout_.placements[*parent].guards,
                                                  [&](auto const& inherited) {
                                                      return inherited.offset == guard.offset &&
                                                             inherited.bits == guard.bits &&
                                                             inherited.value == guard.value;
                                                  })) {
                    continue;
                }
                check = all({guard_check(guard, source), check});
            }
            if (union_) {
                alternatives[placement.child].push_back(check);
            } else {
                checks.push_back(check);
            }
        }
        for (auto const& [child, elements] : alternatives) {
            checks.push_back(all(elements));
        }
        auto result{union_ ? any(checks) : all(checks)};
        if (parent) {
            auto const& placement{layout_.placements[*parent]};
            if (auto const invalid{record_invalid(child_type)};
                invalid && *invalid <= mask(placement.bits)) {
                auto const constant_{constant(field_metadata_[*parent],
                                              placement.path + "_invalid_value",
                                              "storage_type",
                                              number(*invalid))};
                result = all({binary(BinaryOperator::not_equal,
                                     extract(field_name(placement) + "_layout", source),
                                     constant_),
                              result});
            }
        }
        return result;
    }
    auto selected(std::uint32_t index, std::optional<std::uint32_t> choice) const -> bool {
        auto const& placement{layout_.placements[index]};
        if (!placement.alternative) {
            return true;
        }
        for (auto current{choice}; current; current = layout_.placements[*current].parent) {
            auto const& selected_placement{layout_.placements[*current]};
            if (*current == index || (placement.child == selected_placement.child &&
                                      placement.parent == selected_placement.parent)) {
                return true;
            }
        }
        return false;
    }
    void parameters(std::optional<std::uint32_t> parent,
                    std::optional<std::uint32_t> choice,
                    std::vector<std::uint32_t>& result) const {
        auto const count{layout_.placements.size()};
        for (std::size_t index{}; index < count; ++index) {
            auto const& placement{layout_.placements[index]};
            if (placement.parent != parent || placement.reserved ||
                !selected(static_cast<std::uint32_t>(index), choice)) {
                continue;
            }
            if (placement.discriminator && choice &&
                std::ranges::any_of(layout_.placements[*choice].guards, [&](auto const& guard) {
                    return guard.offset == placement.offset && guard.bits == placement.bits;
                })) {
                continue;
            }
            bool contains_choice{};
            for (auto current{choice}; current; current = layout_.placements[*current].parent) {
                if (*current == index) {
                    contains_choice = true;
                    break;
                }
            }
            auto const union_region{graph_.definition(placement.type).kind() ==
                                    schema::TypeDefinitionKind::union_type};
            if ((addressable(placement) && (!contains_choice || choice == index)) ||
                (union_region && !contains_choice) ||
                (addressable(placement) && !aggregate(placement))) {
                result.push_back(static_cast<std::uint32_t>(index));
            } else {
                parameters(static_cast<std::uint32_t>(index), choice, result);
            }
        }
    }
    void constructor(std::optional<std::uint32_t> choice) {
        std::vector<std::uint32_t> inputs;
        parameters(std::nullopt, choice, inputs);
        if (inputs.empty() && !choice) {
            return;
        }
        std::vector<FunctionParameterId> params;
        Nodes body;
        auto const result{ir_.named("result")};
        auto const target{choice ? ir_.member_access(result, "value_") : raw()};
        if (choice) {
            body.push_back(ir_.variable("auto", "result", invoke("from_raw", {number(0)})));
            for (auto const& guard : layout_.placements[*choice].guards) {
                body.push_back(insert_member(guard.path + "_layout", guard_code(guard), result));
            }
        } else if (layout_.default_raw_value.value_or(0) != 0) {
            body.push_back(ir_.assignment(raw(), number(0)));
        }
        bool validate_record{};
        for (auto const index : inputs) {
            auto const& input{layout_.placements[index]};
            auto const name{field_name(input)};
            auto const argument_name{scalar_ ? "value"
                                             : name + (addressable(input) ? "_value" : "_encoded")};
            auto const argument{ir_.named(argument_name)};
            params.push_back(parameter(access_type(input), argument_name));
            validate_record = validate_record || input.discriminator;
            if (!addressable(input)) {
                validate_record = true;
                if (input.bits < layout_.storage_bits) {
                    body.push_back(assertion(
                        binary(BinaryOperator::less_equal,
                               argument,
                               ir_.member_access(ir_.named(name + "_layout"), "value_mask"))));
                }
                body.push_back(insert_member(name + "_layout",
                                             cast("storage_type", argument),
                                             choice ? std::optional<Expr>{result} : std::nullopt));
            } else {
                assert_if(body, value_check(input, argument));
                body.push_back(insert_member(
                    name + "_layout",
                    aggregate(input) ? cast("storage_type", member_call(argument, "raw_value"))
                                     : argument,
                    choice ? std::optional<Expr>{result} : std::nullopt));
            }
        }
        if (validate_record) {
            body.push_back(
                assertion(choice ? member_call(result, "is_valid") : invoke("is_valid")));
        }
        if (record_invalid(type_)) {
            body.push_back(
                assertion(binary(BinaryOperator::not_equal, target, ir_.named("invalid_value"))));
        }
        if (choice) {
            body.push_back(ir_.return_stmt(result));
            auto const& placement{layout_.placements[*choice]};
            auto path{placement.path};
            auto const& children{graph_.definitions().children};
            if (children.count[children.index(placement.child)]) {
                path.resize(path.rfind('_'));
            }
            functions_.push_back(
                function("make_" + path, name_, FunctionReceiver::static_, params, body));
        } else {
            auto const ctor{ir_.constructor(text(name_), true)};
            ir_.function_flags(ctor).set_is_constexpr(true);
            ir_.function_flags(ctor).set_define_in_header(true);
            ir_.set_function_noexcept(ctor);
            ir_.set_function_parameters(ctor, params);
            ir_.set_function_body(ctor, body);
            constructors_.push_back(ir_.header_function(ctor));
        }
    }
    void accessor(std::uint32_t index) {
        auto const& placement{layout_.placements[index]};
        auto const name{field_name(placement)};
        Nodes guards;
        for (auto const& guard : placement.guards) {
            guards.push_back(assertion(guard_check(guard)));
        }
        auto body{guards};
        auto const decoded{extract(name + "_layout")};
        auto const decoded_type{access_type(placement)};
        body.push_back(ir_.return_stmt(
            aggregate(placement)
                ? ir_.call(ir_.type_reference(ir_.types.decorate(decoded_type, "", "::from_raw")),
                           {cast(ir_.types.decorate(decoded_type, "", "::storage_type"), decoded)})
                : decoded));
        functions_.push_back(function(name, decoded_type, FunctionReceiver::const_, {}, body));
        if (placement.discriminator) {
            return;
        }
        body = std::move(guards);
        assert_if(body, value_check(placement, value()));
        auto const encoded{aggregate(placement)
                               ? cast("storage_type", member_call(value(), "raw_value"))
                               : value()};
        auto const& children{graph_.definitions().children};
        if (!scalar_ && children.tag[children.index(placement.child)]) {
            body.push_back(
                ir_.variable("auto const", "updated", insert(name + "_layout", raw(), encoded)));
            assert_if(body, valid_children(index, placement.type, ir_.named("updated")));
            body.push_back(ir_.assignment(raw(), ir_.named("updated")));
        } else {
            body.push_back(insert_member(name + "_layout", encoded));
        }
        functions_.push_back(function("set_" + name,
                                      "void",
                                      FunctionReceiver::unqualified,
                                      {parameter(decoded_type, "value")},
                                      body));
        if (!scalar_ &&
            graph_.definition(placement.type).kind() == schema::TypeDefinitionKind::fixed_point) {
            fixed_conversion(index);
        }
    }

    void fixed_conversion(std::uint32_t index) {
        auto const& placement{layout_.placements[index]};
        auto const definition{graph_.definition(placement.type)};
        auto const& fixed{graph_.definitions().fixed_points};
        auto const row{fixed.index(definition)};
        auto const signedness{fixed.signedness[row] != 0};
        auto const& path{placement.path};
        auto& metadata{field_metadata_[index]};
        auto const fractional{constant(
            metadata, path + "_fractional_bits", "int", number(fixed.fractional_bits[row]))};
        auto const magnitude{ir_.member_access(ir_.named(path + "_raw_layout"), "magnitude_bits")};
        auto const lower{range_bound(path + "_raw_layout", "min")};
        auto const upper{range_bound(path + "_raw_layout", "max")};
        auto const negative_fractional{ir_.unary(UnaryOperator::negate, fractional)};
        auto const ldexp = [&](Expr mantissa, Expr exponent) {
            return library_call("std::ldexp", "cmath", {mantissa, exponent});
        };
        auto const not_finite = [&](Expr value) {
            return ir_.unary(UnaryOperator::logical_not,
                             library_call("std::isfinite", "cmath", {value}));
        };
        auto const fail_if = [&](Expr condition) {
            return ir_.if_stmt(condition, ir_.block({ir_.return_stmt(ir_.literal("false"))}));
        };
        functions_.push_back(function(
            path + "_value",
            "double",
            FunctionReceiver::const_,
            {},
            {ir_.return_stmt(ldexp(cast("double", invoke(path + "_raw")), negative_fractional))},
            false));
        std::vector<Predicate> invalid_inputs{
            not_finite(value()),
            binary(
                BinaryOperator::less, value(), ldexp(cast("double", lower), negative_fractional)),
            binary(BinaryOperator::greater,
                   value(),
                   ldexp(cast("double", upper), negative_fractional))};
        Nodes body{fail_if(boolean(any(invalid_inputs))),
                   ir_.variable("auto const", "scaled", ldexp(value(), fractional)),
                   fail_if(not_finite(ir_.named("scaled")))};
        auto const scaled{ir_.named("scaled")};
        auto const rounded{ir_.named("rounded")};
        if (fixed.rounding[row] == FixedPointRounding::toward_zero) {
            body.push_back(ir_.variable(
                "auto const", "rounded", library_call("std::trunc", "cmath", {scaled})));
        } else {
            body.push_back(
                ir_.variable("auto", "rounded", library_call("std::floor", "cmath", {scaled})));
            body.push_back(ir_.variable(
                "auto const", "fraction", binary(BinaryOperator::subtract, scaled, rounded)));
            auto const fraction{ir_.named("fraction")};
            auto const tie{
                binary(BinaryOperator::logical_and,
                       binary(BinaryOperator::equal, fraction, ir_.literal("0.5")),
                       binary(BinaryOperator::not_equal,
                              library_call("std::fmod", "cmath", {rounded, ir_.literal("2.0")}),
                              ir_.literal("0.0")))};
            auto const round_up{
                binary(BinaryOperator::logical_or,
                       binary(BinaryOperator::greater, fraction, ir_.literal("0.5")),
                       tie)};
            body.push_back(ir_.if_stmt(
                round_up,
                ir_.block({ir_.assignment(
                    rounded, binary(BinaryOperator::add, rounded, ir_.literal("1.0")))})));
        }
        auto const range{ldexp(ir_.literal("1.0"), magnitude)};
        body.push_back(fail_if(binary(
            BinaryOperator::logical_or,
            binary(BinaryOperator::greater_equal, rounded, range),
            binary(BinaryOperator::less,
                   rounded,
                   signedness ? ir_.unary(UnaryOperator::negate, range) : ir_.literal("0.0")))));
        auto const raw_type{descriptor_value(path + "_raw_layout")};
        body.push_back(ir_.variable(
            ir_.types.decorate(raw_type, "", " const"), "raw", cast(raw_type, rounded)));
        if (auto const valid{value_check(placement, ir_.named("raw"))}) {
            body.push_back(fail_if(ir_.unary(UnaryOperator::logical_not, *valid)));
        }
        body.push_back(ir_.assignment(ir_.named("out_raw"), ir_.named("raw")));
        body.push_back(ir_.return_stmt(ir_.literal("true")));
        functions_.push_back(
            function("try_encode_" + path + "_value",
                     "bool",
                     FunctionReceiver::static_,
                     {parameter("double", "value"),
                      ir_.parameter(ir_.types.decorate(raw_type, "", "&"), "out_raw")},
                     body,
                     false));
        functions_.push_back(function(
            "set_" + path + "_value",
            "void",
            FunctionReceiver::unqualified,
            {parameter("double", "value")},
            {ir_.variable(raw_type, "raw", ir_.init_list({})),
             ir_.variable(
                 ir_.types.decorate(ir_.types.intern("auto"), "[[maybe_unused]] ", " const"),
                 "success",
                 invoke("try_encode_" + path + "_value", {value(), ir_.named("raw")})),
             assertion(ir_.named("success")),
             ir_.expr_stmt(invoke("set_" + path + "_raw", {ir_.named("raw")}))},
            false));
    }

    LoweringContext& context_;
    CppIR& ir_;
    schema::TypeGraph const& graph_;
    schema::TypeId type_;
    schema::BitLayout layout_;
    std::string name_;
    bool scalar_;
    Nodes metadata_;
    std::vector<Nodes> field_metadata_;
    Nodes constructors_;
    Nodes operators_;
    Nodes functions_;
    Nodes private_functions_;
    std::vector<TypeDependencyId> dependencies_;
    std::set<std::string> constants_;
    std::map<std::string, Validation> checks_;
};

} // namespace

auto lower_packed_value(LoweringContext& context, DeclarationHandle, schema::TypeId type)
    -> DeclarationEmission {
    return {.header = PackedEmitter{context, type}.emit()};
}

} // namespace lispb::codegen::detail
