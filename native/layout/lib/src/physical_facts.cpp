#include "analyzer_internal.hpp"

namespace ioj::layout {

PhysicalFactsResolver::PhysicalFactsResolver(lispb::schema::TypeGraph const& types,
                                             AbiProfile const& abi,
                                             Variant const* variant)
    : types_{types}
    , abi_{abi}
    , variant_{variant} {}

auto PhysicalFactsResolver::analyze_record(lispb::schema::TypeId const type) -> RecordAnalysis {
    std::vector<lispb::schema::TypeId> active;
    auto result{analyze_record(type, active)};
    check_supplied_layout(type, result.size_bytes, result.alignment_bytes, result.diagnostics);
    return result;
}

auto PhysicalFactsResolver::analyze_union(lispb::schema::TypeId const type) -> UnionAnalysis {
    std::vector<lispb::schema::TypeId> active;
    auto result{analyze_union(type, active)};
    check_supplied_layout(type, result.size_bytes, result.alignment_bytes, result.diagnostics);
    return result;
}

auto PhysicalFactsResolver::analyze_tagged_union(lispb::schema::TypeId const type)
    -> TaggedUnionAnalysis {
    std::vector<lispb::schema::TypeId> active;
    auto result{analyze_tagged_union(type, active)};
    check_supplied_layout(type, result.size_bytes, result.alignment_bytes, result.diagnostics);
    return result;
}

void PhysicalFactsResolver::check_supplied_layout(lispb::schema::TypeId const type,
                                                  std::optional<std::uint64_t>& size,
                                                  std::optional<std::uint64_t>& alignment,
                                                  std::vector<Diagnostic>& diagnostics) const {
    auto const& spelling{types_.type(type).cpp_spelling};
    auto const supplied{abi_.find(spelling)};
    if (size.has_value() && alignment.has_value() && supplied.has_value() &&
        (supplied->size_bytes != *size || supplied->alignment_bytes != *alignment)) {
        diagnostics.push_back(
            {DiagnosticSeverity::error,
             spelling +
                 ": supplied complete-object facts conflict with the derived LispB layout (" +
                 supplied->provenance + ")."});
        size.reset();
        alignment.reset();
    }
}

auto PhysicalFactsResolver::resolve(lispb::schema::ResolvedTypeRef const& use)
    -> PhysicalFactsResult {
    PhysicalFactsResult result{};
    std::vector<lispb::schema::TypeId> active;
    result.facts = facts_for(use, active, result.diagnostics, use.cpp_type.spelling);
    return result;
}

auto PhysicalFactsResolver::resolve(lispb::schema::TypeId const type) -> PhysicalFactsResult {
    PhysicalFactsResult result{};
    std::vector<lispb::schema::TypeId> active;
    result.facts = facts_for(type, active, result.diagnostics, types_.type(type).cpp_spelling);
    return result;
}

auto PhysicalFactsResolver::resolve_spelling(std::string const& spelling, std::string const& module)
    -> PhysicalFactsResult {
    codegen::TypeRef const reference{spelling, {}, std::nullopt};
    auto resolved{codegen::resolve_type_use(reference, {})};
    auto const target{types_.find_reference(reference, module)};
    lispb::schema::ResolvedTypeRef use{.type = target.value_or(lispb::schema::TypeId{}),
                                       .cpp_type = std::move(resolved.cpp_type),
                                       .physical = std::move(resolved.physical)};
    use.physical.names_semantic_type = target.has_value();
    return resolve(use);
}

auto PhysicalFactsResolver::lookup(std::string const& spelling,
                                   std::vector<Diagnostic>& diagnostics,
                                   std::string const& context) const -> std::optional<TypeFacts> {
    auto const normalized{codegen::native_spelling(spelling)};
    auto facts{abi_.find(normalized)};
    if (!facts.has_value()) {
        diagnostics.push_back({DiagnosticSeverity::warning,
                               context + " has unknown physical facts for '" + normalized +
                                   "' in profile '" + abi_.name() + "'.",
                               normalized});
    }
    return facts;
}

auto PhysicalFactsResolver::facts_for(lispb::schema::ResolvedTypeRef const& use,
                                      std::vector<lispb::schema::TypeId>& active,
                                      std::vector<Diagnostic>& diagnostics,
                                      std::string const& context) -> std::optional<TypeFacts> {
    auto const& physical{use.physical};
    if (physical.form == codegen::PhysicalTypeForm::unsupported) {
        diagnostics.push_back({DiagnosticSeverity::warning, context + ": " + physical.diagnostic});
        return std::nullopt;
    }
    if (physical.form == codegen::PhysicalTypeForm::lvalue_reference ||
        physical.form == codegen::PhysicalTypeForm::rvalue_reference) {
        diagnostics.push_back(
            {DiagnosticSeverity::warning,
             context + ": reference-member storage is not established by sizeof the referent."});
        return std::nullopt;
    }
    if (physical.form == codegen::PhysicalTypeForm::object_pointer) {
        if (auto exact{abi_.find(use.cpp_type.spelling)}) {
            return exact;
        }
        if (abi_.object_pointer_representation().has_value()) {
            auto facts{abi_.find(*abi_.object_pointer_representation())};
            if (facts.has_value() && !facts->integer_signed.has_value()) {
                if (facts->origin != FactOrigin::manual_assumption) {
                    facts->origin = FactOrigin::target_abi;
                }
                facts->provenance = "Object-pointer ABI policy for '" + abi_.name() + "' using " +
                                  *abi_.object_pointer_representation() + ": " + facts->provenance;
                return facts;
            }
        }
        return lookup(use.cpp_type.spelling, diagnostics, context);
    }
    if (physical.names_semantic_type && use.type.valid()) {
        return facts_for(use.type, active, diagnostics, context);
    }
    return lookup(physical.object_spelling, diagnostics, context);
}

auto PhysicalFactsResolver::facts_for(lispb::schema::TypeId const type,
                                      std::vector<lispb::schema::TypeId>& active,
                                      std::vector<Diagnostic>& diagnostics,
                                      std::string const& context) -> std::optional<TypeFacts> {
    if (std::ranges::find(active, type) != active.end()) {
        diagnostics.push_back(
            {DiagnosticSeverity::error, context + ": recursive physical representation."});
        return std::nullopt;
    }
    if (auto const cached{known_.find(type)}; cached != known_.end()) {
        return cached->second;
    }
    auto const& node{types_.type(type)};
    auto aggregate_facts = [&](auto nested) -> std::optional<TypeFacts> {
        for (auto& diagnostic : nested.diagnostics) {
            diagnostic.message = context + ": " + diagnostic.message;
            diagnostics.push_back(std::move(diagnostic));
        }
        if (!nested.size_bytes.has_value() || !nested.alignment_bytes.has_value()) {
            return std::nullopt;
        }
        TypeFacts facts{.size_bytes = *nested.size_bytes,
                        .alignment_bytes = *nested.alignment_bytes,
                        .integer_signed = std::nullopt,
                        .unsigned_value_bits = std::nullopt,
                        .provenance = "Derived from LispB '" + node.cpp_spelling +
                                      "' using profile '" + abi_.name() + "'.",
                        .origin = FactOrigin::derived};
        check_supplied_layout(type, nested.size_bytes, nested.alignment_bytes, diagnostics);
        if (!nested.size_bytes.has_value()) {
            return std::nullopt;
        }
        known_.emplace(type, facts);
        return facts;
    };
    if (std::holds_alternative<lispb::schema::RecordType>(node.definition)) {
        return aggregate_facts(analyze_record(type, active));
    }
    if (std::holds_alternative<lispb::schema::UnionType>(node.definition)) {
        return aggregate_facts(analyze_union(type, active));
    }
    if (std::holds_alternative<lispb::schema::TaggedUnionType>(node.definition)) {
        return aggregate_facts(analyze_tagged_union(type, active));
    }
    if (auto const* external{std::get_if<lispb::schema::ExternalType>(&node.definition)}) {
        auto const physical{codegen::classify_physical_type_use(external->cpp_type.spelling)};
        return facts_for(lispb::schema::ResolvedTypeRef{.type = type,
                                                        .cpp_type = external->cpp_type,
                                                        .physical = physical},
                         active,
                         diagnostics,
                         context);
    }
    active.push_back(type);
    std::optional<TypeFacts> facts;
    if (auto const* enumeration{std::get_if<lispb::schema::EnumType>(&node.definition)}) {
        if (enumeration->underlying_type.has_value()) {
            facts = facts_for(*enumeration->underlying_type, active, diagnostics, context);
        } else if (auto const backing{derived_enum_backing_type(*enumeration)}) {
            facts = lookup(*backing, diagnostics, context);
        }
    } else if (auto const* packed{std::get_if<lispb::schema::PackedType>(&node.definition)}) {
        auto const overridden{variant_ != nullptr &&
                              variant_->overrides.packed_storage_types.contains(type)};
        if (overridden) {
            codegen::TypeRef const reference{
                variant_->overrides.packed_storage_types.at(type), {}, std::nullopt};
            auto resolved{codegen::resolve_type_use(reference, {})};
            auto const target{types_.find_reference(reference, node.identity.module_name)};
            resolved.physical.names_semantic_type = target.has_value();
            facts = facts_for(
                lispb::schema::ResolvedTypeRef{.type = target.value_or(lispb::schema::TypeId{}),
                                               .cpp_type = std::move(resolved.cpp_type),
                                               .physical = std::move(resolved.physical)},
                active,
                diagnostics,
                context);
        } else {
            facts = facts_for(packed->storage_type, active, diagnostics, context);
        }
    } else if (auto const* scalar{std::get_if<lispb::schema::IntegerScalarType>(&node.definition)};
               scalar != nullptr && scalar->cpp_representation.has_value()) {
        facts = facts_for(*scalar->cpp_representation, active, diagnostics, context);
    } else {
        diagnostics.push_back({DiagnosticSeverity::warning,
                               context + " has no supported complete-object representation."});
    }
    active.pop_back();
    if (facts.has_value()) {
        known_.emplace(type, *facts);
    }
    return facts;
}

auto PhysicalFactsResolver::analyze_record(lispb::schema::TypeId const type,
                                           std::vector<lispb::schema::TypeId>& active)
    -> RecordAnalysis {
    RecordAnalysis result{.type = type,
                          .members = {},
                          .payload_bytes = std::nullopt,
                          .internal_padding_bytes = std::nullopt,
                          .tail_padding_bytes = std::nullopt,
                          .size_bytes = std::nullopt,
                          .alignment_bytes = std::nullopt,
                          .diagnostics = {},
                          .aggregate = {}};
    auto const& node{types_.type(type)};
    auto const* record{std::get_if<lispb::schema::RecordType>(&node.definition)};
    if (record == nullptr) {
        result.diagnostics.push_back(
            {DiagnosticSeverity::error, "Selected semantic type is not a record."});
        return result;
    }
    if (std::ranges::find(active, type) != active.end()) {
        result.diagnostics.push_back(
            {DiagnosticSeverity::error,
             "Illegal by-value record cycle involving '" + node.identity.name + "'."});
        return result;
    }
    active.push_back(type);

    std::optional<std::uint64_t> offset{0};
    std::uint64_t payload{};
    std::uint64_t internal_padding{};
    std::uint64_t record_alignment{1};
    result.members.reserve(record->members.size());
    for (auto const& member : record->members) {
        auto member_result{RecordMemberAnalysis{.name = member.name,
                                                .semantic_type = member.semantic_type.type,
                                                .element_count = member.count.value_or(1),
                                                .element_facts = std::nullopt,
                                                .offset_bytes = std::nullopt,
                                                .extent_bytes = std::nullopt,
                                                .padding_before_bytes = std::nullopt}};
        auto const context{"Record '" + node.identity.name + "' member '" + member.name + "'"};
        member_result.element_facts =
            facts_for(member.semantic_type, active, result.diagnostics, context);
        if (!member_result.element_facts.has_value()) {
            offset.reset();
            result.members.push_back(std::move(member_result));
            continue;
        }
        auto const alignment{member_result.element_facts->alignment_bytes};
        if (member_result.element_facts->size_bytes == 0 || alignment == 0) {
            offset.reset();
            result.diagnostics.push_back(
                {DiagnosticSeverity::error, context + " has zero-sized target facts."});
            result.members.push_back(std::move(member_result));
            continue;
        }
        member_result.extent_bytes =
            checked_multiply(member_result.element_facts->size_bytes, member_result.element_count);
        if (!member_result.extent_bytes.has_value()) {
            offset.reset();
            result.diagnostics.push_back(
                {DiagnosticSeverity::error, context + " fixed-array extent overflows uint64."});
            result.members.push_back(std::move(member_result));
            continue;
        }
        if (!offset.has_value()) {
            result.members.push_back(std::move(member_result));
            continue;
        }
        auto const aligned{align_up(*offset, alignment)};
        if (!aligned.has_value()) {
            offset.reset();
            result.diagnostics.push_back(
                {DiagnosticSeverity::error, context + " aligned offset overflows uint64."});
            result.members.push_back(std::move(member_result));
            continue;
        }
        member_result.offset_bytes = *aligned;
        member_result.padding_before_bytes = *aligned - *offset;
        auto const next_offset{checked_add(*aligned, *member_result.extent_bytes)};
        auto const next_payload{checked_add(payload, *member_result.extent_bytes)};
        auto const next_padding{checked_add(internal_padding, *member_result.padding_before_bytes)};
        if (!next_offset.has_value() || !next_payload.has_value() || !next_padding.has_value()) {
            offset.reset();
            result.diagnostics.push_back(
                {DiagnosticSeverity::error, context + " layout arithmetic overflows uint64."});
        } else {
            offset = *next_offset;
            payload = *next_payload;
            internal_padding = *next_padding;
            record_alignment = std::max(record_alignment, alignment);
        }
        result.members.push_back(std::move(member_result));
    }

    if (offset.has_value()) {
        if (record->members.empty()) {
            result.payload_bytes = 0;
            result.internal_padding_bytes = 0;
            result.tail_padding_bytes = 0;
            result.size_bytes = 1;
            result.alignment_bytes = 1;
        } else {
            auto const size{align_up(*offset, record_alignment)};
            if (!size.has_value()) {
                result.diagnostics.push_back(
                    {DiagnosticSeverity::error, "Record tail alignment overflows uint64."});
            } else {
                result.payload_bytes = payload;
                result.internal_padding_bytes = internal_padding;
                result.tail_padding_bytes = *size - *offset;
                result.size_bytes = *size;
                result.alignment_bytes = record_alignment;
            }
        }
    }
    active.pop_back();
    return result;
}

auto PhysicalFactsResolver::analyze_union(lispb::schema::TypeId const type,
                                          std::vector<lispb::schema::TypeId>& active)
    -> UnionAnalysis {
    UnionAnalysis result{.type = type,
                         .alternatives = {},
                         .largest_alternative_bytes = std::nullopt,
                         .tail_padding_bytes = std::nullopt,
                         .size_bytes = std::nullopt,
                         .alignment_bytes = std::nullopt,
                         .diagnostics = {},
                         .aggregate = {}};
    auto const& node{types_.type(type)};
    auto const* union_type{std::get_if<lispb::schema::UnionType>(&node.definition)};
    if (union_type == nullptr) {
        result.diagnostics.push_back(
            {DiagnosticSeverity::error, "Selected semantic type is not a union."});
        return result;
    }
    if (std::ranges::find(active, type) != active.end()) {
        result.diagnostics.push_back(
            {DiagnosticSeverity::error,
             "Illegal by-value aggregate cycle involving '" + node.identity.name + "'."});
        return result;
    }
    active.push_back(type);

    std::uint64_t largest_extent{};
    std::uint64_t union_alignment{1};
    bool complete{true};
    result.alternatives.reserve(union_type->alternatives.size());
    for (auto const& alternative : union_type->alternatives) {
        auto alternative_result{
            UnionAlternativeAnalysis{.name = alternative.name,
                                     .semantic_type = alternative.semantic_type.type,
                                     .element_count = alternative.count.value_or(1),
                                     .element_facts = std::nullopt,
                                     .extent_bytes = std::nullopt,
                                     .slack_bytes = std::nullopt,
                                     .total_slack_bytes = std::nullopt}};
        auto const context{"Union '" + node.identity.name + "' alternative '" + alternative.name +
                           "'"};
        alternative_result.element_facts =
            facts_for(alternative.semantic_type, active, result.diagnostics, context);
        if (!alternative_result.element_facts.has_value()) {
            complete = false;
            result.alternatives.push_back(std::move(alternative_result));
            continue;
        }
        auto const alignment{alternative_result.element_facts->alignment_bytes};
        if (alternative_result.element_facts->size_bytes == 0 || alignment == 0) {
            complete = false;
            result.diagnostics.push_back(
                {DiagnosticSeverity::error, context + " has zero-sized target facts."});
            result.alternatives.push_back(std::move(alternative_result));
            continue;
        }
        alternative_result.extent_bytes = checked_multiply(
            alternative_result.element_facts->size_bytes, alternative_result.element_count);
        if (!alternative_result.extent_bytes.has_value()) {
            complete = false;
            result.diagnostics.push_back(
                {DiagnosticSeverity::error, context + " fixed-array extent overflows uint64."});
            result.alternatives.push_back(std::move(alternative_result));
            continue;
        }
        largest_extent = std::max(largest_extent, *alternative_result.extent_bytes);
        union_alignment = std::max(union_alignment, alignment);
        result.alternatives.push_back(std::move(alternative_result));
    }

    if (complete) {
        auto const size{align_up(largest_extent, union_alignment)};
        if (!size.has_value()) {
            result.diagnostics.push_back(
                {DiagnosticSeverity::error, "Union tail alignment overflows uint64."});
        } else {
            result.largest_alternative_bytes = largest_extent;
            result.tail_padding_bytes = *size - largest_extent;
            result.size_bytes = *size;
            result.alignment_bytes = union_alignment;
            for (auto& alternative : result.alternatives) {
                alternative.slack_bytes = *size - *alternative.extent_bytes;
            }
        }
    }
    active.pop_back();
    return result;
}

auto PhysicalFactsResolver::analyze_tagged_union(lispb::schema::TypeId const type,
                                                 std::vector<lispb::schema::TypeId>& active)
    -> TaggedUnionAnalysis {
    TaggedUnionAnalysis result{.type = type,
                               .discriminant_type = {},
                               .discriminant_facts = std::nullopt,
                               .alternatives = {},
                               .largest_alternative_bytes = std::nullopt,
                               .payload_size_bytes = std::nullopt,
                               .payload_alignment_bytes = std::nullopt,
                               .payload_offset_bytes = std::nullopt,
                               .internal_padding_bytes = std::nullopt,
                               .tail_padding_bytes = std::nullopt,
                               .size_bytes = std::nullopt,
                               .alignment_bytes = std::nullopt,
                               .mapped_live_tags = {},
                               .unmapped_live_tags = {},
                               .sentinel_tags = {},
                               .count_sentinel_tag = std::nullopt,
                               .diagnostics = {},
                               .aggregate = {}};
    auto const& node{types_.type(type)};
    auto const* tagged{std::get_if<lispb::schema::TaggedUnionType>(&node.definition)};
    if (tagged == nullptr) {
        result.diagnostics.push_back(
            {DiagnosticSeverity::error, "Selected semantic type is not a tagged union."});
        return result;
    }
    result.discriminant_type = tagged->discriminant.type;
    auto const& discriminant_node{types_.type(tagged->discriminant.type)};
    auto const* enumeration{std::get_if<lispb::schema::EnumType>(&discriminant_node.definition)};
    if (enumeration != nullptr) {
        std::set<std::string, std::less<>> mapped;
        for (auto const& alternative : tagged->alternatives) {
            mapped.insert(alternative.tag);
        }
        for (auto const& enumerator : enumeration->enumerators) {
            if (enumerator.count_sentinel) {
                result.count_sentinel_tag = enumerator.name;
            } else if (enumerator.sentinel) {
                result.sentinel_tags.push_back(enumerator.name);
            } else if (mapped.contains(enumerator.name)) {
                result.mapped_live_tags.push_back(enumerator.name);
            } else {
                result.unmapped_live_tags.push_back(enumerator.name);
            }
        }
    }
    if (std::ranges::find(active, type) != active.end()) {
        result.diagnostics.push_back(
            {DiagnosticSeverity::error,
             "Illegal by-value aggregate cycle involving '" + node.identity.name + "'."});
        return result;
    }
    active.push_back(type);

    result.discriminant_facts = facts_for(tagged->discriminant,
                                          active,
                                          result.diagnostics,
                                          "Tagged union '" + node.identity.name + "' discriminant");
    std::uint64_t largest_extent{};
    std::uint64_t payload_alignment{1};
    bool payload_complete{true};
    result.alternatives.reserve(tagged->alternatives.size());
    for (auto const& alternative : tagged->alternatives) {
        auto alternative_result{
            TaggedUnionAlternativeAnalysis{.name = alternative.name,
                                           .tag = alternative.tag,
                                           .semantic_type = alternative.semantic_type.type,
                                           .element_count = alternative.count.value_or(1),
                                           .element_facts = std::nullopt,
                                           .extent_bytes = std::nullopt,
                                           .payload_slack_bytes = std::nullopt,
                                           .total_payload_slack_bytes = std::nullopt}};
        auto const context{"Tagged union '" + node.identity.name + "' alternative '" +
                           alternative.name + "'"};
        alternative_result.element_facts =
            facts_for(alternative.semantic_type, active, result.diagnostics, context);
        if (!alternative_result.element_facts.has_value()) {
            payload_complete = false;
            result.alternatives.push_back(std::move(alternative_result));
            continue;
        }
        auto const alignment{alternative_result.element_facts->alignment_bytes};
        if (alternative_result.element_facts->size_bytes == 0 || alignment == 0) {
            payload_complete = false;
            result.diagnostics.push_back(
                {DiagnosticSeverity::error, context + " has zero-sized target facts."});
            result.alternatives.push_back(std::move(alternative_result));
            continue;
        }
        alternative_result.extent_bytes = checked_multiply(
            alternative_result.element_facts->size_bytes, alternative_result.element_count);
        if (!alternative_result.extent_bytes.has_value()) {
            payload_complete = false;
            result.diagnostics.push_back(
                {DiagnosticSeverity::error, context + " fixed-array extent overflows uint64."});
            result.alternatives.push_back(std::move(alternative_result));
            continue;
        }
        largest_extent = std::max(largest_extent, *alternative_result.extent_bytes);
        payload_alignment = std::max(payload_alignment, alignment);
        result.alternatives.push_back(std::move(alternative_result));
    }

    if (payload_complete) {
        auto const payload_size{align_up(largest_extent, payload_alignment)};
        if (!payload_size.has_value()) {
            result.diagnostics.push_back(
                {DiagnosticSeverity::error, "Tagged union payload alignment overflows uint64."});
        } else {
            result.largest_alternative_bytes = largest_extent;
            result.payload_size_bytes = *payload_size;
            result.payload_alignment_bytes = payload_alignment;
            for (auto& alternative : result.alternatives) {
                alternative.payload_slack_bytes = *payload_size - *alternative.extent_bytes;
            }
        }
    }

    if (result.discriminant_facts.has_value() && result.payload_size_bytes.has_value()) {
        auto const& discriminant{*result.discriminant_facts};
        if (discriminant.size_bytes == 0 || discriminant.alignment_bytes == 0) {
            result.diagnostics.push_back(
                {DiagnosticSeverity::error,
                 "Tagged union discriminant has zero-sized target facts."});
        } else {
            auto const payload_offset{
                align_up(discriminant.size_bytes, *result.payload_alignment_bytes)};
            auto const content_end{payload_offset.has_value()
                                       ? checked_add(*payload_offset, *result.payload_size_bytes)
                                       : std::nullopt};
            auto const alignment{
                std::max(discriminant.alignment_bytes, *result.payload_alignment_bytes)};
            auto const size{content_end.has_value() ? align_up(*content_end, alignment)
                                                    : std::nullopt};
            if (!payload_offset.has_value() || !content_end.has_value() || !size.has_value()) {
                result.diagnostics.push_back(
                    {DiagnosticSeverity::error,
                     "Tagged union object layout arithmetic overflows uint64."});
            } else {
                result.payload_offset_bytes = *payload_offset;
                result.internal_padding_bytes = *payload_offset - discriminant.size_bytes;
                result.tail_padding_bytes = *size - *content_end;
                result.size_bytes = *size;
                result.alignment_bytes = alignment;
            }
        }
    }
    active.pop_back();
    return result;
}

} // namespace ioj::layout
