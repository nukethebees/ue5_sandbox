#pragma once

#include <ioj/layout/abi_profile.hpp>
#include <ioj/layout/analyzer/integer_varint_distribution_entry.hpp>
#include <ioj/layout/analyzer/relationship_target_facts.hpp>
#include <ioj/layout/layout_status.hpp>
#include <ioj/layout/planner_analysis_inputs.hpp>
#include <ioj/layout/planner_analysis_results.hpp>
#include <ioj/layout/planner_selection.hpp>
#include <ioj/layout/revision.hpp>
#include <ioj/layout/type_facts.hpp>

#include <lispb/schema/indices.h>
#include <lispb/schema/type_identity.h>

#include <cstdint>
#include <expected>
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace ioj::layout {

class Document;

class PlannerAnalysisSession {
  public:
    using DistributionWeights = std::map<std::string, std::uint64_t>;
    using DistributionTable = std::map<lispb::schema::DeclarationId, DistributionWeights>;

    PlannerAnalysisSession() = default;
    explicit PlannerAnalysisSession(lispb::schema::TypeGraph const& types);
    PlannerAnalysisSession(lispb::schema::TypeGraph const&&) = delete;

    auto inputs() const -> PlannerAnalysisInputs const&;
    // Do not retain a mutable reference across refresh calls.
    auto edit_inputs() -> PlannerAnalysisInputs&;
    auto primary_abi() const -> AbiProfile const&;
    auto comparison_abi() const -> AbiProfile const&;
    auto set_primary_abi(AbiProfile abi) -> bool;
    auto set_external_type_facts(lispb::schema::TypeId type, TypeFacts facts)
        -> std::expected<bool, std::string>;
    auto set_comparison_abi(AbiProfile abi) -> bool;
    auto union_distributions() const -> DistributionTable const&;
    auto tagged_union_distributions() const -> DistributionTable const&;
    auto union_distribution(lispb::schema::DeclarationId declaration) const
        -> DistributionWeights const*;
    auto tagged_union_distribution(lispb::schema::DeclarationId declaration) const
        -> DistributionWeights const*;
    auto set_union_distribution_weight(lispb::schema::DeclarationId declaration,
                                       std::string name,
                                       std::uint64_t weight) -> bool;
    auto set_tagged_union_distribution_weight(lispb::schema::DeclarationId declaration,
                                              std::string name,
                                              std::uint64_t weight) -> bool;
    auto clear_union_distribution(lispb::schema::DeclarationId declaration) -> bool;
    auto clear_tagged_union_distribution(lispb::schema::DeclarationId declaration) -> bool;
    void clear_distributions();
    auto set_varint_distribution(std::optional<lispb::schema::TypeIdentity> source,
                                 std::vector<IntegerVarintDistributionEntry> entries,
                                 bool rows_present) -> bool;
    auto results() const -> PlannerAnalysisResults const&;
    auto status(lispb::schema::TypeId type) -> LayoutStatus;
    void replace_types(Document const& document,
                       std::optional<lispb::schema::TypeIdentity> selection);
    auto refresh(Document* document) -> bool;
  private:
    void validate_comparison_variants();
    void analyze_selected(Document const* document);
    PlannerAnalysisInputs inputs_;
    PlannerAnalysisResults results_;
    AbiProfile primary_abi_{AbiProfile::host_common()};
    AbiProfile comparison_abi_{AbiProfile::host_common()};
    std::optional<lispb::schema::TypeIdentity> varint_distribution_source_;
    std::vector<IntegerVarintDistributionEntry> varint_distribution_entries_;
    bool varint_distribution_rows_present_{};
    DistributionTable union_distributions_;
    DistributionTable tagged_union_distributions_;
    std::map<lispb::schema::TypeId, LayoutStatus> status_cache_;
    std::vector<RelationshipTargetFacts> status_relationship_targets_;
    Revision inputs_revision_{};
    Document const* document_{};
    Revision document_revision_{invalid_revision};
    Revision cached_inputs_revision_{invalid_revision};
    Revision status_inputs_revision_{invalid_revision};
};

} // namespace ioj::layout
