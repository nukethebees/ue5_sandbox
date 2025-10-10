#include "Sandbox/utilities/logging.h"

namespace ml {
void log_value_is_x(FString const& var_name,
                    FString const& null_form,
                    std::source_location const loc) {
    FString const fn_name{StringCast<TCHAR>(loc.function_name())};
    FString const file_name{StringCast<TCHAR>(loc.file_name())};
    UE_LOGFMT(LogTemp,
              Warning,
              "{0} is {1}. [{2}, {3}, {4}]",
              *var_name,
              *null_form,
              *fn_name,
              loc.line(),
              *file_name);
}
}
