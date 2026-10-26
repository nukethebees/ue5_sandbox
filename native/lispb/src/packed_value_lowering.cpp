#include "declaration_emission.h"
#include "lowering.h"
#include "lowering_context.h"
#include "packed_value_internal.h"
#include <ioj/format_writer.h>

#include <codegen/ast/cpp_ir.h>
#include <codegen/ast/cpp_types.h>
#include <codegen/schema/child_occurrences.h>
#include <codegen/schema/declarations.h>
#include <codegen/schema/enumerator_schemas.h>
#include <codegen/schema/packed_named_code_schemas.h>
#include <lispb/schema/bit_layout.h>
#include <lispb/schema/integer_domain.h>
#include <lispb/schema/type_graph.h>

#include <algorithm>
#include <cstdint>
#include <format>
#include <iterator>
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

auto combine(std::vector<std::string> const& checks,
             std::string_view operation,
             std::string_view empty) -> std::string {
    std::string result;
    for (auto const& check : checks) {
        if ((operation == " && " && check == "true") || (operation == " || " && check == "false")) {
            continue;
        }
        if (!result.empty()) {
            result += operation;
        }
        result += '(' + check + ')';
    }
    return result.empty() ? std::string{empty} : result;
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

auto field_type(std::string const& value, std::uint32_t offset, std::uint32_t bits) -> std::string {
    return std::format("ioj::PackedField<storage_type, {}, {}, {}>", value, offset, bits);
}

using NamedDeclarations = std::vector<std::pair<std::string, std::string>>;

auto named_declaration(NamedDeclarations& declarations, std::string name, std::string declaration)
    -> std::string {
    if (std::ranges::none_of(declarations,
                             [&](auto const& entry) { return entry.first == name; })) {
        declarations.emplace_back(name, std::move(declaration));
    }
    return name;
}

auto named_constant(NamedDeclarations& declarations,
                    std::string name,
                    std::string const& type,
                    std::string const& expression) -> std::string {
    return named_declaration(
        declarations,
        name,
        std::format("    static constexpr {} {}{{{}}};\n", type, name, expression));
}

auto consecutive(PackedIntegerValue left, PackedIntegerValue right) -> bool {
    if (left.negative) {
        return left.magnitude == 1 ? right.magnitude == 0
                                   : right.negative && right.magnitude == left.magnitude - 1;
    }
    return !right.negative && left.magnitude != (std::numeric_limits<std::uint64_t>::max)() &&
           right.magnitude == left.magnitude + 1;
}

auto scalar_validity(LoweringContext& context,
                     schema::TypeId type,
                     std::uint32_t bits,
                     std::string const& expression,
                     std::string const& prefix,
                     std::string const& decoded,
                     NamedDeclarations& constants,
                     bool field_range = false) -> std::string {
    auto const& graph{context.semantic};
    auto const definition{graph.definition(type)};
    auto const& definitions{graph.definitions()};
    auto const qualify = [&](std::string_view suffix) {
        return prefix.empty() ? std::string{suffix} : prefix + "_" + std::string{suffix};
    };
    auto const bound = [&](std::string_view suffix, PackedIntegerValue value) {
        return named_constant(constants,
                              qualify(suffix),
                              decoded,
                              "static_cast<" + decoded + ">(" + literal(value) + ')');
    };
    if (auto const domain{schema::integer_domain(graph, type)}) {
        auto const& scalars{definitions.integer_scalars};
        auto const row{scalars.index(*domain)};
        auto const signedness{scalars.signedness[row] != 0};
        auto const representable_minimum{PackedIntegerValue::from_parts(
            signedness, signedness ? std::uint64_t{1} << (bits - 1) : 0)};
        auto const representable_maximum{PackedIntegerValue{mask(bits - (signedness ? 1 : 0))}};
        auto const minimum{field_range ? prefix + "_range.min"
                                       : bound(prefix.empty() ? "minimum_value" : "minimum",
                                               scalars.minimum_value[row])};
        auto const maximum{field_range ? prefix + "_range.max"
                                       : bound(prefix.empty() ? "maximum_value" : "maximum",
                                               scalars.maximum_value[row])};
        auto lower{scalars.minimum_value[row]};
        auto upper{scalars.maximum_value[row]};
        auto const& codes{context.declarations.packed_named_codes};
        auto const named{context.declarations.items(scalars.named_codes[row])};
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
            return "true";
        }
        std::vector<std::string> bounds;
        if (packed_integer_less(representable_minimum, scalars.minimum_value[row])) {
            bounds.push_back(expression + " >= " + minimum);
        }
        if (packed_integer_less(scalars.maximum_value[row], representable_maximum)) {
            bounds.push_back(expression + " <= " + maximum);
        }
        auto range{combine(bounds, " && ", "true")};
        if (range == "true") {
            return range;
        }
        for (auto const id : context.declarations.items(scalars.named_codes[row])) {
            if (codes.sentinel[id]) {
                auto const name{qualify(context.text(codes.name[id]))};
                if (!field_range && !prefix.empty()) {
                    bound(context.text(codes.name[id]), codes.value[id]);
                }
                range = '(' + range + ") || " + expression + " == " + name;
            }
        }
        return range;
    }
    switch (definition.kind()) {
        case schema::TypeDefinitionKind::enumeration: {
            auto const& enums{definitions.enums};
            auto const domain{enums.domain[enums.index(definition)]};
            auto const& domains{graph.enum_domains()};
            auto const values{domains.values(domain)};
            auto const count_value{domains.count_value[domains.index(domain)]};
            std::vector<std::string> checks;
            auto const count{values.size()};
            for (std::size_t ordinal{}; ordinal < count; ++ordinal) {
                if (values[ordinal] == count_value) {
                    continue;
                }
                auto const enumerator{
                    context.text(context.declarations.enumerators.name[values[ordinal]])};
                auto const code{domains.code(domain, ordinal)};
                auto const initializer{code ? "static_cast<" + decoded + ">(" +
                                                  literal(PackedIntegerValue::from_parts(
                                                      code->negative, code->magnitude)) +
                                                  ')'
                                            : decoded + "::" + enumerator};
                auto const name{
                    named_constant(constants, qualify(enumerator + "_code"), decoded, initializer)};
                checks.push_back(expression + " == " + name);
            }
            return combine(checks, " || ", "false");
        }
        case schema::TypeDefinitionKind::fixed_point: {
            auto const& fixed{definitions.fixed_points};
            auto const row{fixed.index(definition)};
            std::vector<std::string> checks;
            if (fixed.minimum_raw_value[row]) {
                auto const minimum{
                    field_range ? prefix.substr(0, prefix.size() - 4) + "_minimum_allowed_raw"
                                : bound("minimum_value", *fixed.minimum_raw_value[row])};
                checks.push_back(expression + " >= " + minimum);
            }
            if (fixed.maximum_raw_value[row]) {
                auto const maximum{
                    field_range ? prefix.substr(0, prefix.size() - 4) + "_maximum_allowed_raw"
                                : bound("maximum_value", *fixed.maximum_raw_value[row])};
                checks.push_back(expression + " <= " + maximum);
            }
            return combine(checks, " && ", "true");
        }
        case schema::TypeDefinitionKind::linear_quantized: {
            auto const& quantized{definitions.linear_quantized};
            auto const row{quantized.index(definition)};
            return expression + " <= " +
                   bound("maximum_encoded",
                         PackedIntegerValue{mask(bits) - quantized.reserved_codes[row]});
        }
        case schema::TypeDefinitionKind::optional_sentinel: {
            auto const& optional{definitions.optional_sentinels};
            auto const row{optional.index(definition)};
            auto const source{
                graph.occurrences().type[graph.occurrences().index(optional.source[row])]};
            auto const payload{qualify("payload")};
            auto const payload_alias{payload + "_type"};
            named_declaration(constants,
                              payload_alias,
                              "    using " + payload_alias + " = " +
                                  value_type(context, source, bits) + ";\n");
            auto const payload_field{payload + "_field"};
            named_declaration(constants,
                              payload_field,
                              "    using " + payload_field + " = " +
                                  field_type(payload_alias, 0, bits) + ";\n");
            auto const decoded_source{"ioj::packed_extract<" + payload_field +
                                      ">(static_cast<storage_type>(" + expression + "))"};
            return expression + " == " + bound("sentinel", optional.sentinel_value[row]) + " || (" +
                   scalar_validity(
                       context, source, bits, decoded_source, payload, payload_alias, constants) +
                   ')';
        }
        case schema::TypeDefinitionKind::optional_presence_bit: {
            auto const& optional{definitions.optional_presence_bits};
            auto const row{optional.index(definition)};
            auto const source{
                graph.occurrences().type[graph.occurrences().index(optional.source[row])]};
            auto const payload_bits{optional.payload_bits[row]};
            auto const payload{qualify("payload")};
            auto const payload_alias{payload + "_type"};
            named_declaration(constants,
                              payload_alias,
                              "    using " + payload_alias + " = " +
                                  value_type(context, source, payload_bits) + ";\n");
            auto const payload_field{payload + "_field"};
            named_declaration(constants,
                              payload_field,
                              "    using " + payload_field + " = " +
                                  field_type(payload_alias, 0, payload_bits) + ";\n");
            auto const decoded_source{"ioj::packed_extract<" + payload_field +
                                      ">(static_cast<storage_type>(" + expression + "))"};
            auto const presence{named_constant(constants,
                                               qualify("presence_mask"),
                                               "storage_type",
                                               "static_cast<storage_type>(storage_type{1} << " +
                                                   payload_field + "::bits)")};
            return "(static_cast<storage_type>(" + expression + ") & " + presence + ") == 0 || (" +
                   scalar_validity(context,
                                   source,
                                   payload_bits,
                                   decoded_source,
                                   payload,
                                   payload_alias,
                                   constants) +
                   ')';
        }
        default:
            return "true";
    }
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

auto base_dependencies(LoweringContext& context) -> std::vector<TypeDependencyId> {
    auto& types{context.ir.types};
    return {types.dependency("ioj::PackedField", "ioj/packed_value.h"),
            types.dependency("std::uint64_t", "cstdint"),
            types.dependency("assert", "cassert"),
            types.dependency("auto", "compare")};
}

auto standard_value_support(LoweringContext& context, schema::TypeId type, bool scalar) -> NodeId {
    auto qualified{context.text(context.semantic.cpp_spelling(type))};
    if (!qualified.starts_with("::")) {
        qualified = "::" + qualified;
    }
    auto const integer{qualified + (scalar ? "::value_type" : "::storage_type")};
    auto const accessor{scalar ? "value" : "raw_value"};
    auto const text{std::format(
        "namespace std {{\n"
        "template <typename Char>\n"
        "struct formatter<{0}, Char> : formatter<{1}, Char> {{\n"
        "    template <typename FormatContext>\n"
        "    auto format({0} const& value, FormatContext& context) const -> typename "
        "FormatContext::iterator {{\n"
        "        return formatter<{1}, Char>::format(value.{2}(), context);\n    }}\n}};\n"
        "template <>\nstruct hash<{0}> {{\n"
        "    [[nodiscard]] auto operator()({0} const& value) const noexcept -> size_t {{\n"
        "        return hash<{1}>{{}}(value.{2}());\n    }}\n}};\n}}\n",
        qualified,
        integer,
        accessor)};
    auto& types{context.ir.types};
    return context.ir.raw(text,
                          {types.dependency("std::formatter", "format"),
                           types.dependency("std::hash", "functional"),
                           types.dependency("std::size_t", "cstddef")});
}

class PackedEmitter {
  public:
    PackedEmitter(LoweringContext& context, schema::TypeId type)
        : context_{context}
        , type_{type}
        , layout_{context.semantic.bit_layout(type)}
        , dependencies_{base_dependencies(context)} {}

    auto emit() -> NodeId {
        auto const& graph{context_.semantic};
        auto const name{context_.text(graph.name(type_))};
        auto const definition{graph.definition(type_)};
        auto const& definitions{graph.definitions()};
        auto const invalid{
            definition.kind() == schema::TypeDefinitionKind::record
                ? definitions.records.invalid_raw_value[definitions.records.index(definition)]
                : std::optional<std::uint64_t>{}};
        ioj::FormatWriter write{std::back_inserter(output_)};
        auto const declaration{graph.source_declaration(type_)};
        auto const export_specifier{
            definition.kind() == schema::TypeDefinitionKind::record
                ? context_.declarations.records.export_specifier[declaration]
                : context_.declarations.unions.export_specifier[declaration]};
        auto const export_text{context_.text(export_specifier)};
        write("struct {}{} {{\n  public:\n    using storage_type = {};\n",
              export_text.empty() ? "" : export_text + ' ',
              name,
              integer_type(layout_.storage_bits));
        write("    static constexpr int encoded_bits{{{}}};\n"
              "    static constexpr int storage_bits{{{}}};\n"
              "    static constexpr storage_type default_raw_value{{{}ULL}};\n",
              *layout_.encoded_bits,
              layout_.storage_bits,
              layout_.default_raw_value.value_or(0));
        auto const count{layout_.placements.size()};
        for (std::size_t index{}; index < count; ++index) {
            auto const& placement{layout_.placements[index]};
            if (placement.reserved || placement.bits == 0) {
                continue;
            }
            auto const accessor{accessor_name(context_, placement)};
            auto const decoded{addressable(placement)
                                   ? value_type(context_, placement.type, placement.bits)
                                   : "storage_type"};
            if (addressable(placement) &&
                (aggregate(placement) || graph.definition(placement.type).kind() ==
                                             schema::TypeDefinitionKind::enumeration)) {
                append_dependencies(context_, placement.child, dependencies_);
            }
            write("    using {}_type = {};\n", accessor, decoded);
            write("    using {}_field = {};\n",
                  accessor,
                  field_type(aggregate(placement) ? "storage_type" : accessor + "_type",
                             placement.offset,
                             placement.bits));
            append_constants(placement, accessor);
        }
        for (auto const& placement : layout_.placements) {
            if (addressable(placement) && !aggregate(placement)) {
                append_validator(placement);
            }
        }
        if (invalid) {
            write("    static constexpr storage_type invalid_value{{{}ULL}};\n", *invalid);
        }
        write("\n    constexpr {}() noexcept = {};\n",
              name,
              layout_.default_raw_value ? "default" : "delete");
        write(
            "    [[nodiscard]] static constexpr auto from_raw(storage_type raw) noexcept -> {} {{\n"
            "        return {}{{RawTag{{}}, raw}};\n    }}\n"
            "    [[nodiscard]] constexpr auto raw_value() const noexcept -> storage_type {{\n"
            "        return value_;\n    }}\n",
            name,
            name);

        auto const has_alternatives{std::ranges::any_of(
            layout_.placements, [](auto const& placement) { return placement.alternative; })};
        if (!has_alternatives) {
            append_constructor(name, std::nullopt);
        }
        std::set<std::pair<std::optional<std::uint32_t>, schema::ChildOccurrenceId>>
            emitted_alternatives;
        for (std::size_t index{}; index < count; ++index) {
            auto const& placement{layout_.placements[index]};
            if (placement.alternative &&
                emitted_alternatives.emplace(placement.parent, placement.child).second) {
                append_constructor(name, static_cast<std::uint32_t>(index));
            }
        }
        auto validity{valid_children(std::nullopt, type_)};
        if (invalid) {
            validity = "value_ != invalid_value && (" + validity + ')';
        }
        write("\n    [[nodiscard]] constexpr auto is_valid() const noexcept -> bool {{\n"
              "        return {};\n    }}\n",
              validity);
        write("    [[nodiscard]] constexpr auto operator<=>({} const&) const noexcept = default;\n",
              name);
        for (auto const& placement : layout_.placements) {
            if (addressable(placement)) {
                append_accessor(placement);
            }
        }
        for (auto const& entry : constants_) {
            output_ += entry.second;
        }
        write("\n  private:\n    struct RawTag {{}};\n"
              "    constexpr {}(RawTag, storage_type raw) noexcept : value_{{raw}} {{}}\n"
              "    storage_type value_{{default_raw_value}};\n}};\n"
              "static_assert(ioj::valid_packed_value_layout<{}>());\n",
              name,
              name);
        return context_.ir.raw(output_, std::move(dependencies_));
    }
  private:
    auto guard_code(schema::BitGuard const& guard) const -> std::string {
        auto const definition{context_.semantic.definition(guard.enumeration)};
        auto const& enums{context_.semantic.definitions().enums};
        auto const domain{enums.domain[enums.index(definition)]};
        auto const& domains{context_.semantic.enum_domains()};
        auto const values{domains.values(domain)};
        for (std::size_t ordinal{}; ordinal < values.size(); ++ordinal) {
            auto const code{domains.code(domain, ordinal)};
            if (code && !code->negative && code->magnitude == guard.value) {
                return guard.path + "_" +
                       context_.text(context_.declarations.enumerators.name[values[ordinal]]) +
                       "_code";
            }
        }
        std::unreachable();
    }

    auto guard_expression(schema::BitGuard const& guard, std::string_view raw = "value_") const
        -> std::string {
        return "ioj::packed_extract<" + guard.path + "_field>(" + std::string{raw} +
               ") == " + guard_code(guard);
    }

    auto aggregate(schema::BitPlacement const& placement) const -> bool {
        return composite(context_.semantic.definition(placement.type).kind());
    }

    auto addressable(schema::BitPlacement const& placement) const -> bool {
        return !placement.reserved && placement.bits != 0 &&
               (!aggregate(placement) ||
                schema::is_packed_projection(context_.semantic, placement.type));
    }

    auto value_check(schema::BitPlacement const& placement, std::string const& value)
        -> std::string {
        if (aggregate(placement)) {
            return value + ".is_valid()";
        }
        return "is_valid_" + accessor_name(context_, placement) + '(' + value + ')';
    }

    void append_validator(schema::BitPlacement const& placement) {
        auto const accessor{accessor_name(context_, placement)};
        auto const alias{accessor + "_type"};
        auto check{scalar_validity(
            context_, placement.type, placement.bits, "value", accessor, alias, constants_, true)};
        auto const decoded{value_type(context_, placement.type, placement.bits)};
        auto const full_width{placement.bits == 8 || placement.bits == 16 || placement.bits == 32 ||
                              placement.bits == 64};
        if (decoded != "bool" && !full_width) {
            auto const field{accessor + "_field"};
            auto const representation{decoded.starts_with("std::int")
                                          ? "ioj::packed_extract<" + field + ">(ioj::packed_pack<" +
                                                field + ">(value)) == value"
                                          : "static_cast<storage_type>(value) <= " + field +
                                                "::value_mask"};
            check = check == "true" ? representation : '(' + check + ") && " + representation;
        }
        ioj::FormatWriter write{std::back_inserter(output_)};
        write(
            "    [[nodiscard]] static constexpr auto is_valid_{}({} const {}) noexcept -> bool {{\n"
            "        return {};\n    }}\n",
            accessor,
            alias,
            check == "true" ? "" : "value",
            check);
    }

    void append_constants(schema::BitPlacement const& placement, std::string const& accessor) {
        auto const domain{schema::integer_domain(context_.semantic, placement.type)};
        if (!domain) {
            return;
        }

        ioj::FormatWriter write{std::back_inserter(output_)};
        auto const& scalars{context_.semantic.definitions().integer_scalars};
        auto const row{scalars.index(*domain)};
        write("    struct {0}_range_type {{ {0}_type min; {0}_type max; }};\n"
              "    static constexpr {0}_range_type {0}_range{{static_cast<{0}_type>({1}), "
              "static_cast<{0}_type>({2})}};\n",
              accessor,
              literal(scalars.minimum_value[row]),
              literal(scalars.maximum_value[row]));
        for (auto const code : context_.declarations.items(scalars.named_codes[row])) {
            auto const& named{context_.declarations.packed_named_codes};
            write("    static constexpr {0}_type {0}_{1}{{static_cast<{0}_type>({2})}};\n",
                  accessor,
                  context_.text(named.name[code]),
                  literal(named.value[code]));
        }
    }

    auto valid_children(std::optional<std::uint32_t> parent,
                        schema::TypeId type,
                        std::string_view raw = "value_") -> std::string {
        std::vector<std::string> checks;
        std::map<schema::ChildOccurrenceId, std::vector<std::string>> alternatives;
        auto const is_union{context_.semantic.definition(type).kind() ==
                            schema::TypeDefinitionKind::union_type};
        auto const count{layout_.placements.size()};
        for (std::size_t index{}; index < count; ++index) {
            auto const& placement{layout_.placements[index]};
            if (placement.parent != parent || placement.reserved) {
                continue;
            }
            auto check{aggregate(placement)
                           ? valid_children(static_cast<std::uint32_t>(index), placement.type, raw)
                           : value_check(placement,
                                         "ioj::packed_extract<" +
                                             accessor_name(context_, placement) + "_field>(" +
                                             std::string{raw} + ')')};
            for (auto const& guard : placement.guards) {
                check = '(' + guard_expression(guard, raw) + ") && (" + check + ')';
            }
            if (is_union) {
                alternatives[placement.child].push_back(std::move(check));
            } else {
                checks.push_back(std::move(check));
            }
        }
        for (auto const& [child, elements] : alternatives) {
            checks.push_back(combine(elements, " && ", "true"));
        }
        auto result{combine(checks, is_union ? " || " : " && ", is_union ? "false" : "true")};
        auto const definition{context_.semantic.definition(type)};
        if (parent && definition.kind() == schema::TypeDefinitionKind::record) {
            auto const& records{context_.semantic.definitions().records};
            if (auto const invalid{records.invalid_raw_value[records.index(definition)]}) {
                auto const& placement{layout_.placements[*parent]};
                auto const constant{named_constant(constants_,
                                                   placement.path + "_invalid_value",
                                                   "storage_type",
                                                   literal(PackedIntegerValue{*invalid}))};
                result = std::format("ioj::packed_extract<{}_field>({}) != {} && ({})",
                                     accessor_name(context_, placement),
                                     raw,
                                     constant,
                                     result);
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
            auto const union_region{context_.semantic.definition(placement.type).kind() ==
                                    schema::TypeDefinitionKind::union_type};
            if ((addressable(placement) && (!contains_choice || choice == index)) ||
                (union_region && !contains_choice)) {
                result.push_back(static_cast<std::uint32_t>(index));
            } else if (addressable(placement) && !aggregate(placement)) {
                result.push_back(static_cast<std::uint32_t>(index));
            } else {
                parameters(static_cast<std::uint32_t>(index), choice, result);
            }
        }
    }

    void append_constructor(std::string const& name, std::optional<std::uint32_t> choice) {
        std::vector<std::uint32_t> inputs;
        parameters(std::nullopt, choice, inputs);
        if (inputs.empty() && !choice) {
            return;
        }
        ioj::FormatWriter write{std::back_inserter(output_)};
        if (choice) {
            auto const& placement{layout_.placements[*choice]};
            auto path{placement.path};
            auto const& children{context_.semantic.definitions().children};
            if (children.count[children.index(placement.child)]) {
                path.resize(path.rfind('_'));
            }
            write("\n    [[nodiscard]] static constexpr auto make_{}(", path);
        } else {
            write("\n    explicit constexpr {}(", name);
        }
        for (std::size_t index{}; index < inputs.size(); ++index) {
            if (index) {
                output_ += ", ";
            }
            auto const& input{layout_.placements[inputs[index]]};
            auto const accessor{accessor_name(context_, input)};
            if (addressable(input)) {
                write("{0}_type const {0}_value", accessor);
            } else {
                write("{0}_type const {0}_encoded", accessor);
            }
        }
        if (choice) {
            write(") noexcept -> {} {{\n        auto result{{from_raw(0)}};\n", name);
        } else {
            output_ += ") noexcept : value_{0} {\n";
        }
        auto const target{choice ? "result.value_" : "value_"};
        if (choice) {
            for (auto const& guard : layout_.placements[*choice].guards) {
                write("        {0} = ioj::packed_insert<{1}_field>({0}, {2});\n",
                      target,
                      guard.path,
                      guard_code(guard));
            }
        }
        bool validate_record{};
        for (auto const input_index : inputs) {
            auto const& input{layout_.placements[input_index]};
            auto const accessor{accessor_name(context_, input)};
            validate_record = validate_record || input.discriminator;
            if (!addressable(input)) {
                validate_record = true;
                if (input.bits < layout_.storage_bits) {
                    write("        assert({0}_encoded <= {0}_field::value_mask);\n", accessor);
                }
                write("        {0} = ioj::packed_insert<{1}>({0}, "
                      "static_cast<storage_type>({2}_encoded));\n",
                      target,
                      accessor + "_field",
                      accessor);
                continue;
            }
            auto const argument{accessor + "_value"};
            if (auto const check{value_check(input, argument)}; check != "true") {
                write("        assert(({}));\n", check);
            }
            write("        {0} = ioj::packed_insert<{1}_field>({0}, {2});\n",
                  target,
                  accessor,
                  aggregate(input) ? "static_cast<storage_type>(" + argument + ".raw_value())"
                                   : argument);
        }
        if (validate_record) {
            output_ +=
                choice ? "        assert(result.is_valid());\n" : "        assert(is_valid());\n";
        }
        auto const definition{context_.semantic.definition(type_)};
        if (definition.kind() == schema::TypeDefinitionKind::record) {
            auto const& records{context_.semantic.definitions().records};
            if (records.invalid_raw_value[records.index(definition)]) {
                write("        assert({} != invalid_value);\n", target);
            }
        }
        output_ += choice ? "        return result;\n    }\n" : "    }\n";
    }

    void append_accessor(schema::BitPlacement const& placement) {
        ioj::FormatWriter write{std::back_inserter(output_)};
        auto const accessor{accessor_name(context_, placement)};
        write("\n    [[nodiscard]] constexpr auto {0}() const noexcept -> {0}_type {{\n", accessor);
        for (auto const& guard : placement.guards) {
            write("        assert(({}));\n", guard_expression(guard));
        }
        if (aggregate(placement)) {
            write("        return {0}_type::from_raw(static_cast<{0}_type::storage_type>("
                  "ioj::packed_extract<{0}_field>(value_)));\n    }}\n",
                  accessor);
        } else {
            write("        return ioj::packed_extract<{}_field>(value_);\n    }}\n", accessor);
        }
        if (placement.discriminator) {
            return;
        }
        write("    constexpr void set_{0}({0}_type const value) noexcept {{\n", accessor);
        for (auto const& guard : placement.guards) {
            write("        assert(({}));\n", guard_expression(guard));
        }
        if (auto const check{value_check(placement, "value")}; check != "true") {
            write("        assert(({}));\n", check);
        }
        auto const& children{context_.semantic.definitions().children};
        if (children.tag[children.index(placement.child)]) {
            auto const index{static_cast<std::uint32_t>(&placement - layout_.placements.data())};
            write("        auto const updated{{ioj::packed_insert<{}_field>(value_, "
                  "static_cast<storage_type>(value.raw_value()))}};\n"
                  "        assert(({}));\n"
                  "        value_ = updated;\n    }}\n",
                  accessor,
                  valid_children(index, placement.type, "updated"));
        } else {
            write("        value_ = ioj::packed_insert<{}_field>(value_, {});\n    }}\n",
                  accessor,
                  aggregate(placement) ? "static_cast<storage_type>(value.raw_value())" : "value");
        }
        auto const definition{context_.semantic.definition(placement.type)};
        if (definition.kind() == schema::TypeDefinitionKind::fixed_point) {
            append_fixed_conversion(placement, definition);
        }
    }

    void append_fixed_conversion(schema::BitPlacement const& placement,
                                 schema::TypeDefinitionId definition) {
        auto const& fixed{context_.semantic.definitions().fixed_points};
        auto const row{fixed.index(definition)};
        auto const fractional{fixed.fractional_bits[row]};
        auto const signedness{fixed.signedness[row] != 0};
        auto const maximum{signedness ? mask(placement.bits - 1) : mask(placement.bits)};
        auto const minimum{
            PackedIntegerValue::from_parts(signedness, signedness ? maximum + 1 : 0)};
        auto const lower{fixed.minimum_raw_value[row].value_or(minimum)};
        auto const upper{fixed.maximum_raw_value[row].value_or(PackedIntegerValue{maximum})};
        auto const& path{placement.path};
        named_constant(constants_, path + "_fractional_bits", "int", std::to_string(fractional));
        named_constant(constants_,
                       path + "_magnitude_bits",
                       "int",
                       std::to_string(placement.bits - (signedness ? 1 : 0)));
        dependencies_.push_back(context_.ir.types.dependency("std::ldexp", "cmath"));
        ioj::FormatWriter write{std::back_inserter(output_)};
        write("    static constexpr {0}_raw_type "
              "{0}_minimum_allowed_raw{{static_cast<{0}_raw_type>({1})}};\n"
              "    static constexpr {0}_raw_type "
              "{0}_maximum_allowed_raw{{static_cast<{0}_raw_type>({2})}};\n"
              "    [[nodiscard]] auto {0}_value() const noexcept -> double {{\n"
              "        return std::ldexp(static_cast<double>({0}_raw()), -{3});\n    }}\n"
              "    [[nodiscard]] static auto try_encode_{0}_value(double value, {0}_raw_type& "
              "out_raw) noexcept -> bool {{\n"
              "        if (!std::isfinite(value) || value < "
              "std::ldexp(static_cast<double>({0}_minimum_allowed_raw), -{3}) ||\n"
              "            value > std::ldexp(static_cast<double>({0}_maximum_allowed_raw), -{3})) "
              "{{ return false; }}\n"
              "        auto const scaled{{std::ldexp(value, {3})}};\n"
              "        if (!std::isfinite(scaled)) {{ return false; }}\n",
              path,
              literal(lower),
              literal(upper),
              path + "_fractional_bits");
        if (fixed.rounding[row] == FixedPointRounding::toward_zero) {
            output_ += "        auto const rounded{std::trunc(scaled)};\n";
        } else {
            output_ += "        auto const lower{std::floor(scaled)};\n"
                       "        auto const fraction{scaled - lower};\n"
                       "        auto const rounded{fraction > 0.5 || (fraction == 0.5 && "
                       "std::fmod(lower, 2.0) != 0.0) ? lower + 1.0 : lower};\n";
        }
        write("        if (rounded >= std::ldexp(1.0, {}) || rounded < {}) {{ return false; }}\n"
              "        auto const raw{{static_cast<{}_raw_type>(rounded)}};\n"
              "        if (raw < {}_minimum_allowed_raw || raw > {}_maximum_allowed_raw) {{ return "
              "false; }}\n"
              "        out_raw = raw;\n        return true;\n    }}\n",
              path + "_magnitude_bits",
              signedness ? std::format("-std::ldexp(1.0, {}_magnitude_bits)", path) : "0.0",
              path,
              path,
              path);
        write("    void set_{0}_value(double value) noexcept {{\n"
              "        {0}_raw_type raw{{}};\n"
              "        [[maybe_unused]] auto const success{{try_encode_{0}_value(value, raw)}};\n"
              "        assert(success);\n        set_{0}_raw(raw);\n    }}\n",
              path);
    }

    LoweringContext& context_;
    schema::TypeId type_;
    schema::BitLayout const& layout_;
    std::vector<TypeDependencyId> dependencies_;
    NamedDeclarations constants_;
    std::string output_;
};

} // namespace

auto lower_packed_value(LoweringContext& context, DeclarationHandle, schema::TypeId type)
    -> DeclarationEmission {
    return {.header = {PackedEmitter{context, type}.emit()},
            .header_global = {standard_value_support(context, type, false)}};
}

auto lower_scalar(LoweringContext& context, DeclarationHandle declaration, schema::TypeId type)
    -> DeclarationEmission {
    auto const& graph{context.semantic};
    auto const& layout{graph.bit_layout(type)};
    auto const bits{*layout.encoded_bits};
    auto const name{context.text(graph.name(type))};
    auto const decoded{value_type(context, type, bits)};
    NamedDeclarations constants;
    auto input_validity{scalar_validity(context, type, bits, "value", "", "value_type", constants)};
    if (bits != 8 && bits != 16 && bits != 32 && bits != 64) {
        auto const representation{
            decoded.starts_with("std::int")
                ? "ioj::packed_extract<value_field>(ioj::packed_pack<value_field>(value)) == value"
                : "static_cast<storage_type>(value) <= value_field::value_mask"};
        input_validity = input_validity == "true" ? representation
                                                  : '(' + input_validity + ") && " + representation;
    }
    std::string output;
    ioj::FormatWriter write{std::back_inserter(output)};
    write("struct {0} {{\n  public:\n    using storage_type = {1};\n    using value_type = {2};\n"
          "    static constexpr int encoded_bits{{{3}}};\n"
          "    static constexpr int storage_bits{{{4}}};\n"
          "    static constexpr storage_type default_raw_value{{}};\n"
          "    using value_field = ioj::PackedField<storage_type, value_type, 0, encoded_bits>;\n",
          name,
          integer_type(layout.storage_bits),
          decoded,
          bits,
          layout.storage_bits);
    if (declaration.kind() == DeclarationKind::integer_scalar) {
        auto const& scalars{context.declarations.integer_scalars};
        auto const& codes{context.declarations.packed_named_codes};
        for (auto const code : context.declarations.items(scalars.named_codes[declaration])) {
            write("    static constexpr value_type {}{{static_cast<value_type>({})}};\n",
                  context.text(codes.name[code]),
                  literal(codes.value[code]));
        }
    }
    for (auto const& entry : constants) {
        output += entry.second;
    }
    write(
        "\n    constexpr {0}() noexcept = default;\n"
        "    explicit constexpr {0}(value_type const value) noexcept\n"
        "        : value_{{ioj::packed_pack<value_field>(value)}} {{\n"
        "        assert(is_valid_value(value));\n    }}\n"
        "    [[nodiscard]] static constexpr auto from_raw(storage_type raw) noexcept -> {0} {{\n"
        "        {0} result;\n        result.value_ = raw;\n        return result;\n    }}\n"
        "    [[nodiscard]] constexpr auto raw_value() const noexcept -> storage_type {{ return "
        "value_; }}\n"
        "    [[nodiscard]] constexpr auto value() const noexcept -> value_type {{\n"
        "        return ioj::packed_extract<value_field>(value_);\n    }}\n"
        "    constexpr void set_value(value_type const value) noexcept {{\n"
        "        assert(is_valid_value(value));\n        value_ = "
        "ioj::packed_insert<value_field>(value_, value);\n    }}\n"
        "    [[nodiscard]] static constexpr auto is_valid_value(value_type const {2}) noexcept -> "
        "bool {{\n"
        "        return {1};\n    }}\n"
        "    [[nodiscard]] constexpr auto is_valid() const noexcept -> bool {{ return "
        "is_valid_value(value()); }}\n",
        name,
        input_validity,
        input_validity == "true" ? "" : "value");
    if (decoded.starts_with("std::uint") && bits == layout.storage_bits) {
        write("    [[nodiscard]] constexpr auto operator<=>({} const&) const noexcept = default;\n",
              name);
    } else {
        write("    [[nodiscard]] constexpr auto operator<=>({0} const& other) const noexcept {{\n"
              "        return value() <=> other.value();\n    }}\n"
              "    [[nodiscard]] constexpr auto operator==({0} const& other) const noexcept -> "
              "bool {{\n"
              "        return value() == other.value();\n    }}\n",
              name);
    }
    write("  private:\n    storage_type value_{{default_raw_value}};\n}};\n"
          "static_assert(ioj::valid_packed_value_layout<{}>());\n",
          name);
    return {.header = {context.ir.raw(output, base_dependencies(context))},
            .header_global = {standard_value_support(context, type, true)}};
}

} // namespace lispb::codegen::detail
