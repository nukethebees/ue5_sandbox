#include "declaration_emission.h"
#include "lowering.h"
#include "lowering_context.h"
#include "packed_value_internal.h"
#include <ioj/min_max_formatter.h>

#include <codegen/ast/cpp_ir_builder.h>
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
#include <cmath>
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

auto packed_literal(PackedIntegerValue value) -> std::string {
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

auto consecutive(PackedIntegerValue left, PackedIntegerValue right) -> bool {
    if (left.negative) {
        return left.magnitude == 1 ? right.magnitude == 0
                                   : right.negative && right.magnitude == left.magnitude - 1;
    }
    return !right.negative && left.magnitude != (std::numeric_limits<std::uint64_t>::max)() &&
           right.magnitude == left.magnitude + 1;
}

auto accessor_name(LoweringContext& ctx, schema::BitPlacement const& placement) -> std::string {
    switch (ctx.semantic.definition(placement.type).kind()) {
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

class PackedEmitter : public CppIRBuilder {
  public:
    PackedEmitter(LoweringContext& ctx, schema::TypeId type)
        : CppIRBuilder{ctx.ir}
        , context_{ctx}
        , graph_{ctx.semantic}
        , type_{type}
        , layout_{graph_.bit_layout(type)}
        , name_{ctx.text(graph_.name(type))}
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
        auto default_value{intern_type("0")};
        auto const base{named_type("ioj::PackedValueBase",
                                   NameOrigin::global,
                                   {dependency("ioj::PackedValueBase", "ioj/packed_value.h")})};
        auto const storage_type{std_types().integer(layout_.storage_bits)};
        if (layout_.default_raw_value.value_or(0) != 0) {
            auto const default_name{soa_namespace_name(name_) + "_default_raw_value"};
            auto const default_symbol{type(detail_namespace + default_name, NameOrigin::global)};
            detail_members.push_back(variable(decorate_type(storage_type, "inline constexpr ", ""),
                                              text(default_name),
                                              number(*layout_.default_raw_value)));
            default_value = default_symbol;
        }
        auto const descriptor{
            template_type(named_type("ioj::PackedStorage",
                                     NameOrigin::global,
                                     {dependency("ioj::PackedStorage", "ioj/packed_value.h")}),
                          {storage_type, intern_type(std::to_string(*layout_.encoded_bits))})};
        auto const storage{compose_type({{.type = descriptor},
                                         {.text = text("{")},
                                         {.type = default_value},
                                         {.text = text("}")}})};
        auto const base_type{template_type(base, {type(name_), storage})};
        auto const base_name{name_ + "Base"};
        auto const base_alias{type(detail_namespace + base_name, NameOrigin::global)};
        detail_members.push_back(using_declaration(text(base_name), base_type));
        metadata_.push_back(line_comment("Storage"));
        metadata_.push_back(using_declaration(text("Base"), base_alias));
        metadata_.push_back(using_declaration(
            text("packed_integer_tag"),
            named_type(scalar_ ? "ioj::packed::scalar_tag" : "ioj::packed::integer_tag",
                       NameOrigin::global,
                       {dependency("ioj::PackedStorage", "ioj/packed_value.h")})));
        packed_members();
        Nodes members{access()};
        members.push_back(lines(1));
        append(members, metadata_);
        auto const count{layout_.placements.size()};
        for (std::size_t index{}; index < count; ++index) {
            if (!field_metadata_[index].empty()) {
                members.push_back(lines(2));
                members.push_back(line_comment(layout_.placements[index].path));
                members.push_back(lines(1));
                append(members, field_metadata_[index]);
            }
        }
        for (auto const* group : {&constructors_, &operators_, &functions_}) {
            if (!group->empty()) {
                members.push_back(lines(2));
                append(members, *group);
            }
        }
        if (!private_functions_.empty()) {
            members.push_back(lines(2));
            members.push_back(access(Access::private_));
            members.push_back(lines(1));
            append(members, private_functions_);
        }
        auto const record_id{record(text(name_), members)};
        set_record_bases(record_id, {base_alias});
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
        set_scope_bindings(record_id, inherited);
        auto const declaration_id{graph_.source_declaration(type_)};
        auto const kind{graph_.definition(type_).kind()};
        StringId export_specifier{};
        if (kind == schema::TypeDefinitionKind::record) {
            export_specifier = context_.declarations.records.export_specifier[declaration_id];
        } else if (kind == schema::TypeDefinitionKind::union_type) {
            export_specifier = context_.declarations.unions.export_specifier[declaration_id];
        }
        set_record_export_specifier(record_id, export_specifier);
        set_record_dependencies(record_id, dependencies_);
        return {forward_declaration(text(name_), RecordKind::struct_, {}, export_specifier),
                namespace_node(text("detail"), detail_members),
                record_id,
                static_assertion(template_call(
                    "ioj::valid_packed_value_layout", "ioj/packed_value.h", {type(name_)}, {}))};
    }
  private:
    auto value_type(LoweringContext& ctx, schema::TypeId type, std::uint32_t bits) -> CppTypeId {
        auto const& graph{ctx.semantic};
        auto const definition{graph.definition(type)};
        auto const kind{definition.kind()};
        auto const& definitions{graph.definitions()};
        if (composite(kind) && schema::is_packed_projection(graph, type)) {
            return named_type(ctx.text(graph.cpp_spelling(type)), NameOrigin::global);
        }
        if (kind == schema::TypeDefinitionKind::enumeration) {
            auto const declaration_id{graph.source_declaration(type)};
            if (ctx.declarations.enums.cpp_projection[declaration_id]) {
                return named_type(ctx.text(graph.cpp_spelling(type)), NameOrigin::global);
            }
        }
        if (ctx.text(graph.cpp_spelling(type)) == "bool") {
            return std_types().bool_;
        }
        if (auto const domain{schema::integer_domain(graph, type)}) {
            auto const row{definitions.integer_scalars.index(*domain)};
            return std_types().integer(bits, definitions.integer_scalars.signedness[row] != 0);
        }
        if (kind == schema::TypeDefinitionKind::fixed_point) {
            return std_types().integer(bits, definitions.fixed_points.signedness[definition] != 0);
        }
        if (packed_signed_width(ctx.text(graph.cpp_spelling(type)))) {
            return std_types().integer(bits, true);
        }
        return std_types().integer(bits);
    }
    void append_dependencies(LoweringContext& ctx,
                             schema::ChildOccurrenceId child,
                             std::vector<TypeDependencyId>& dependencies) {
        auto const& children{ctx.semantic.definitions().children};
        auto const occurrence{children.semantic_type[child]};
        auto const& occurrences{ctx.semantic.occurrences()};
        auto const type{occurrences.cpp_type[occurrence]};
        if (type.value == 0) {
            return;
        }
        auto const required{type_dependencies(type)};
        dependencies.insert(dependencies.end(), required.begin(), required.end());
    }
    using Predicate = std::optional<Expr>;
    struct Validation {
        bool input_required{};
        std::optional<std::string> stored_helper;
    };

    void append(Nodes& target, Nodes const& source) {
        bool first{true};
        for (auto const node : source) {
            if (!first) {
                target.push_back(lines(1));
            }
            target.push_back(node);
            first = false;
        }
    }
    auto text(std::string_view value) -> StringId { return store_text(value); }
    auto type(std::string_view value, NameOrigin origin = NameOrigin::enclosing) -> CppTypeId {
        return named_type(value, origin);
    }
    auto number(std::uint64_t value) -> Expr {
        return literal(
            std::to_string(value) +
            (value > static_cast<std::uint64_t>((std::numeric_limits<std::int64_t>::max)()) ? "ULL"
                                                                                            : ""));
    }
    auto integer(PackedIntegerValue value) -> Expr { return literal(packed_literal(value)); }
    auto value() -> Expr { return named("value"); }
    auto stored_value() -> Expr { return named("value_"); }
    auto external(std::string_view name, std::string_view header) -> Expr {
        return type_reference(named_type(name, NameOrigin::global, {dependency(name, header)}));
    }
    auto invoke(std::string_view name, std::initializer_list<Expr> args = {}) -> Expr {
        return call(named(name), args);
    }
    auto template_call(std::string_view name,
                       std::string_view header,
                       std::initializer_list<CppTypeId> types,
                       std::initializer_list<Expr> args) -> Expr {
        std::vector<Expr> arguments;
        for (auto const item : types) {
            arguments.push_back(type_reference(item));
        }
        return call(template_id(external(name, header), arguments), args);
    }
    auto extract(std::string const& field, std::optional<Expr> source = {}) -> Expr {
        return source
                 ? template_call("ioj::packed_extract",
                                 "ioj/packed_value.h",
                                 {descriptor_type(field)},
                                 {*source})
                 : call(template_id(named("extract"), {type_reference(descriptor_type(field))}),
                        {});
    }
    auto insert_member(std::string const& field, Expr source, std::optional<Expr> target = {})
        -> NodeId {
        return expr_stmt(
            call(template_id(target ? member_access(*target, "insert") : named("insert"),
                             {type_reference(descriptor_type(field))}),
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
        return decltype_type(named(name));
    }
    auto descriptor_value(std::string const& name) -> CppTypeId {
        return field_value_types_.at(name);
    }
    auto cast(CppTypeId target, Expr source) -> Expr { return static_cast_expr(target, source); }
    auto cast(std::string const& target, Expr source) -> Expr {
        return static_cast_expr(type(target), source);
    }
    auto member_call(Expr target,
                     std::string_view member,
                     std::initializer_list<Expr> arguments = {}) -> Expr {
        return call(member_access(target, member), arguments);
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
        return result.value_or(literal("false"));
    }
    auto boolean(Predicate predicate) -> Expr { return predicate.value_or(literal("true")); }
    void assert_if(Nodes& body, Predicate predicate) {
        if (predicate) {
            body.push_back(assertion(*predicate));
        }
    }
    void alias(Nodes& nodes, std::string const& name, CppTypeId target) {
        nodes.push_back(using_declaration(text(name), target));
    }
    auto constant(Nodes& nodes, std::string const& name, CppTypeId target, Expr initializer)
        -> Expr {
        if (constants_.insert(name).second) {
            nodes.push_back(constexpr_member(target, text(name), initializer));
        }
        return named(name);
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
        field_value_types_.emplace(name, decoded);
        auto const& definitions{graph_.definitions()};
        auto const definition{graph_.definition(logical)};
        std::optional<PackedIntegerValue> minimum;
        std::optional<PackedIntegerValue> maximum;
        if (auto const domain{schema::integer_domain(graph_, logical)}) {
            auto const row{definitions.integer_scalars.index(*domain)};
            minimum = definitions.integer_scalars.minimum_value[row];
            maximum = definitions.integer_scalars.maximum_value[row];
        } else if (definition.kind() == schema::TypeDefinitionKind::enumeration) {
            auto const declaration_id{graph_.source_declaration(logical)};
            if (!context_.declarations.enums.cpp_projection[declaration_id]) {
                auto const domain{definitions.enums.domain[definition]};
                if (auto const range{graph_.enum_domains().contiguous_value_range(domain)}) {
                    minimum = PackedIntegerValue{range->min};
                    maximum = PackedIntegerValue{range->max};
                }
            }
        } else if (definition.kind() == schema::TypeDefinitionKind::linear_quantized) {
            maximum = PackedIntegerValue{mask(bits) -
                                         definitions.linear_quantized.reserved_codes[definition]};
        }
        auto initializer{init_list({})};
        auto const signedness{std_types().is_signed_integer(value_type(context_, logical, bits))};
        auto const physical_min{PackedIntegerValue::from_parts(
            signedness, signedness ? std::uint64_t{1} << (bits - 1) : 0)};
        auto const physical_max{PackedIntegerValue{mask(bits - (signedness ? 1 : 0))}};
        auto const lower{minimum.value_or(physical_min)};
        auto const upper{maximum.value_or(physical_max)};
        if (lower != physical_min || upper != physical_max) {
            initializer = init_list(
                {init_list({cast(decoded, integer(lower)), cast(decoded, integer(upper))})});
        }
        auto const packed{named_type("ioj::PackedField",
                                     NameOrigin::global,
                                     {dependency("ioj::PackedField", "ioj/packed_value.h")})};
        auto const descriptor{template_type(
            packed,
            {type("storage_type"),
             decoded,
             intern_type(std::to_string(offset)),
             intern_type(logical == type_ && !composite(graph_.definition(type_).kind())
                             ? "storage.encoded_bits"
                             : std::to_string(bits))})};
        constant(metadata, name, descriptor, initializer);
    }
    auto range_bound(std::string const& layout, std::string_view bound) -> Expr {
        return member_access(member_access(named(layout), "range"), bound);
    }
    auto const_parameter(CppTypeId alias, std::string_view name) -> FunctionParameterId {
        return parameter(decorate_type(alias, "", " const"), name);
    }
    auto emit_function(std::string const& name,
                       CppTypeId result,
                       FunctionReceiver receiver,
                       std::span<FunctionParameterId const> parameters,
                       Nodes const& body,
                       bool constexpr_ = true) -> NodeId {
        auto const fn{function(name, result)};
        set_function_receiver(fn, receiver);
        set_function_noexcept(fn);
        set_function_parameters(fn, parameters);
        set_function_return_type(fn,
                                 result,
                                 result == std_types().void_ ? ReturnSyntax::direct
                                                             : ReturnSyntax::trailing);
        function_flags(fn).set_is_constexpr(constexpr_);
        function_flags(fn).set_is_nodiscard(result != std_types().void_);
        function_flags(fn).set_define_in_header(true);
        set_function_body(fn, body);
        return header_function(fn);
    }
    auto emit_function(std::string const& name,
                       CppTypeId result,
                       FunctionReceiver receiver,
                       std::initializer_list<FunctionParameterId> parameters,
                       Nodes const& body,
                       bool constexpr_ = true) -> NodeId {
        return emit_function(name,
                             result,
                             receiver,
                             std::span<FunctionParameterId const>{parameters},
                             body,
                             constexpr_);
    }
    auto emit_function(std::string const& name,
                       std::string const& result,
                       FunctionReceiver receiver,
                       std::span<FunctionParameterId const> parameters,
                       Nodes const& body,
                       bool constexpr_ = true) -> NodeId {
        return emit_function(name, type(result), receiver, parameters, body, constexpr_);
    }
    auto default_constructor(bool available) -> NodeId {
        auto const fn{constructor(text(name_))};
        function_flags(fn).set_is_constexpr(true);
        function_flags(fn).set_define_in_header(true);
        set_function_noexcept(fn);
        if (available) {
            set_function_defaulted(fn);
        } else {
            set_function_deleted(fn);
        }
        return header_function(fn);
    }
    void comparison(bool defaulted) {
        auto const owner{decorate_type(type(name_), "", " const&")};
        for (auto const equality : {false, true}) {
            if (equality && defaulted) {
                continue;
            }
            auto const decoded{!defaulted && !equality};
            auto const fn{function(equality ? "operator==" : "operator<=>",
                                   equality
                                       ? std_types().bool_
                                       : intern_type("auto", {dependency("auto", "compare")}))};
            set_function_receiver(fn, FunctionReceiver::const_);
            set_function_noexcept(fn);
            function_flags(fn).set_is_constexpr(true);
            function_flags(fn).set_define_in_header(true);
            function_flags(fn).set_is_nodiscard(true);
            set_function_parameters(fn, {parameter(owner, decoded ? "other" : "")});
            if (decoded) {
                set_function_body(fn,
                                  {return_stmt(binary(BinaryOperator::three_way,
                                                      invoke("value"),
                                                      member_call(named("other"), "value")))});
            } else {
                set_function_defaulted(fn);
            }
            operators_.push_back(header_function(fn));
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
            auto const named_codes{context_.declarations.items(scalars.named_codes[row])};
            for (std::size_t pass{}; pass < named_codes.size(); ++pass) {
                for (auto const id : named_codes) {
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
            for (auto const id : named_codes) {
                if (codes.sentinel[id]) {
                    auto const name{qualify(context_.text(codes.name[id]))};
                    if (!field_range && !prefix.empty()) {
                        bound(context_.text(codes.name[id]), codes.value[id]);
                    }
                    alternatives.push_back(binary(BinaryOperator::equal, expression, named(name)));
                }
            }
            return any(alternatives);
        }
        switch (definition.kind()) {
            case schema::TypeDefinitionKind::enumeration: {
                auto const domain{definitions.enums.domain[definition]};
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
                auto const count_value{domains.count_value[domain]};
                std::vector<Predicate> checks;
                auto const count{enumerators.size()};
                for (std::size_t ordinal{}; ordinal < count; ++ordinal) {
                    if (enumerators[ordinal] == count_value) {
                        continue;
                    }
                    auto const enumerator{context_.text(
                        context_.declarations.enumerators.name[enumerators[ordinal]])};
                    auto const code{domains.code(domain, ordinal)};
                    auto const declaration_id{graph_.source_declaration(scalar)};
                    auto const projected{
                        context_.declarations.enums.cpp_projection[declaration_id].has_value()};
                    auto const enumerator_value{
                        projected ? type_reference(type(context_.text(graph_.cpp_spelling(scalar)) +
                                                            "::" + enumerator,
                                                        NameOrigin::global))
                                  : cast(decoded,
                                         integer(PackedIntegerValue::from_parts(code->negative,
                                                                                code->magnitude)))};
                    checks.push_back(binary(BinaryOperator::equal, expression, enumerator_value));
                }
                return any(checks);
            }
            case schema::TypeDefinitionKind::linear_quantized: {
                auto const& quantized{definitions.linear_quantized};
                auto const reserved{quantized.reserved_codes[definition]};
                auto const maximum{range_bound(layout, "max")};
                return reserved ? Predicate{binary(BinaryOperator::less_equal, expression, maximum)}
                                : Predicate{};
            }
            case schema::TypeDefinitionKind::optional_sentinel:
            case schema::TypeDefinitionKind::optional_presence_bit: {
                auto const sentinel{definition.kind() ==
                                    schema::TypeDefinitionKind::optional_sentinel};
                auto const source_occurrence{
                    sentinel ? definitions.optional_sentinels.source[definition]
                             : definitions.optional_presence_bits.source[definition]};
                auto const source{graph_.occurrences().type[source_occurrence]};
                auto const payload_bits{
                    sentinel ? bits : definitions.optional_presence_bits.payload_bits[definition]};
                auto const payload{qualify("payload")};
                field_layout(metadata,
                             payload + "_layout",
                             source,
                             payload_bits,
                             value_type(context_, source, payload_bits));
                auto const payload_alias{descriptor_value(payload + "_layout")};
                auto const decoded_source{
                    extract(payload + "_layout", cast("storage_type", expression))};
                auto const source_check{scalar_domain(
                    source, payload_bits, decoded_source, payload, payload_alias, metadata)};
                Expr absent;
                if (sentinel) {
                    absent =
                        binary(BinaryOperator::equal,
                               expression,
                               bound("sentinel",
                                     definitions.optional_sentinels.sentinel_value[definition]));
                } else {
                    auto const presence{binary(BinaryOperator::shift_left,
                                               init_list({number(1)}, type("storage_type")),
                                               member_access(named(payload + "_layout"), "bits"))};
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
            auto const domain{definitions.enums.domain[definition]};
            auto const& domains{graph_.enum_domains()};
            auto const enumerators{domains.values(domain)};
            auto const count_value{domains.count_value[domain]};
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
        return definition.kind() == schema::TypeDefinitionKind::linear_quantized;
    }
    auto representation(schema::TypeId logical,
                        CppTypeId decoded,
                        std::string const& field,
                        std::uint32_t bits,
                        bool checked_domain) -> Predicate {
        if (graph_.definition(logical).kind() == schema::TypeDefinitionKind::enumeration) {
            auto const declaration_id{graph_.source_declaration(logical)};
            auto const& enums{context_.declarations.enums};
            if (auto const& projection{enums.cpp_projection[declaration_id]}) {
                std::optional<std::uint32_t> underlying_bits;
                auto const explicit_type{projection->storage
                                             ? projection->storage
                                             : enums.underlying_type[declaration_id]};
                if (explicit_type) {
                    auto const occurrence{graph_.resolved(graph_.module(logical), *explicit_type)};
                    auto const& occurrences{graph_.occurrences()};
                    auto const underlying{occurrences.cpp_type[occurrence]};
                    if (auto const width{packed_unsigned_width(type_spelling(underlying))}) {
                        underlying_bits = static_cast<std::uint32_t>(*width);
                    }
                } else {
                    auto const& domains{graph_.enum_domains()};
                    if (auto const requirement{schema::derive_enum_storage_requirement(
                            domains,
                            domains.find(declaration_id),
                            enums.bit_width[declaration_id])}) {
                        underlying_bits = requirement->bit_width;
                    }
                }
                if ((underlying_bits && *underlying_bits <= bits) ||
                    (checked_domain && domain_implies_width(logical, bits))) {
                    return {};
                }
                return binary(BinaryOperator::less_equal,
                              call(type_reference(std_types().to_underlying), {value()}),
                              member_access(named(field), "value_mask"));
            }
        }
        if (decoded == std_types().bool_ || bits == 8 || bits == 16 || bits == 32 || bits == 64) {
            return {};
        }
        if (checked_domain && domain_implies_width(logical, bits)) {
            return {};
        }
        return std_types().is_signed_integer(decoded)
                 ? binary(BinaryOperator::equal, extract(field, pack(field, value())), value())
                 : binary(BinaryOperator::less_equal,
                          cast("storage_type", value()),
                          member_access(named(field), "value_mask"));
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
                private_functions_.push_back(emit_function(*result.stored_helper,
                                                           std_types().bool_,
                                                           FunctionReceiver::static_,
                                                           {const_parameter(alias, "value")},
                                                           {return_stmt(*domain)}));
                domain = invoke(*result.stored_helper, {value()});
            }
        }
        functions_.push_back(
            emit_function(name,
                          std_types().bool_,
                          FunctionReceiver::static_,
                          {const_parameter(alias, result.input_required ? "value" : "")},
                          {return_stmt(boolean(all({domain, width})))}));
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
                 ? type(context_.text(graph_.cpp_spelling(placement.type)), NameOrigin::global)
                 : descriptor_value(field_name(placement) + "_layout");
    }
    auto record_invalid(schema::TypeId record_id) const -> std::optional<std::uint64_t> {
        auto const definition{graph_.definition(record_id)};
        if (definition.kind() != schema::TypeDefinitionKind::record) {
            return {};
        }
        auto const& records{graph_.definitions().records};
        return records.invalid_raw_value[definition];
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
                         ? member_access(named("storage"), "default_raw_value")
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
                                   : type("storage_type")};
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
                         scalar_                ? type("value_type")
                         : aggregate(placement) ? type("storage_type")
                                                : decoded,
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
            build_constructor(std::nullopt);
        }
        comparison(!scalar_ || !std_types().is_signed_integer(
                                   value_type(context_, type_, *layout_.encoded_bits)));
        std::set<std::pair<std::optional<std::uint32_t>, schema::ChildOccurrenceId>> alternatives;
        for (std::size_t index{}; index < count; ++index) {
            auto const& placement{layout_.placements[index]};
            if (placement.alternative &&
                alternatives.emplace(placement.parent, placement.child).second) {
                build_constructor(static_cast<std::uint32_t>(index));
            }
        }
        auto validity{valid_children(std::nullopt, type_, std::nullopt)};
        if (validity) {
            auto const base_validity{
                call(type_reference(decorate_type(type("Base"), "", "::is_valid")), {})};
            validity = all({base_validity, validity});
            functions_.push_back(emit_function("is_valid",
                                               std_types().bool_,
                                               FunctionReceiver::const_,
                                               {},
                                               {return_stmt(*validity)}));
        }
        for (std::size_t index{}; index < count; ++index) {
            if (addressable(layout_.placements[index])) {
                accessor(static_cast<std::uint32_t>(index));
            }
        }
    }
    auto guard_code(schema::BitGuard const& guard) -> Expr {
        auto const declaration_id{graph_.source_declaration(guard.enumeration)};
        if (!context_.declarations.enums.cpp_projection[declaration_id]) {
            return cast(descriptor_value(guard.path + "_layout"), number(guard.value));
        }
        auto const definition{graph_.definition(guard.enumeration)};
        auto const& enums{graph_.definitions().enums};
        auto const domain{enums.domain[definition]};
        auto const& domains{graph_.enum_domains()};
        auto const values{domains.values(domain)};
        auto const count{values.size()};
        for (std::size_t ordinal{}; ordinal < count; ++ordinal) {
            auto const code{domains.code(domain, ordinal)};
            if (code && !code->negative && code->magnitude == guard.value) {
                return type_reference(
                    type(context_.text(graph_.cpp_spelling(guard.enumeration)) + "::" +
                             context_.text(context_.declarations.enumerators.name[values[ordinal]]),
                         NameOrigin::global));
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
    auto encode_field(schema::BitPlacement const& placement, Expr input) -> Expr {
        if (placement.offset != 0 || placement.bits != layout_.storage_bits) {
            return pack(field_name(placement) + "_layout", input);
        }

        auto const already_storage{aggregate(placement) || !addressable(placement) ||
                                   value_type(context_, placement.type, placement.bits) ==
                                       std_types().integer(layout_.storage_bits)};
        return already_storage ? input : cast("storage_type", input);
    }
    auto combine_fields(std::span<Expr const> fields) -> Expr {
        if (fields.empty()) {
            return number(0);
        }

        auto encoded{fields.front()};
        auto const count{fields.size()};
        for (std::size_t index{1}; index < count; ++index) {
            encoded = binary(BinaryOperator::bitwise_or, encoded, fields[index]);
        }

        return count == 1 ? encoded : cast("storage_type", encoded);
    }
    void build_constructor(std::optional<std::uint32_t> choice) {
        std::vector<std::uint32_t> inputs;
        parameters(std::nullopt, choice, inputs);
        if (inputs.empty() && !choice) {
            return;
        }
        std::vector<FunctionParameterId> params;
        std::vector<Expr> fields;
        Nodes body;
        auto const result{named("result")};
        auto const target{choice ? member_access(result, "value_") : stored_value()};
        if (choice) {
            for (auto const& guard : layout_.placements[*choice].guards) {
                fields.push_back(pack(guard.path + "_layout", guard_code(guard)));
            }
        }
        bool validate_record{};
        for (auto const index : inputs) {
            auto const& input{layout_.placements[index]};
            auto const name{field_name(input)};
            auto const argument_name{scalar_ ? "value"
                                             : name + (addressable(input) ? "_value" : "_encoded")};
            auto const argument{named(argument_name)};
            params.push_back(const_parameter(access_type(input), argument_name));
            validate_record = validate_record || input.discriminator;
            Expr encoded{};
            if (!addressable(input)) {
                validate_record = true;
                if (input.bits < layout_.storage_bits) {
                    body.push_back(
                        assertion(binary(BinaryOperator::less_equal,
                                         argument,
                                         member_access(named(name + "_layout"), "value_mask"))));
                }
                encoded = cast("storage_type", argument);
            } else {
                assert_if(body, value_check(input, argument));
                encoded = aggregate(input)
                            ? cast("storage_type", member_call(argument, "raw_value"))
                            : argument;
            }
            fields.push_back(encode_field(input, encoded));
        }

        auto const encoded{combine_fields(fields)};
        if (choice) {
            body.push_back(variable("auto", "result", invoke("from_raw", {encoded})));
        }

        if (validate_record) {
            body.push_back(
                assertion(choice ? member_call(result, "is_valid") : invoke("is_valid")));
        }
        if (record_invalid(type_)) {
            body.push_back(
                assertion(binary(BinaryOperator::not_equal, target, named("invalid_value"))));
        }
        if (choice) {
            body.push_back(return_stmt(result));
            auto const& placement{layout_.placements[*choice]};
            auto path{placement.path};
            auto const& children{graph_.definitions().children};
            if (children.count[placement.child]) {
                path.resize(path.rfind('_'));
            }
            functions_.push_back(
                emit_function("make_" + path, name_, FunctionReceiver::static_, params, body));
        } else {
            auto const ctor{constructor(text(name_), true)};
            function_flags(ctor).set_is_constexpr(true);
            function_flags(ctor).set_define_in_header(true);
            set_function_noexcept(ctor);
            set_function_parameters(ctor, params);
            set_function_body(ctor, body, {member_initializer(text("Base"), encoded)});
            constructors_.push_back(header_function(ctor));
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
        body.push_back(return_stmt(
            aggregate(placement)
                ? call(type_reference(decorate_type(decoded_type, "", "::from_raw")),
                       {cast(decorate_type(decoded_type, "", "::storage_type"), decoded)})
                : decoded));
        functions_.push_back(emit_function(name, decoded_type, FunctionReceiver::const_, {}, body));
        if (placement.discriminator) {
            return;
        }
        body = std::move(guards);
        assert_if(body, value_check(placement, value()));
        auto const encoded{aggregate(placement)
                               ? cast("storage_type", member_call(value(), "raw_value"))
                               : value()};
        auto const& children{graph_.definitions().children};
        if (!scalar_ && children.tag[placement.child]) {
            body.push_back(variable(
                "auto const", "updated", insert(name + "_layout", stored_value(), encoded)));
            assert_if(body, valid_children(index, placement.type, named("updated")));
            body.push_back(assignment(stored_value(), named("updated")));
        } else {
            body.push_back(insert_member(name + "_layout", encoded));
        }
        functions_.push_back(emit_function("set_" + name,
                                           std_types().void_,
                                           FunctionReceiver::unqualified,
                                           {const_parameter(decoded_type, "value")},
                                           body));
        if (graph_.definition(placement.type).kind() == schema::TypeDefinitionKind::fixed_point) {
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
        auto const raw_name{field_name(placement)};
        auto const layout_name{raw_name + "_layout"};
        auto const decoded_name{scalar_ ? "decoded_value" : path + "_value"};
        auto const encode_name{scalar_ ? "try_encode_value" : "try_encode_" + path + "_value"};
        auto& metadata{field_metadata_[index]};
        auto const magnitude{member_access(named(layout_name), "magnitude_bits")};
        auto const lower{range_bound(layout_name, "min")};
        auto const upper{range_bound(layout_name, "max")};
        auto const ldexp = [&](Expr mantissa, Expr exponent) {
            return call(type_reference(std_types().ldexp), {mantissa, exponent});
        };
        auto const not_finite = [&](Expr value) {
            return unary(UnaryOperator::logical_not,
                         call(type_reference(std_types().isfinite), {value}));
        };
        auto const fail_if = [&](Expr condition) {
            return if_stmt(condition, block({return_stmt(literal("false"))}));
        };
        Expr minimum;
        Expr maximum;
        Expr decode;
        Expr scale;
        auto const range_form{fixed.form[row] == FixedPointForm::range};
        if (range_form) {
            auto const range{fixed.value_range[row]};
            auto const span{range.max - range.min};
            auto const intervals{static_cast<double>(mask(placement.bits))};
            auto const step{std::isfinite(span) ? span / intervals
                                                : range.max / intervals - range.min / intervals};
            metadata.push_back(line_comment(std::format("Range: {}; step: {}.", range, step)));
            auto const real = [&](double value) {
                auto spelling{std::format("{}", value)};
                if (spelling.find_first_of(".eE") == std::string::npos) {
                    spelling += ".0";
                }
                return literal(spelling);
            };
            minimum = constant(metadata, path + "_minimum", std_types().f64, real(range.min));
            maximum = constant(metadata, path + "_maximum", std_types().f64, real(range.max));
            auto const codes{cast(std_types().f64, upper)};
            decode = call(
                type_reference(std_types().lerp),
                {minimum,
                 maximum,
                 binary(BinaryOperator::divide, cast(std_types().f64, invoke(raw_name)), codes)});

            // Halve opposite extreme endpoints before subtracting to avoid an overflowing span.
            auto const half_span{!std::isfinite(span)};
            auto const adjusted = [&](Expr input) {
                return half_span ? binary(BinaryOperator::multiply, input, literal("0.5")) : input;
            };
            scale = binary(
                BinaryOperator::multiply,
                binary(BinaryOperator::divide,
                       binary(BinaryOperator::subtract, adjusted(value()), adjusted(minimum)),
                       binary(BinaryOperator::subtract, adjusted(maximum), adjusted(minimum))),
                codes);
        } else {
            auto const fractional_bits{fixed.fractional_bits[row]};
            auto const fractional{constant(
                metadata, path + "_fractional_bits", std_types().int_, number(fractional_bits))};
            auto const negative_fractional{unary(UnaryOperator::negate, fractional)};
            minimum = ldexp(cast(std_types().f64, lower), negative_fractional);
            maximum = ldexp(cast(std_types().f64, upper), negative_fractional);
            decode = ldexp(cast(std_types().f64, invoke(raw_name)), negative_fractional);
            scale = ldexp(value(), fractional);

            auto const magnitude_bits{placement.bits - (signedness ? 1U : 0U)};
            auto const min_raw{signedness ? "-" + std::to_string(std::uint64_t{1} << magnitude_bits)
                                          : "0"};
            auto const max_raw{std::to_string(mask(magnitude_bits))};
            auto const divisor{fractional_bits == 64
                                   ? "18446744073709551616"
                                   : std::to_string(std::uint64_t{1} << fractional_bits)};
            auto const range{fractional_bits == 0
                                 ? ioj::MinMax{min_raw, max_raw}
                                 : ioj::MinMax{std::format("{} / {}", min_raw, divisor),
                                               std::format("{} / {}", max_raw, divisor)}};
            metadata.push_back(
                line_comment(std::format("Range: {}; step: {}.",
                                         range,
                                         std::ldexp(1.0, -static_cast<int>(fractional_bits)))));
        }
        functions_.push_back(emit_function(decoded_name,
                                           std_types().f64,
                                           FunctionReceiver::const_,
                                           {},
                                           {return_stmt(decode)},
                                           false));
        std::vector<Predicate> invalid_inputs{not_finite(value()),
                                              binary(BinaryOperator::less, value(), minimum),
                                              binary(BinaryOperator::greater, value(), maximum)};
        Nodes body{fail_if(boolean(any(invalid_inputs))),
                   variable("auto const", "scaled", scale),
                   fail_if(not_finite(named("scaled")))};
        auto const scaled{named("scaled")};
        auto const rounded{named("rounded")};
        if (fixed.rounding[row] == FixedPointRounding::toward_zero) {
            body.push_back(variable(
                "auto const", "rounded", call(type_reference(std_types().trunc), {scaled})));
        } else {
            body.push_back(
                variable("auto", "rounded", call(type_reference(std_types().floor), {scaled})));
            body.push_back(variable(
                "auto const", "fraction", binary(BinaryOperator::subtract, scaled, rounded)));
            auto const fraction{named("fraction")};
            auto const tie{
                binary(BinaryOperator::logical_and,
                       binary(BinaryOperator::equal, fraction, literal("0.5")),
                       binary(BinaryOperator::not_equal,
                              call(type_reference(std_types().fmod), {rounded, literal("2.0")}),
                              literal("0.0")))};
            auto const round_up{binary(BinaryOperator::logical_or,
                                       binary(BinaryOperator::greater, fraction, literal("0.5")),
                                       tie)};
            body.push_back(if_stmt(
                round_up,
                block(
                    {assignment(rounded, binary(BinaryOperator::add, rounded, literal("1.0")))})));
        }
        auto const range{ldexp(literal("1.0"), magnitude)};
        if (range_form) {
            // The largest 64-bit code rounds to 2^64 as a double; handle it before casting.
            body.push_back(if_stmt(
                binary(BinaryOperator::greater_equal, rounded, cast(std_types().f64, upper)),
                block({assignment(named("out_raw"), upper), return_stmt(literal("true"))})));
        }
        body.push_back(fail_if(
            binary(BinaryOperator::logical_or,
                   binary(BinaryOperator::greater_equal, rounded, range),
                   binary(BinaryOperator::less,
                          rounded,
                          signedness ? unary(UnaryOperator::negate, range) : literal("0.0")))));
        auto const raw_type{descriptor_value(layout_name)};
        body.push_back(
            variable(decorate_type(raw_type, "", " const"), "raw", cast(raw_type, rounded)));
        if (auto const valid{value_check(placement, named("raw"))}) {
            body.push_back(fail_if(unary(UnaryOperator::logical_not, *valid)));
        }
        body.push_back(assignment(named("out_raw"), named("raw")));
        body.push_back(return_stmt(literal("true")));
        functions_.push_back(emit_function(encode_name,
                                           std_types().bool_,
                                           FunctionReceiver::static_,
                                           {const_parameter(std_types().f64, "value"),
                                            parameter(decorate_type(raw_type, "", "&"), "out_raw")},
                                           body,
                                           false));
        functions_.push_back(emit_function(
            "set_" + decoded_name,
            std_types().void_,
            FunctionReceiver::unqualified,
            {const_parameter(std_types().f64, "value")},
            {variable(raw_type, "raw", init_list({})),
             variable(decorate_type(intern_type("auto"), "[[maybe_unused]] ", " const"),
                      "success",
                      invoke(encode_name, {value(), named("raw")})),
             assertion(named("success")),
             expr_stmt(invoke("set_" + raw_name, {named("raw")}))},
            false));
    }

    LoweringContext& context_;
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
    std::map<std::string, CppTypeId> field_value_types_;
};

} // namespace

auto lower_packed_value(LoweringContext& ctx, DeclarationHandle, schema::TypeId type)
    -> DeclarationEmission {
    return {.header = PackedEmitter{ctx, type}.emit()};
}

} // namespace lispb::codegen::detail
