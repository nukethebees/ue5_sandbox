#include <ioj/layout/analysis/analysis_storage.hpp>
namespace ioj::layout {
void AnalysisStorage::clear() {
    runs.clear();
    totals.clear();
    types.reset();
    scalars.reset();
    codes.reset();
    components.reset();
    fields.reset();
    columns.reset();
    relationships.reset();
    tags.reset();
    workloads.reset();
    selections.reset();
    distributions.reset();
    distribution_entries.reset();
    strings.clear();
}
}
