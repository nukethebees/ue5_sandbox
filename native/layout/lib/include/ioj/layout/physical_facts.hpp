#pragma once

#include <ioj/layout/abi_profile.hpp>
#include <ioj/layout/analyzer/record_analysis.hpp>
#include <ioj/layout/analyzer/tagged_union_analysis.hpp>
#include <ioj/layout/analyzer/union_analysis.hpp>
#include <ioj/layout/diagnostic.hpp>
#include <ioj/layout/resolved_variant.hpp>
#include <ioj/layout/type_facts.hpp>

#include <codegen/ast/indices.h>
#include <codegen/schema/module_handle.h>
#include <codegen/schema/physical_type_form.h>
#include <lispb/indices.h>
#include <lispb/project_loader.h>
#include <lispb/schema/indices.h>

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace ioj::layout {

struct PhysicalFactsResult {
    std::optional<TypeFacts> facts{};
    std::vector<Diagnostic> diagnostics{};
};

class PhysicalFactsResolver {
  public:
    PhysicalFactsResolver(lispb::schema::TypeGraph const& types,
                          AbiProfile const& abi,
                          ResolvedVariant const* variant = nullptr);

    auto resolve(lispb::schema::ResolvedOccurrenceId const use) -> PhysicalFactsResult;
    auto resolve(lispb::schema::TypeId type) -> PhysicalFactsResult;
    auto resolve_spelling(std::string const& spelling, lispb::codegen::ModuleHandle module = {})
        -> PhysicalFactsResult;
    auto analyze_record(lispb::schema::TypeId type) -> RecordAnalysis;
    auto analyze_union(lispb::schema::TypeId type) -> UnionAnalysis;
    auto analyze_tagged_union(lispb::schema::TypeId type) -> TaggedUnionAnalysis;
  private:
    void check_supplied_layout(lispb::schema::TypeId type,
                               std::optional<std::uint64_t>& size,
                               std::optional<std::uint64_t>& alignment,
                               std::vector<Diagnostic>& diagnostics) const;
    auto facts_for(lispb::schema::ResolvedOccurrenceId const use,
                   std::vector<lispb::schema::TypeId>& active,
                   std::vector<Diagnostic>& diagnostics,
                   std::string const& context) -> std::optional<TypeFacts>;
    auto facts_for(lispb::schema::TypeId semantic_type,
                   lispb::codegen::CppTypeId cpp_type,
                   lispb::codegen::PhysicalTypeForm form,
                   lispb::StringId object_spelling,
                   bool names_semantic_type,
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
    ResolvedVariant const* variant_{};
    std::map<lispb::schema::TypeId, TypeFacts> known_;
};

} // namespace ioj::layout
