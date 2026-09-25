#pragma once

#include <ioj/layout/analyzer.hpp>

#include <map>

namespace ioj::layout {

struct PhysicalFactsResult {
    std::optional<TypeFacts> facts;
    std::vector<Diagnostic> diagnostics;
};

class PhysicalFactsResolver {
  public:
    PhysicalFactsResolver(lispb::schema::TypeGraph const& types,
                          AbiProfile const& abi,
                          Variant const* variant = nullptr);

    auto resolve(lispb::schema::ResolvedTypeRef const& use) -> PhysicalFactsResult;
    auto resolve(lispb::schema::TypeId type) -> PhysicalFactsResult;
    auto resolve_spelling(std::string const& spelling, std::string const& module = {})
        -> PhysicalFactsResult;
    auto analyze_record(lispb::schema::TypeId type) -> RecordAnalysis;
    auto analyze_union(lispb::schema::TypeId type) -> UnionAnalysis;
    auto analyze_tagged_union(lispb::schema::TypeId type) -> TaggedUnionAnalysis;
  private:
    void check_supplied_layout(lispb::schema::TypeId type,
                               std::optional<std::uint64_t>& size,
                               std::optional<std::uint64_t>& alignment,
                               std::vector<Diagnostic>& diagnostics) const;
    auto facts_for(lispb::schema::ResolvedTypeRef const& use,
                   std::vector<lispb::schema::TypeId>& active,
                   std::vector<Diagnostic>& diagnostics,
                   std::string const& context) -> std::optional<TypeFacts>;
    auto facts_for(lispb::schema::TypeId type,
                   std::vector<lispb::schema::TypeId>& active,
                   std::vector<Diagnostic>& diagnostics,
                   std::string const& context) -> std::optional<TypeFacts>;
    auto lookup(std::string const& spelling,
                std::vector<Diagnostic>& diagnostics,
                std::string const& context) const -> std::optional<TypeFacts>;
    auto analyze_record(lispb::schema::TypeId type, std::vector<lispb::schema::TypeId>& active)
        -> RecordAnalysis;
    auto analyze_union(lispb::schema::TypeId type, std::vector<lispb::schema::TypeId>& active)
        -> UnionAnalysis;
    auto analyze_tagged_union(lispb::schema::TypeId type,
                              std::vector<lispb::schema::TypeId>& active) -> TaggedUnionAnalysis;

    lispb::schema::TypeGraph const& types_;
    AbiProfile const& abi_;
    Variant const* variant_{};
    std::map<lispb::schema::TypeId, TypeFacts> known_;
};

} // namespace ioj::layout
