#include "ExampleDataTable.h"

#include "SpaceGame/support/logging/SandboxLogCategories.h"

auto FExampleDataTableRow::example_row_function() const -> float {
    return example_float + static_cast<float>(example_int);
}

UExampleDataTable::UExampleDataTable() {
    RowStruct = FExampleDataTableRow::StaticStruct();
}

FVector UExampleDataTable::example_table_function(FName row_name) const {
    auto const* row{FindRow<FExampleDataTableRow>(row_name, TEXT("example_table_function"))};
    if (!row) {
        UE_LOG(LogSandbox,
               Warning,
               TEXT("Could not find example data table row '%s'."),
               *row_name.ToString());
        return FVector::ZeroVector;
    }

    return row->example_vector;
}

void UExampleDataTable::example_call_in_editor() const {}
