#pragma once

#include <ioj/min_max.h>

#include <codegen/schema/declaration_handle.h>
#include <codegen/schema/indices.h>
#include <codegen/schema/schema_range.h>
#include <lispb/indexed_column.h>
#include <lispb/schema/enum_code.h>
#include <lispb/schema/enum_domain_issue_kind.h>
#include <lispb/schema/enum_domain_issue_severity.h>
#include <lispb/schema/enum_signedness.h>
#include <lispb/schema/indices.h>
#include <lispb/schema/semantic_range.h>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace lispb::codegen {
class Declarations;
struct Manifest;
}

namespace lispb::schema {

struct EnumDomains {
    EnumDomains() = default;
    explicit EnumDomains(codegen::Declarations const& declarations);
    auto append(codegen::DeclarationHandle declaration) -> EnumDomainId;
    auto find(codegen::DeclarationHandle declaration) const -> EnumDomainId;
    auto index(EnumDomainId id) const -> std::uint32_t;
    auto values(EnumDomainId id) const -> std::span<codegen::EnumeratorId const>;
    auto code(EnumDomainId id, std::size_t ordinal) const -> std::optional<EnumCode>;
    // Include sentinel codes, exclude the count enumerator; gaps, unknowns and negatives have no
    // range.
    auto contiguous_value_range(EnumDomainId id) const -> std::optional<ioj::MinMax<std::uint64_t>>;
    auto issue_count(EnumDomainId id) const -> std::uint32_t;
    auto issue_severity(EnumDomainId id, std::size_t ordinal) const -> EnumDomainIssueSeverity;
    auto issue_message(EnumDomainId id, std::size_t ordinal) const -> std::string;

    IndexedColumn<std::uint64_t, OneBasedIndex<EnumDomainId>> live_value_count;
    IndexedColumn<std::uint64_t, OneBasedIndex<EnumDomainId>> reserved_value_count;
    IndexedColumn<std::uint64_t, OneBasedIndex<EnumDomainId>> distinct_code_count;
    IndexedColumn<std::optional<EnumCode>, OneBasedIndex<EnumDomainId>> minimum_value;
    IndexedColumn<std::optional<EnumCode>, OneBasedIndex<EnumDomainId>> maximum_value;
    IndexedColumn<EnumSignedness, OneBasedIndex<EnumDomainId>> signedness;
    IndexedColumn<std::optional<std::uint8_t>, OneBasedIndex<EnumDomainId>> minimum_required_bits;
    IndexedColumn<codegen::EnumeratorId, OneBasedIndex<EnumDomainId>> count_value;
  private:
    codegen::Declarations const* declarations_{};
    std::vector<EnumDomainId> declarations_to_domains_;
    std::vector<codegen::SchemaRange<codegen::EnumeratorId>> values_;
    std::vector<std::uint32_t> code_offsets_;
    std::vector<std::optional<EnumCode>> codes_;
    std::vector<SemanticRange> issue_ranges_;
    std::vector<EnumDomainIssueKind> issue_kinds_;
    std::vector<codegen::EnumeratorId> issue_values_;
};

auto analyze_enum_domains(codegen::Manifest const& manifest) -> EnumDomains;

}
