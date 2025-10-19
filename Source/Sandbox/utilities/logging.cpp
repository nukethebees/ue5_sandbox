#include "Sandbox/utilities/logging.h"

#include "Sandbox/SandboxLogCategories.h"

namespace ml {
void log_value_is_x(FString const& var_name,
                    FString const& null_form,
                    std::source_location const loc) {
    FString const fn_name{StringCast<TCHAR>(loc.function_name())};
    FString const file_name{StringCast<TCHAR>(loc.file_name())};
    UE_LOGFMT(LogSandboxCore,
              Warning,
              "{0} is {1}.\n    File: {4}\n    Line: {3}\n    Function: {2}",
              *var_name,
              *null_form,
              *fn_name,
              loc.line(),
              *file_name);
}
}
