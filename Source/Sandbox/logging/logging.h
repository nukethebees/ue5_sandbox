#pragma once

#include <source_location>

#include "CoreMinimal.h"

namespace ml {
// Need to mark with MODULE_API so this can be used in other modules
SANDBOX_API void log_value_is_x(FString const& var_name,
                                FString const& null_form,
                                std::source_location const loc = std::source_location::current());
}
