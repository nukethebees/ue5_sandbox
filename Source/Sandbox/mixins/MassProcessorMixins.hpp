#pragma once

#include <type_traits>

#include "CoreMinimal.h"

#include "MassProcessingTypes.h"

namespace ml {
struct MassProcessorMixins {
    template <typename Self>
    void set_execution_flags(this Self& self, EProcessorExecutionFlags flags) {
        using U = std::underlying_type_t<EProcessorExecutionFlags>;
        self.ExecutionFlags = static_cast<U>(flags);
    }
};
}
