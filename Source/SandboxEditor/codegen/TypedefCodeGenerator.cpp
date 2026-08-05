#include "SandboxEditor/codegen/TypedefCodeGenerator.h"

#include "HAL/PlatformProcess.h"
#include "Misc/Paths.h"

bool FTypedefCodeGenerator::generate_typedefs() {
    constexpr auto logger{NestedLogger<"generate_typedefs">()};
    logger.log_log(TEXT("Generating strong typedefs..."));

    FString const script_path{get_python_script_path()};
    if (script_path.IsEmpty()) {
        logger.log_error(TEXT("Failed to find Python script"));
        return false;
    }

    logger.log_log(TEXT("Running Python script: %s"), *script_path);

    bool const success{run_python_script(script_path)};

    if (success) {
        logger.log_log(TEXT("Typedef generation completed successfully!"));
    } else {
        logger.log_error(TEXT("Typedef generation failed!"));
    }

    return success;
}

FString FTypedefCodeGenerator::get_python_script_path() {
    constexpr auto logger{NestedLogger<"get_python_script_path">()};

    FString const project_dir{FPaths::ProjectDir()};
    FString const script_path{project_dir /
                              TEXT("Source/SandboxEditor/codegen/strong_typedefs.py")};

    if (!FPaths::FileExists(script_path)) {
        logger.log_error(TEXT("Python script not found at: %s"), *script_path);
        return FString{};
    }

    return script_path;
}

bool FTypedefCodeGenerator::run_python_script(FString const& script_path) {
    constexpr auto logger{NestedLogger<"run_python_script">()};

    // Get Python executable path (try "python" first, fallback to "python3")
    FString python_exe{TEXT("python")};

    // Prepare command arguments
    FString args{FString::Printf(TEXT("\"%s\""), *script_path)};

    // Run the process
    int32 return_code{-1};
    FString std_out{};
    FString std_err{};

    bool const success{
        FPlatformProcess::ExecProcess(*python_exe, *args, &return_code, &std_out, &std_err)};

    if (!success) {
        logger.log_error(TEXT("Failed to execute Python script"));
        return false;
    }

    // Log output
    if (!std_out.IsEmpty()) {
        logger.log_log(TEXT("Python output:\n%s"), *std_out);
    }

    if (!std_err.IsEmpty()) {
        logger.log_warning(TEXT("Python stderr:\n%s"), *std_err);
    }

    if (return_code != 0) {
        logger.log_error(TEXT("Python script returned error code: %d"), return_code);
        return false;
    }

    return true;
}
