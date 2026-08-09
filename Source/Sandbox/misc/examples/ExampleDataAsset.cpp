#include "ExampleDataAsset.h"

#include "Sandbox/logging/SandboxLogCategories.h"

void UExampleDataAsset::example_call_in_editor() {
    UE_LOG(LogSandbox,
           Log,
           TEXT("Example data asset values: float=%f, int=%d, bool=%s, enum=%d, vector=%s."),
           values.example_float,
           values.example_int,
           values.example_bool ? TEXT("true") : TEXT("false"),
           static_cast<int32>(values.example_state),
           *values.example_vector.ToString());
}
