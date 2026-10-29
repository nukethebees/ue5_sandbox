#include <lispb/schema/bit_layout.h>

#include "packed_value_internal.h"

#include <codegen/schema/child_occurrences.h>
#include <codegen/schema/declaration_schema.h>
#include <codegen/schema/declarations.h>
#include <codegen/schema/packed_integer_value.h>
#include <lispb/schema/integer_domain.h>
#include <lispb/schema/type_graph.h>
#include <lispb/strings.h>

#include <algorithm>
#include <cassert>
#include <limits>
#include <map>
#include <set>
#include <stdexcept>

namespace lispb::schema {
namespace {

auto mask(std::uint32_t const bits) -> std::uint64_t {
    return bits == 64 ? std::numeric_limits<std::uint64_t>::max() : (std::uint64_t{1} << bits) - 1;
}

class BitLayoutResolver {
  public:
    BitLayoutResolver(TypeGraph const& graph, BitLayoutOverrides const& overrides)
        : graph_{graph}
        , defs_{graph.definitions()}
        , children_{defs_.children}
        , overrides_{overrides}
        , states_(graph.types().size())
        , widths_(graph.types().size()) {}

    auto resolve(TypeId const type) -> BitLayout {
        BitLayout result;
        result.encoded_bits = width(type);
        if (!result.encoded_bits) {
            auto const definition{graph_.definition(type)};
            if (is_packed_projection(graph_, type) ||
                (definition.kind() == TypeDefinitionKind::record &&
                 defs_.records.layout[definition] == codegen::LayoutKind::packed) ||
                (definition.kind() == TypeDefinitionKind::union_type &&
                 defs_.unions.layout[definition] == codegen::LayoutKind::packed)) {
                fail("Packed projection has no fixed-width encoding", type);
            }
            return result;
        }
        auto const bits{*result.encoded_bits};
        if (bits <= 64) {
            result.storage_bits = bits <= 8 ? 8 : bits <= 16 ? 16 : bits <= 32 ? 32 : 64;
        }
        auto const storage{storage_type(type)};
        if (is_valid(storage) && !overrides_.storage_bits) {
            auto const target{graph_.occurrences().type[storage]};
            auto const specified{codegen::detail::packed_unsigned_width(
                graph_.strings().view(graph_.cpp_spelling(target)))};
            if (!specified || static_cast<std::uint32_t>(*specified) < bits) {
                fail("C++ storage must be an unsigned fixed-width integer containing the encoding",
                     type);
            }
            result.storage_bits = static_cast<std::uint32_t>(*specified);
        }
        if (overrides_.storage_bits) {
            auto const storage_bits{*overrides_.storage_bits};
            if ((storage_bits != 8 && storage_bits != 16 && storage_bits != 32 &&
                 storage_bits != 64) ||
                storage_bits < bits) {
                fail("Storage override cannot contain the encoding", type);
            }
            result.storage_bits = storage_bits;
        }
        if (is_packed_projection(graph_, type) && (bits == 0 || bits > 64)) {
            fail("Packed C++ owners require an encoding between 1 and 64 bits", type);
        }
        auto const definition{graph_.definition(type)};
        if (bits > 64 && ((definition.kind() == TypeDefinitionKind::record &&
                           defs_.records.layout[definition] == codegen::LayoutKind::native) ||
                          (definition.kind() == TypeDefinitionKind::union_type &&
                           defs_.unions.layout[definition] == codegen::LayoutKind::native))) {
            return result;
        }
        if (is_packed_projection(graph_, type) && definition.kind() == TypeDefinitionKind::record) {
            auto const invalid{defs_.records.invalid_raw_value[definition]};
            if (invalid && *invalid > mask(result.storage_bits)) {
                fail("Invalid value exceeds the owning storage", type);
            }
        }
        flatten(result, type, "", "", 0, std::nullopt, {});
        if (is_packed_projection(graph_, type)) {
            validate_names(result, type);
        }
        if (is_packed_projection(graph_, type)) {
            for (auto const& placement : result.placements) {
                if (placement.reserved ||
                    graph_.definition(placement.type).kind() != TypeDefinitionKind::enumeration) {
                    continue;
                }
                auto const row{defs_.enums.index(graph_.definition(placement.type))};
                auto const domain{defs_.enums.domain[row]};
                auto underlying{defs_.enums.underlying_type[row]};
                auto const source{graph_.source_declaration(placement.type)};
                auto const& projection{graph_.declarations().enums.cpp_projection[source]};
                if (projection && projection->storage) {
                    underlying =
                        graph_.resolved(graph_.module(placement.type), *projection->storage);
                }
                auto const signed_underlying{is_valid(underlying) &&
                                             codegen::detail::packed_signed_width(
                                                 graph_.strings().view(graph_.cpp_spelling(
                                                     graph_.occurrences().type[underlying])))
                                                 .has_value()};
                if (signed_underlying ||
                    graph_.enum_domains().signedness[domain] == EnumSignedness::signed_) {
                    fail("Packed enum members require an unsigned encoding", placement.type);
                }
                if (!graph_.strings().view(graph_.cpp_spelling(placement.type)).empty()) {
                    continue;
                }
                auto const count{graph_.enum_domains().values(domain).size()};
                for (std::size_t ordinal{}; ordinal < count; ++ordinal) {
                    if (!graph_.enum_domains().code(domain, ordinal)) {
                        fail("Logical-only packed enums require numeric codes", placement.type);
                    }
                }
            }
        }
        for (auto const& [path, bits_override] : overrides_.field_bits) {
            if (!applied_overrides_.contains(path)) {
                throw std::invalid_argument{"Unknown packed field override: " + path};
            }
        }
        result.default_raw_value = default_value(type, result);
        return result;
    }
  private:
    [[noreturn]] void fail(std::string const& message, TypeId const type) const {
        throw std::invalid_argument{message + ": " +
                                    std::string{graph_.strings().view(graph_.name(type))}};
    }
    auto children_of(TypeId const type) const -> std::span<ChildOccurrenceId const> {
        auto const definition{graph_.definition(type)};
        if (definition.kind() == TypeDefinitionKind::record) {
            return defs_.records.members_of(definition);
        }
        if (definition.kind() == TypeDefinitionKind::union_type) {
            return defs_.unions.alternatives_of(definition);
        }
        return {};
    }
    auto child_type(std::uint32_t const row) const -> TypeId {
        return graph_.occurrences().type[children_.semantic_type[row]];
    }
    auto storage_type(TypeId const type) const -> ResolvedOccurrenceId {
        auto const definition{graph_.definition(type)};
        if (definition.kind() == TypeDefinitionKind::record) {
            return defs_.records.storage_type[definition];
        }
        if (definition.kind() == TypeDefinitionKind::union_type) {
            return defs_.unions.storage_type[definition];
        }
        if (definition.kind() == TypeDefinitionKind::integer_scalar) {
            return defs_.integer_scalars.cpp_representation[definition];
        }
        if (graph_.origin(type) == TypeOrigin::declaration) {
            auto const source{graph_.source_declaration(type)};
            auto const& decls{graph_.declarations()};
            std::optional<codegen::CppProjection> projection;
            switch (definition.kind()) {
                case TypeDefinitionKind::fixed_point:
                    projection = decls.fixed_points.cpp_projection[source];
                    break;
                case TypeDefinitionKind::linear_quantized:
                    projection = decls.linear_quantizeds.cpp_projection[source];
                    break;
                case TypeDefinitionKind::mini_float:
                    projection = decls.mini_floats.cpp_projection[source];
                    break;
                case TypeDefinitionKind::optional_sentinel:
                    projection = decls.optional_sentinels.cpp_projection[source];
                    break;
                case TypeDefinitionKind::optional_presence_bit:
                    projection = decls.optional_presence_bits.cpp_projection[source];
                    break;
                default:
                    break;
            }
            if (projection && projection->storage) {
                return graph_.resolved(graph_.module(type), *projection->storage);
            }
        }
        return {};
    }
    auto explicit_width(TypeId const type) const -> std::optional<std::uint32_t> {
        auto const definition{graph_.definition(type)};
        if (definition.kind() == TypeDefinitionKind::record) {
            return defs_.records.bits[definition];
        }
        if (definition.kind() == TypeDefinitionKind::union_type) {
            return defs_.unions.bits[definition];
        }
        return std::nullopt;
    }
    auto extent(ChildOccurrenceId const id, std::string const& path = {})
        -> std::optional<std::uint32_t> {
        auto const row{children_.index(id)};
        if (children_.reserved[row]) {
            auto bits{children_.bits[row]};
            if (auto const override{overrides_.field_bits.find(path)};
                override != overrides_.field_bits.end()) {
                bits = override->second;
                applied_overrides_.insert(path);
            }
            if (!bits || !*bits) {
                throw std::invalid_argument{"Reserved child requires a positive bit extent"};
            }
            return bits;
        }
        auto const occurrence{graph_.occurrences().index(children_.semantic_type[row])};
        if (graph_.occurrences().physical_form[occurrence] != codegen::PhysicalTypeForm::value ||
            graph_.occurrences().cv_qualified[occurrence] ||
            !graph_.occurrences().names_semantic_type[occurrence]) {
            return std::nullopt;
        }
        auto const bits{width(child_type(row), path)};
        if (!bits) {
            return std::nullopt;
        }
        auto const count{children_.count[row].value_or(1)};
        if (*bits && count > std::numeric_limits<std::uint32_t>::max() / *bits) {
            return std::nullopt;
        }
        auto const required{std::uint64_t{*bits} * count};
        if (required > std::numeric_limits<std::uint32_t>::max()) {
            return std::nullopt;
        }
        auto specified{children_.bits[row]};
        if (auto const override{overrides_.field_bits.find(path)};
            override != overrides_.field_bits.end()) {
            specified = override->second;
            applied_overrides_.insert(path);
        }
        if (specified && *specified < required) {
            throw std::invalid_argument{"Child extent cannot truncate its encoding: " +
                                        std::string{graph_.strings().view(children_.name[row])}};
        }
        return specified.value_or(static_cast<std::uint32_t>(required));
    }
    auto width(TypeId const type, std::string const& prefix = {}) -> std::optional<std::uint32_t> {
        auto const definition{graph_.definition(type)};
        auto const overridden_composition{!overrides_.field_bits.empty() &&
                                          (definition.kind() == TypeDefinitionKind::record ||
                                           definition.kind() == TypeDefinitionKind::union_type)};
        if (states_[type.value] == 2 && !overridden_composition) {
            return widths_[type.value];
        }
        if (states_[type.value] == 1) {
            fail("Recursive fixed-width encoding", type);
        }
        states_[type.value] = 1;
        std::optional<std::uint32_t> result;
        if (auto const scalar{integer_domain(graph_, type)}) {
            result = defs_.integer_scalars.bit_width[*scalar];
        } else {
            switch (definition.kind()) {
                case TypeDefinitionKind::external: {
                    auto const spelling{graph_.strings().view(graph_.cpp_spelling(type))};
                    auto const unsigned_bits{codegen::detail::packed_unsigned_width(spelling)};
                    auto const signed_bits{codegen::detail::packed_signed_width(spelling)};
                    if (spelling == "bool") {
                        result = 1;
                    } else if (unsigned_bits) {
                        result = static_cast<std::uint32_t>(*unsigned_bits);
                    } else if (signed_bits) {
                        result = static_cast<std::uint32_t>(*signed_bits);
                    }
                    break;
                }
                case TypeDefinitionKind::enumeration: {
                    auto const row{defs_.enums.index(definition)};
                    if (defs_.enums.bit_width[row]) {
                        result = *defs_.enums.bit_width[row];
                    } else {
                        auto const domain{defs_.enums.domain[row]};
                        auto const bits{graph_.enum_domains().minimum_required_bits[domain]};
                        if (bits) {
                            result = std::max<std::uint32_t>(1, *bits);
                        }
                    }
                    break;
                }
                case TypeDefinitionKind::fixed_point:
                    result = defs_.fixed_points.total_bits[definition];
                    break;
                case TypeDefinitionKind::linear_quantized:
                    result = defs_.linear_quantized.bit_width[definition];
                    break;
                case TypeDefinitionKind::mini_float: {
                    auto const row{defs_.mini_floats.index(definition)};
                    result = defs_.mini_floats.sign_bits[row] +
                             defs_.mini_floats.exponent_bits[row] +
                             defs_.mini_floats.significand_bits[row];
                    break;
                }
                case TypeDefinitionKind::optional_sentinel:
                    result = defs_.optional_sentinels.bit_width[definition];
                    break;
                case TypeDefinitionKind::optional_presence_bit:
                    result = defs_.optional_presence_bits.encoded_bits[definition];
                    break;
                case TypeDefinitionKind::record:
                case TypeDefinitionKind::union_type: {
                    std::uint64_t total{};
                    bool fixed{true};
                    for (auto const child : children_of(type)) {
                        auto const name{std::string{graph_.strings().view(children_.name[child])}};
                        auto const path{prefix.empty() ? name : prefix + "." + name};
                        auto const bits{extent(child, path)};
                        if (!bits) {
                            fixed = false;
                            break;
                        }
                        total = definition.kind() == TypeDefinitionKind::union_type
                                  ? std::max(total, std::uint64_t{*bits})
                                  : total + *bits;
                        if (total > std::numeric_limits<std::uint32_t>::max()) {
                            fixed = false;
                            break;
                        }
                    }
                    if (fixed) {
                        auto const specified{explicit_width(type)};
                        if (specified && *specified < total) {
                            fail("Declared width cannot truncate child encodings", type);
                        }
                        result = specified.value_or(static_cast<std::uint32_t>(total));
                    }
                    break;
                }
                default:
                    break;
            }
        }
        states_[type.value] = 2;
        widths_[type.value] = result;
        return result;
    }
    void flatten(BitLayout& result,
                 TypeId const type,
                 std::string const& prefix,
                 std::string const& source_prefix,
                 std::uint32_t const base,
                 std::optional<std::uint32_t> const parent,
                 std::vector<BitGuard> const& guards) {
        auto const definition{graph_.definition(type)};
        auto const is_union{definition.kind() == TypeDefinitionKind::union_type};
        auto const msb{definition.kind() == TypeDefinitionKind::record &&
                       defs_.records.bit_order[definition] ==
                           codegen::PackedBitOrder::most_significant_first};
        std::uint32_t cursor{};
        std::vector<std::uint32_t> direct;
        for (auto const child : children_of(type)) {
            auto const row{children_.index(child)};
            auto const name{
                codegen::soa_namespace_name(graph_.strings().view(children_.name[row]))};
            auto const path{prefix.empty() ? name : prefix + "_" + name};
            auto const source_name{std::string{graph_.strings().view(children_.name[row])}};
            auto const source_path{source_prefix.empty() ? source_name
                                                         : source_prefix + "." + source_name};
            auto const required_extent{extent(child, source_path)};
            assert(required_extent.has_value());
            auto const child_extent{*required_extent};
            auto const relative{is_union ? 0
                                : msb    ? *width(type, source_prefix) - cursor - child_extent
                                         : cursor};
            auto const count{children_.count[row].value_or(1)};
            auto const encoded{children_.reserved[row] ? child_extent
                                                       : *width(child_type(row), source_path)};
            for (std::uint64_t element{}; element < count; ++element) {
                auto const element_path{children_.count[row] ? path + "_" + std::to_string(element)
                                                             : path};
                auto const index{static_cast<std::uint32_t>(result.placements.size())};
                result.placements.push_back(
                    {.child = child,
                     .type = children_.reserved[row] ? TypeId{} : child_type(row),
                     .path = element_path,
                     .source_path = source_path,
                     .parent = parent,
                     .offset = base + relative + static_cast<std::uint32_t>(element * encoded),
                     .bits = encoded,
                     .extent = count == 1 ? child_extent : encoded,
                     .reserved = children_.reserved[row] != 0,
                     .alternative = is_union,
                     .guards = guards});
                direct.push_back(index);
            }
            if (!is_union) {
                cursor += child_extent;
            }
        }
        for (auto const index : direct) {
            auto const placement{result.placements[index]};
            auto const row{children_.index(placement.child)};
            if (placement.reserved) {
                continue;
            }
            if (!children_.tag[row]) {
                flatten(result,
                        placement.type,
                        placement.path,
                        placement.source_path,
                        placement.offset,
                        index,
                        placement.guards);
                continue;
            }
            if (graph_.definition(placement.type).kind() != TypeDefinitionKind::union_type) {
                fail("Only a union occurrence can bind a discriminator", placement.type);
            }
            auto const found{std::ranges::find_if(direct, [&](auto const candidate) {
                return children_.name[result.placements[candidate].child] == *children_.tag[row];
            })};
            if (found == direct.end()) {
                fail("Union discriminator must name a sibling member", type);
            }
            auto& discriminator{result.placements[*found]};
            if (discriminator.reserved ||
                graph_.definition(discriminator.type).kind() != TypeDefinitionKind::enumeration ||
                children_.count[discriminator.child]) {
                fail("Union discriminator must be a scalar enum member", type);
            }
            discriminator.discriminator = true;
            auto const tag{discriminator};
            auto const union_begin{result.placements.size()};
            flatten(result,
                    placement.type,
                    placement.path,
                    placement.source_path,
                    placement.offset,
                    index,
                    placement.guards);
            auto const enum_def{graph_.definition(tag.type)};
            auto const domain{defs_.enums.domain[enum_def]};
            auto const values{graph_.enum_domains().values(domain)};
            auto const& authored{graph_.declarations()};
            std::map<std::uint64_t, ChildOccurrenceId> mapped;
            for (std::size_t alternative{union_begin}; alternative < result.placements.size();
                 ++alternative) {
                auto& value{result.placements[alternative]};
                if (value.parent != index) {
                    continue;
                }
                auto const child_row{children_.index(value.child)};
                auto const case_name{
                    children_.case_label[child_row].value_or(graph_.name(value.type))};
                auto const enumerator{std::ranges::find_if(values, [&](auto const id) {
                    return authored.enumerators.name[id] == case_name;
                })};
                if (enumerator == values.end()) {
                    fail("Union case has no matching enum enumerator", placement.type);
                }
                auto const ordinal{static_cast<std::size_t>(enumerator - values.begin())};
                auto const code{graph_.enum_domains().code(domain, ordinal)};
                if (!code || code->negative || authored.enumerators.sentinel[*enumerator] ||
                    *enumerator == graph_.enum_domains().count_value[domain]) {
                    fail("Union case requires a nonnegative live numeric discriminator",
                         placement.type);
                }
                auto const [mapping, inserted]{mapped.emplace(code->magnitude, value.child)};
                if (!inserted && mapping->second != value.child) {
                    fail("Union discriminator mapping is not unique", placement.type);
                }
                BitGuard const guard{tag.path, tag.offset, tag.bits, code->magnitude, tag.type};
                for (std::size_t descendant{alternative}; descendant < result.placements.size();
                     ++descendant) {
                    auto current{static_cast<std::uint32_t>(descendant)};
                    while (result.placements[current].parent && current != alternative) {
                        current = *result.placements[current].parent;
                    }
                    if (current == alternative) {
                        result.placements[descendant].guards.push_back(guard);
                    }
                }
            }
        }
    }
    void validate_names(BitLayout const& result, TypeId const type) const {
        std::set<std::string> names{"Base",
                                    "extract",
                                    "insert",
                                    "storage",
                                    "storage_type",
                                    "packed_integer_tag",
                                    "raw_value",
                                    "from_raw",
                                    "from_canonical_raw",
                                    "is_valid",
                                    "value_"};
        auto const& decls{graph_.declarations()};
        auto const root_definition{graph_.definition(type)};
        auto const root_kind{root_definition.kind()};
        if (root_kind == TypeDefinitionKind::record ||
            root_kind == TypeDefinitionKind::union_type) {
            if (root_kind == TypeDefinitionKind::record &&
                defs_.records.invalid_raw_value[root_definition]) {
                names.insert("invalid_value");
            }
        } else {
            names.insert({"value_type",
                          "value_layout",
                          "value",
                          "set_value",
                          "is_valid_value",
                          "is_valid_value_domain"});
            switch (root_kind) {
                case TypeDefinitionKind::optional_sentinel:
                case TypeDefinitionKind::optional_presence_bit: {
                    names.insert("payload_layout");
                    if (root_kind == TypeDefinitionKind::optional_sentinel) {
                        names.insert("sentinel");
                    }
                    auto const source{root_kind == TypeDefinitionKind::optional_sentinel
                                          ? defs_.optional_sentinels.source[root_definition]
                                          : defs_.optional_presence_bits.source[root_definition]};
                    auto const source_type{graph_.occurrences().type[source]};
                    auto const domain{*integer_domain(graph_, source_type)};
                    auto const& scalars{defs_.integer_scalars};
                    auto const row{scalars.index(domain)};
                    for (auto const code : decls.items(scalars.named_codes[row])) {
                        names.insert("payload_" + std::string{graph_.strings().view(
                                                      decls.packed_named_codes.name[code])});
                    }
                    break;
                }
                default:
                    break;
            }
        }
        auto const owner_name{std::string{graph_.strings().view(graph_.name(type))}};
        if (!names.insert(owner_name).second) {
            fail("Owning type name collides with generated C++ member: " + owner_name, type);
        }
        if (root_kind == TypeDefinitionKind::integer_scalar) {
            for (auto const code :
                 decls.items(defs_.integer_scalars.named_codes[root_definition])) {
                auto const name{
                    std::string{graph_.strings().view(decls.packed_named_codes.name[code])}};
                if (!names.insert(name).second) {
                    fail("Projected scalar C++ name collision: " + name, type);
                }
            }
        }
        std::set<std::pair<ChildOccurrenceId, std::optional<std::uint32_t>>> factories;
        for (auto const& value : result.placements) {
            if (value.reserved) {
                continue;
            }
            auto accessor{value.path};
            auto const kind{graph_.definition(value.type).kind()};
            if (kind == TypeDefinitionKind::fixed_point) {
                accessor += "_raw";
            }
            if (kind == TypeDefinitionKind::linear_quantized ||
                kind == TypeDefinitionKind::mini_float ||
                kind == TypeDefinitionKind::optional_sentinel ||
                kind == TypeDefinitionKind::optional_presence_bit) {
                accessor += "_encoded";
            }
            auto const add_name = [&](std::string const& name) {
                if (!names.insert(name).second) {
                    fail("Flattened packed C++ name collision at '" + value.path + "': " + name,
                         type);
                }
            };
            if (kind == TypeDefinitionKind::record &&
                defs_.records
                    .invalid_raw_value[defs_.records.index(graph_.definition(value.type))]) {
                add_name(accessor + "_invalid_value");
            }
            for (auto const& name : {accessor,
                                     "set_" + accessor,
                                     "is_valid_" + accessor,
                                     "is_valid_" + accessor + "_domain",
                                     accessor + "_layout"}) {
                add_name(name);
            }
            if (kind == TypeDefinitionKind::fixed_point) {
                for (auto const& name : {value.path + "_value",
                                         "set_" + value.path + "_value",
                                         "try_encode_" + value.path + "_value",
                                         value.path + "_fractional_bits"}) {
                    add_name(name);
                }
            }
            if (kind == TypeDefinitionKind::optional_sentinel ||
                kind == TypeDefinitionKind::optional_presence_bit) {
                if (kind == TypeDefinitionKind::optional_sentinel) {
                    add_name(accessor + "_sentinel");
                }
                add_name(accessor + "_payload_layout");
                auto const definition{graph_.definition(value.type)};
                auto const source{kind == TypeDefinitionKind::optional_sentinel
                                      ? defs_.optional_sentinels.source[definition]
                                      : defs_.optional_presence_bits.source[definition]};
                auto const source_type{graph_.occurrences().type[source]};
                if (auto const domain{integer_domain(graph_, source_type)}) {
                    for (auto const code :
                         decls.items(defs_.integer_scalars.named_codes[*domain])) {
                        add_name(accessor + "_payload_" +
                                 std::string{
                                     graph_.strings().view(decls.packed_named_codes.name[code])});
                    }
                }
            }
            if (auto const domain{integer_domain(graph_, value.type)}) {
                for (auto const code : decls.items(defs_.integer_scalars.named_codes[*domain])) {
                    auto const name{
                        accessor + "_" +
                        std::string{graph_.strings().view(decls.packed_named_codes.name[code])}};
                    add_name(name);
                }
            }
            if (value.alternative && factories.emplace(value.child, value.parent).second) {
                auto path{value.path};
                if (children_.count[value.child]) {
                    path.resize(path.rfind('_'));
                }
                if (!names.insert("make_" + path).second) {
                    fail("Flattened packed factory name collision: " + path, type);
                }
            }
        }
    }
    auto default_value(TypeId const type, BitLayout const& result) const
        -> std::optional<std::uint64_t> {
        if (!result.encoded_bits || *result.encoded_bits > 64) {
            return std::nullopt;
        }
        struct DefaultBits {
            std::uint64_t raw{};
            bool complete{true};
        };
        std::vector<std::optional<std::uint64_t>> defaults(result.placements.size());
        bool any{};
        auto const count{result.placements.size()};
        for (std::size_t index{}; index < count; ++index) {
            auto const& placement{result.placements[index]};
            if (placement.reserved) {
                continue;
            }
            auto const kind{graph_.definition(placement.type).kind()};
            auto const value{children_.default_value[placement.child]};
            if (!value) {
                continue;
            }
            if (kind == TypeDefinitionKind::record || kind == TypeDefinitionKind::union_type) {
                throw std::invalid_argument{"Packed defaults require a scalar occurrence: " +
                                            placement.path};
            }
            any = true;
            if (placement.bits == 0 || placement.bits > 64) {
                throw std::invalid_argument{"Packed default does not fit its encoding: " +
                                            placement.path};
            }
            validate_default(placement, *value);
            auto const encoded{
                (value->negative ? std::uint64_t{} - value->magnitude : value->magnitude) &
                mask(placement.bits)};
            defaults[index] = encoded << placement.offset;
        }
        if (std::ranges::any_of(result.placements,
                                [](auto const& placement) { return !placement.guards.empty(); })) {
            return std::nullopt;
        }
        auto const definition{graph_.definition(type)};
        auto const invalid{definition.kind() == TypeDefinitionKind::record
                               ? defs_.records.invalid_raw_value[definition]
                               : std::nullopt};
        if (!any) {
            return invalid.value_or(0);
        }

        auto compose = [&](auto&& self,
                           TypeId current_type,
                           std::optional<std::uint32_t> parent) -> DefaultBits {
            auto const current_definition{graph_.definition(current_type)};
            auto const is_union{current_definition.kind() == TypeDefinitionKind::union_type};
            DefaultBits combined;
            std::set<ChildOccurrenceId> visited;
            for (std::size_t index{}; index < count; ++index) {
                auto const& placement{result.placements[index]};
                if (placement.parent != parent || placement.reserved ||
                    !visited.insert(placement.child).second) {
                    continue;
                }
                DefaultBits child;
                for (std::size_t element{index}; element < count; ++element) {
                    auto const& value{result.placements[element]};
                    if (value.parent != parent || value.child != placement.child) {
                        continue;
                    }
                    auto const kind{graph_.definition(value.type).kind()};
                    auto const encoded{
                        kind == TypeDefinitionKind::record || kind == TypeDefinitionKind::union_type
                            ? self(self, value.type, static_cast<std::uint32_t>(element))
                            : DefaultBits{defaults[element].value_or(0),
                                          defaults[element].has_value()}};
                    child.raw |= encoded.raw;
                    child.complete = child.complete && encoded.complete;
                }
                if (is_union) {
                    if (child.complete) {
                        return child;
                    }
                } else {
                    combined.raw |= child.raw;
                    combined.complete = combined.complete && child.complete;
                }
            }
            if (is_union) {
                return {.complete = false};
            }
            if (combined.complete && current_definition.kind() == TypeDefinitionKind::record) {
                auto const current_invalid{defs_.records.invalid_raw_value[current_definition]};
                auto const local_raw{parent ? (combined.raw >> result.placements[*parent].offset) &
                                                  mask(result.placements[*parent].bits)
                                            : combined.raw};
                if (current_invalid == local_raw) {
                    fail("Packed defaults produce the invalid representation", current_type);
                }
            }
            return combined;
        };
        auto const resolved{compose(compose, type, std::nullopt)};
        return resolved.complete ? std::optional{resolved.raw} : std::nullopt;
    }
    void validate_default(BitPlacement const& placement,
                          codegen::PackedIntegerValue const value) const {
        auto const& decls{graph_.declarations()};
        auto const definition{graph_.definition(placement.type)};
        auto const kind{definition.kind()};
        auto const signed_encoding{
            kind == TypeDefinitionKind::fixed_point
                ? defs_.fixed_points.signedness[definition] != 0
                : kind == TypeDefinitionKind::external &&
                      codegen::detail::packed_signed_width(
                          graph_.strings().view(graph_.cpp_spelling(placement.type)))
                          .has_value()};
        if (!integer_domain(graph_, placement.type) &&
            !(signed_encoding ? codegen::packed_integer_fits_signed(value, placement.bits)
                              : codegen::packed_integer_fits_unsigned(value, placement.bits))) {
            throw std::invalid_argument{"Packed default does not fit its scalar encoding: " +
                                        placement.path};
        }
        if (auto const domain{integer_domain(graph_, placement.type)}) {
            auto const& scalars{defs_.integer_scalars};
            auto const row{scalars.index(*domain)};
            auto const signed_domain{scalars.signedness[row] != 0};
            auto const fits{signed_domain
                                ? codegen::packed_integer_fits_signed(value, placement.bits)
                                : codegen::packed_integer_fits_unsigned(value, placement.bits)};
            auto const named{
                std::ranges::any_of(decls.items(scalars.named_codes[row]), [&](auto const code) {
                    return decls.packed_named_codes.value[code] == value;
                })};
            if (!fits ||
                (!named &&
                 (!codegen::packed_integer_less_equal(scalars.minimum_value[row], value) ||
                  !codegen::packed_integer_less_equal(value, scalars.maximum_value[row])))) {
                throw std::invalid_argument{"Packed default is outside its scalar domain: " +
                                            placement.path};
            }
        }
        if (definition.kind() == TypeDefinitionKind::enumeration) {
            auto const domain{defs_.enums.domain[definition]};
            auto const values{graph_.enum_domains().values(domain)};
            bool live{};
            for (std::size_t index{}; index < values.size(); ++index) {
                auto const code{graph_.enum_domains().code(domain, index)};
                live = live || (code && code->negative == value.negative &&
                                code->magnitude == value.magnitude &&
                                !decls.enumerators.sentinel[values[index]] &&
                                values[index] != graph_.enum_domains().count_value[domain]);
            }
            if (!live) {
                throw std::invalid_argument{"Packed default is not a live enum code: " +
                                            placement.path};
            }
        }
        if (definition.kind() == TypeDefinitionKind::fixed_point) {
            auto const row{defs_.fixed_points.index(definition)};
            auto const minimum{defs_.fixed_points.minimum_raw_value[row]};
            auto const maximum{defs_.fixed_points.maximum_raw_value[row]};
            if ((minimum && codegen::packed_integer_less(value, *minimum)) ||
                (maximum && codegen::packed_integer_less(*maximum, value))) {
                throw std::invalid_argument{"Packed default is outside its fixed-point domain: " +
                                            placement.path};
            }
        }
        if (definition.kind() == TypeDefinitionKind::linear_quantized &&
            (value.negative ||
             value.magnitude >
                 mask(placement.bits) - defs_.linear_quantized.reserved_codes[definition])) {
            throw std::invalid_argument{"Packed default names a reserved quantization code: " +
                                        placement.path};
        }
        if (kind == TypeDefinitionKind::optional_sentinel ||
            kind == TypeDefinitionKind::optional_presence_bit) {
            auto const sentinel{kind == TypeDefinitionKind::optional_sentinel};
            auto const source{sentinel ? defs_.optional_sentinels.source[definition]
                                       : defs_.optional_presence_bits.source[definition]};
            auto const payload_bits{
                sentinel ? placement.bits : defs_.optional_presence_bits.payload_bits[definition]};
            auto payload{placement};
            payload.type = graph_.occurrences().type[source];
            payload.bits = payload_bits;
            auto const raw{value.magnitude & mask(payload_bits)};
            if (!sentinel && (value.magnitude & (std::uint64_t{1} << payload_bits)) == 0) {
                return;
            }
            auto const scalar{*integer_domain(graph_, payload.type)};
            auto const negative{defs_.integer_scalars.signedness[scalar] != 0 &&
                                (raw & (std::uint64_t{1} << (payload_bits - 1))) != 0};
            auto const decoded{codegen::PackedIntegerValue::from_parts(
                negative, negative ? (std::uint64_t{} - raw) & mask(payload_bits) : raw)};
            validate_default(payload, decoded);
        }
    }

    TypeGraph const& graph_;
    TypeDefinitions const& defs_;
    ChildOccurrences const& children_;
    BitLayoutOverrides const& overrides_;
    std::vector<std::uint8_t> states_;
    std::vector<std::optional<std::uint32_t>> widths_;
    std::set<std::string> applied_overrides_;
};

}

auto resolve_bit_layout(TypeGraph const& graph,
                        TypeId const type,
                        BitLayoutOverrides const& overrides) -> BitLayout {
    return BitLayoutResolver{graph, overrides}.resolve(type);
}
auto packed_storage_bits(std::string_view const spelling) -> std::optional<std::uint32_t> {
    return codegen::detail::packed_unsigned_width(spelling).transform(
        [](auto const bits) { return static_cast<std::uint32_t>(bits); });
}
auto is_packed_projection(TypeGraph const& graph, TypeId const type) -> bool {
    if (graph.origin(type) != TypeOrigin::declaration ||
        graph.strings().view(graph.cpp_spelling(type)).empty()) {
        return false;
    }
    auto const definition{graph.definition(type)};
    auto const& definitions{graph.definitions()};
    if (definition.kind() == TypeDefinitionKind::record) {
        return definitions.records.layout[definition] == codegen::LayoutKind::packed;
    }
    if (definition.kind() == TypeDefinitionKind::union_type) {
        return definitions.unions.layout[definition] == codegen::LayoutKind::packed;
    }
    return definition.kind() == TypeDefinitionKind::integer_scalar ||
           definition.kind() == TypeDefinitionKind::fixed_point ||
           definition.kind() == TypeDefinitionKind::linear_quantized ||
           definition.kind() == TypeDefinitionKind::mini_float ||
           definition.kind() == TypeDefinitionKind::optional_sentinel ||
           definition.kind() == TypeDefinitionKind::optional_presence_bit ||
           definition.kind() == TypeDefinitionKind::integer_varint;
}

}
