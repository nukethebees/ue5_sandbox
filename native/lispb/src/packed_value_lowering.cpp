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

auto extract(std::string const& value,
             std::uint32_t offset,
             std::uint32_t bits,
             std::string_view raw = "value_") -> std::string {
    return std::format("ioj::packed_extract<{}>({})", field_type(value, offset, bits), raw);
}

auto scalar_validity(LoweringContext& context,
                     schema::TypeId type,
                     std::uint32_t bits,
                     std::string const& expression) -> std::string {
    auto const& graph{context.semantic};
    auto const definition{graph.definition(type)};
    auto const& definitions{graph.definitions()};
    auto const decoded{value_type(context, type, bits)};
    if (auto const domain{schema::integer_domain(graph, type)}) {
        auto const& scalars{definitions.integer_scalars};
        auto const row{scalars.index(*domain)};
        auto const signedness{scalars.signedness[row] != 0};
        auto const representable_minimum{PackedIntegerValue::from_parts(
            signedness, signedness ? std::uint64_t{1} << (bits - 1) : 0)};
        auto const representable_maximum{PackedIntegerValue{mask(bits - (signedness ? 1 : 0))}};
        std::vector<std::string> bounds;
        if (packed_integer_less(representable_minimum, scalars.minimum_value[row])) {
            bounds.push_back(std::format("{} >= static_cast<{}>({})",
                                         expression,
                                         decoded,
                                         literal(scalars.minimum_value[row])));
        }
        if (packed_integer_less(scalars.maximum_value[row], representable_maximum)) {
            bounds.push_back(std::format("{} <= static_cast<{}>({})",
                                         expression,
                                         decoded,
                                         literal(scalars.maximum_value[row])));
        }
        auto range{combine(bounds, " && ", "true")};
        if (range == "true") {
            return range;
        }
        auto const& codes{context.declarations.packed_named_codes};
        for (auto const id : context.declarations.items(scalars.named_codes[row])) {
            if (codes.sentinel[id]) {
                range = std::format("({}) || {} == static_cast<{}>({})",
                                    range,
                                    expression,
                                    decoded,
                                    literal(codes.value[id]));
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
                if (auto const code{domains.code(domain, ordinal)}) {
                    checks.push_back(std::format(
                        "static_cast<std::uint64_t>({}) == {}",
                        expression,
                        literal(PackedIntegerValue::from_parts(code->negative, code->magnitude))));
                } else {
                    checks.push_back(std::format(
                        "{} == {}::{}",
                        expression,
                        decoded,
                        context.text(context.declarations.enumerators.name[values[ordinal]])));
                }
            }
            return combine(checks, " || ", "false");
        }
        case schema::TypeDefinitionKind::fixed_point: {
            auto const& fixed{definitions.fixed_points};
            auto const row{fixed.index(definition)};
            std::vector<std::string> checks;
            if (fixed.minimum_raw_value[row]) {
                checks.push_back(expression + " >= static_cast<" + decoded + ">(" +
                                 literal(*fixed.minimum_raw_value[row]) + ')');
            }
            if (fixed.maximum_raw_value[row]) {
                checks.push_back(expression + " <= static_cast<" + decoded + ">(" +
                                 literal(*fixed.maximum_raw_value[row]) + ')');
            }
            return combine(checks, " && ", "true");
        }
        case schema::TypeDefinitionKind::linear_quantized: {
            auto const& quantized{definitions.linear_quantized};
            auto const row{quantized.index(definition)};
            return std::format(
                "{} <= {}ULL", expression, mask(bits) - quantized.reserved_codes[row]);
        }
        case schema::TypeDefinitionKind::optional_sentinel: {
            auto const& optional{definitions.optional_sentinels};
            auto const row{optional.index(definition)};
            auto const source{
                graph.occurrences().type[graph.occurrences().index(optional.source[row])]};
            auto const decoded_source{extract(value_type(context, source, bits),
                                              0,
                                              bits,
                                              "static_cast<storage_type>(" + expression + ')')};
            return std::format("{} == static_cast<{}>({}) || ({})",
                               expression,
                               decoded,
                               literal(optional.sentinel_value[row]),
                               scalar_validity(context, source, bits, decoded_source));
        }
        case schema::TypeDefinitionKind::optional_presence_bit: {
            auto const& optional{definitions.optional_presence_bits};
            auto const row{optional.index(definition)};
            auto const source{
                graph.occurrences().type[graph.occurrences().index(optional.source[row])]};
            auto const payload_bits{optional.payload_bits[row]};
            auto const decoded_source{extract(value_type(context, source, payload_bits),
                                              0,
                                              payload_bits,
                                              "static_cast<storage_type>(" + expression + ')')};
            return std::format("(static_cast<storage_type>({}) & {}ULL) == 0 || ({})",
                               expression,
                               std::uint64_t{1} << payload_bits,
                               scalar_validity(context, source, payload_bits, decoded_source));
        }
        default:
            return "true";
    }
}

auto guard_expression(schema::BitGuard const& guard, std::string_view raw = "value_")
    -> std::string {
    return std::format(
        "{} == {}ULL", extract("storage_type", guard.offset, guard.bits, raw), guard.value);
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
        auto const count{layout_.placements.size()};
        for (std::size_t index{}; index < count; ++index) {
            auto const& placement{layout_.placements[index]};
            if (!addressable(placement)) {
                continue;
            }
            auto const accessor{accessor_name(context_, placement)};
            auto const decoded{value_type(context_, placement.type, placement.bits)};
            if (aggregate(placement) || graph.definition(placement.type).kind() ==
                                            schema::TypeDefinitionKind::enumeration) {
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
        write("\n  private:\n    struct RawTag {{}};\n"
              "    constexpr {}(RawTag, storage_type raw) noexcept : value_{{raw}} {{}}\n"
              "    storage_type value_{{{}ULL}};\n}};\n"
              "static_assert(ioj::valid_packed_value_layout<{}>());\n",
              name,
              layout_.default_raw_value.value_or(0),
              name);
        return context_.ir.raw(output_, std::move(dependencies_));
    }
  private:
    auto aggregate(schema::BitPlacement const& placement) const -> bool {
        return composite(context_.semantic.definition(placement.type).kind());
    }

    auto addressable(schema::BitPlacement const& placement) const -> bool {
        return !placement.reserved && placement.bits != 0 &&
               (!aggregate(placement) ||
                schema::is_packed_projection(context_.semantic, placement.type));
    }

    auto value_check(schema::BitPlacement const& placement,
                     std::string const& value,
                     bool check_representation = true) -> std::string {
        if (aggregate(placement)) {
            return value + ".is_valid()";
        }
        auto const check{scalar_validity(context_, placement.type, placement.bits, value)};
        auto const decoded{value_type(context_, placement.type, placement.bits)};
        auto const full_width{placement.bits == 8 || placement.bits == 16 || placement.bits == 32 ||
                              placement.bits == 64};
        if (!check_representation || decoded == "bool" || full_width) {
            return check;
        }
        auto const field{field_type(decoded, 0, placement.bits)};
        auto const representation{std::format(
            "ioj::packed_extract<{}>(ioj::packed_pack<{}>({})) == {}", field, field, value, value)};
        return check == "true" ? representation : '(' + check + ") && " + representation;
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
            auto check{
                aggregate(placement)
                    ? valid_children(static_cast<std::uint32_t>(index), placement.type, raw)
                    : value_check(placement,
                                  extract(value_type(context_, placement.type, placement.bits),
                                          placement.offset,
                                          placement.bits,
                                          raw),
                                  false)};
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
                result = std::format("{} != {}ULL && ({})",
                                     extract("storage_type", placement.offset, placement.bits, raw),
                                     *invalid,
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
                write("{} const {}_encoded", integer_type(input.bits), accessor);
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
                write("        {0} = ioj::packed_insert<{1}>({0}, "
                      "static_cast<storage_type>({2}ULL));\n",
                      target,
                      field_type("storage_type", guard.offset, guard.bits),
                      guard.value);
            }
        }
        bool validate_record{};
        for (auto const input_index : inputs) {
            auto const& input{layout_.placements[input_index]};
            auto const accessor{accessor_name(context_, input)};
            validate_record = validate_record || input.discriminator;
            if (!addressable(input)) {
                validate_record = true;
                if (input.bits != 8 && input.bits != 16 && input.bits != 32 && input.bits != 64) {
                    write("        assert({}_encoded <= {}ULL);\n", accessor, mask(input.bits));
                }
                write("        {0} = ioj::packed_insert<{1}>({0}, "
                      "static_cast<storage_type>({2}_encoded));\n",
                      target,
                      field_type("storage_type", input.offset, input.bits),
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
              fractional);
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
              placement.bits - (signedness ? 1 : 0),
              signedness ? std::format("-std::ldexp(1.0, {})", placement.bits - 1) : "0.0",
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
    std::string output_;
};

} // namespace

auto lower_packed_value(LoweringContext& context, DeclarationHandle, schema::TypeId type)
    -> DeclarationEmission {
    return {.header = {PackedEmitter{context, type}.emit()}};
}

auto lower_scalar(LoweringContext& context, DeclarationHandle declaration, schema::TypeId type)
    -> DeclarationEmission {
    auto const& graph{context.semantic};
    auto const& layout{graph.bit_layout(type)};
    auto const bits{*layout.encoded_bits};
    auto const name{context.text(graph.name(type))};
    auto const decoded{value_type(context, type, bits)};
    auto const validity{scalar_validity(context, type, bits, "value()")};
    auto input_validity{scalar_validity(context, type, bits, "value")};
    if (bits != 8 && bits != 16 && bits != 32 && bits != 64) {
        auto const representation{"ioj::packed_extract<value_field>("
                                  "ioj::packed_pack<value_field>(value)) == value"};
        input_validity = input_validity == "true" ? representation
                                                  : '(' + input_validity + ") && " + representation;
    }
    auto const assertion{input_validity == "true" ? ""
                                                  : "        assert((" + input_validity + "));\n"};
    std::string output;
    ioj::FormatWriter write{std::back_inserter(output)};
    write("struct {0} {{\n  public:\n    using storage_type = {1};\n    using value_type = {2};\n"
          "    using value_field = ioj::PackedField<storage_type, value_type, 0, {3}>;\n",
          name,
          integer_type(layout.storage_bits),
          decoded,
          bits);
    if (declaration.kind() == DeclarationKind::integer_scalar) {
        auto const& scalars{context.declarations.integer_scalars};
        auto const& codes{context.declarations.packed_named_codes};
        for (auto const code : context.declarations.items(scalars.named_codes[declaration])) {
            write("    static constexpr value_type {}{{static_cast<value_type>({})}};\n",
                  context.text(codes.name[code]),
                  literal(codes.value[code]));
        }
    }
    write(
        "\n    constexpr {0}() noexcept = default;\n"
        "    explicit constexpr {0}(value_type const value) noexcept\n"
        "        : value_{{ioj::packed_pack<value_field>(value)}} {{\n"
        "{3}    }}\n"
        "    [[nodiscard]] static constexpr auto from_raw(storage_type raw) noexcept -> {0} {{\n"
        "        {0} result;\n        result.value_ = raw;\n        return result;\n    }}\n"
        "    [[nodiscard]] constexpr auto raw_value() const noexcept -> storage_type {{ return "
        "value_; }}\n"
        "    [[nodiscard]] constexpr auto value() const noexcept -> value_type {{\n"
        "        return ioj::packed_extract<value_field>(value_);\n    }}\n"
        "    constexpr void set_value(value_type const value) noexcept {{\n"
        "{3}        value_ = ioj::packed_insert<value_field>(value_, value);\n    }}\n"
        "    [[nodiscard]] static constexpr auto is_valid_value(value_type const {4}) noexcept -> "
        "bool {{\n"
        "        return {1};\n    }}\n"
        "    [[nodiscard]] constexpr auto is_valid() const noexcept -> bool {{ return {2}; }}\n"
        "    [[nodiscard]] constexpr auto operator<=>( {0} const& other) const noexcept {{\n"
        "        return value() <=> other.value();\n    }}\n"
        "    [[nodiscard]] constexpr auto operator==( {0} const& other) const noexcept -> bool {{\n"
        "        return value() == other.value();\n    }}\n"
        "  private:\n    storage_type value_{{}};\n}};\n"
        "static_assert(ioj::valid_packed_value_layout<{0}>());\n",
        name,
        input_validity,
        validity,
        assertion,
        input_validity == "true" ? "" : "value");
    return {.header = {context.ir.raw(output, base_dependencies(context))}};
}

} // namespace lispb::codegen::detail
