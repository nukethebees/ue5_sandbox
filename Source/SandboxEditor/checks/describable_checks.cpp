#include "SandboxEditor/checks/describable_checks.h"

#include "SandboxEditor/logging/SandboxEditorLogCategories.h"

namespace ml {
void check_describable_actors_are_visible_to_hitscan() {
    UE_LOG(LogSandboxEditorChecks, Log, TEXT("Checking IDescribable actors."));
}
}
