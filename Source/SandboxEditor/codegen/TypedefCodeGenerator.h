#pragma once

#include "CoreMinimal.h"

#include "Sandbox/logging/LogMsgMixin.hpp"

/**
 * Editor utility for generating strong typedef wrapper structs.
 * Runs a Python script to generate type-safe USTRUCT wrappers with reflection support.
 */
class FTypedefCodeGenerator : public ml::LogMsgMixin<"FTypedefCodeGenerator"> {
  public:
    /**
     * Generate strong typedef wrapper structs by running the Python generator script.
     * @return true if generation succeeded, false otherwise
     */
    static bool generate_typedefs();
  private:
    FTypedefCodeGenerator() = default;

    static FString get_python_script_path();
    static bool run_python_script(FString const& script_path);
};
