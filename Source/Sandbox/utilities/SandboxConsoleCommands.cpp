#include "HAL/IConsoleManager.h"

#include "Sandbox/SandboxLogCategories.h"

static void cmd_sandbox_log(TArray<FString> const& args) {
    if (args.Num() != 1) {
        UE_LOG(LogSandboxCore, Warning, TEXT("Usage: set_all_sandbox_logs <VerbosityLevel>"));
        UE_LOG(LogSandboxCore,
               Warning,
               TEXT("Available levels: NoLogging, Fatal, Error, Warning, Display, Log, Verbose, "
                    "VeryVerbose, All"));
        return;
    }

    static FString const no_logging{TEXT("NoLogging")};
    static FString const fatal{TEXT("Fatal")};
    static FString const error{TEXT("Error")};
    static FString const warning{TEXT("Warning")};
    static FString const display{TEXT("Display")};
    static FString const log{TEXT("Log")};
    static FString const verbose{TEXT("Verbose")};
    static FString const very_verbose{TEXT("VeryVerbose")};
    static FString const all{TEXT("All")};

    FString const verbosity_string{args[0]};
    ELogVerbosity::Type new_verbosity{ELogVerbosity::Log};
    bool valid_verbosity{true};

    auto eq{[&](FString const& other) -> bool {
        return verbosity_string.Equals(other, ESearchCase::IgnoreCase);
    }};

    if (eq(no_logging)) {
        new_verbosity = ELogVerbosity::NoLogging;
    } else if (eq(fatal)) {
        new_verbosity = ELogVerbosity::Fatal;
    } else if (eq(error)) {
        new_verbosity = ELogVerbosity::Error;
    } else if (eq(warning)) {
        new_verbosity = ELogVerbosity::Warning;
    } else if (eq(display)) {
        new_verbosity = ELogVerbosity::Display;
    } else if (eq(log)) {
        new_verbosity = ELogVerbosity::Log;
    } else if (eq(verbose)) {
        new_verbosity = ELogVerbosity::Verbose;
    } else if (eq(very_verbose)) {
        new_verbosity = ELogVerbosity::VeryVerbose;
    } else if (eq(all)) {
        new_verbosity = ELogVerbosity::All;
    } else {
        valid_verbosity = false;
        UE_LOG(LogSandboxCore, Warning, TEXT("Invalid verbosity level: %s"), *verbosity_string);
        UE_LOG(LogSandboxCore,
               Warning,
               TEXT("Available levels: NoLogging, Fatal, Error, Warning, Display, Log, Verbose, "
                    "VeryVerbose, All"));
    }

    auto set{[new_verbosity](this auto& self, auto& cat, auto&... remaining) -> void {
        cat.SetVerbosity(new_verbosity);
        if constexpr (sizeof...(remaining)) {
            self(remaining...);
        }
    }};

    if (valid_verbosity) {
        set(LogSandboxFrameCount,
            // Systems
            LogSandboxCore,
            LogSandboxUI,
            LogSandboxMassEntity,
            LogSandboxCharacter,
            LogSandboxActor,
            LogSandboxActorComponent,
            LogSandboxSubsystem,
            // Gameplay
            LogSandboxWeapon,
            LogSandboxInput,
            LogSandboxInventory,
            LogSandboxHealth);

        UE_LOG(LogSandboxCore,
               Display,
               TEXT("Set all sandbox log categories to %s"),
               *verbosity_string);
    }
}

static FAutoConsoleCommand
    SandboxLogsCommand(TEXT("sandbox_log"),
                       TEXT("Sets the verbosity for all Sandbox log categories. Usage: "
                            "sandbox_log <VerbosityLevel>"),
                       FConsoleCommandWithArgsDelegate::CreateStatic(&cmd_sandbox_log));
