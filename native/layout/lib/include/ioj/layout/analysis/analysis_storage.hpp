#pragma once
#include <ioj/layout/analysis/analysis_run.hpp>
#include <ioj/layout/analysis/code_table.hpp>
#include <ioj/layout/analysis/component_table.hpp>
#include <ioj/layout/analysis/distribution_entry_table.hpp>
#include <ioj/layout/analysis/distribution_table.hpp>
#include <ioj/layout/analysis/element_totals.hpp>
#include <ioj/layout/analysis/packed_field_table.hpp>
#include <ioj/layout/analysis/physical_fact_table.hpp>
#include <ioj/layout/analysis/relationship_table.hpp>
#include <ioj/layout/analysis/scalar_table.hpp>
#include <ioj/layout/analysis/selection_table.hpp>
#include <ioj/layout/analysis/soa_column_table.hpp>
#include <ioj/layout/analysis/tag_table.hpp>
#include <ioj/layout/analysis/type_analysis_table.hpp>
#include <ioj/layout/analysis/workload_table.hpp>

#include <lispb/strings.h>

#include <vector>

namespace ioj::layout {
struct AnalysisStorage {
    void clear();
    // Keep text IDs valid until the analysis tables are cleared.
    lispb::Strings strings;
    std::vector<AnalysisRun> runs;
    std::vector<ElementTotals> totals;
    TypeAnalysisTable types;
    PhysicalFactTable physical_facts;
    ScalarTable scalars;
    CodeTable codes;
    ComponentTable components;
    PackedFieldTable fields;
    SoaColumnTable columns;
    RelationshipTable relationships;
    TagTable tags;
    WorkloadTable workloads;
    SelectionTable selections;
    DistributionTable distributions;
    DistributionEntryTable distribution_entries;
};
}
