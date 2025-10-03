#include <source_location>

#define RETURN_IF_NULLPTR(PTR_NAME)                                    \
    do {                                                               \
        if (!PTR_NAME) {                                               \
            auto const loc{std::source_location::current()};           \
            FString const fn_name{ANSI_TO_TCHAR(loc.function_name())}; \
            FString const file_name{ANSI_TO_TCHAR(loc.file_name())};   \
            UE_LOGFMT(LogTemp,                                         \
                      Warning,                                         \
                      #PTR_NAME " is nullptr. [{0}, {1}, {2}]",        \
                      *fn_name,                                        \
                      loc.line(),                                      \
                      *file_name);                                     \
            return;                                                    \
        }                                                              \
    } while (0)

// Initialse a pointer and return early with a log message if it's null
#define TRY_INIT_PTR(PTR_NAME, PTR_EXPR) \
    auto PTR_NAME{PTR_EXPR};             \
    RETURN_IF_NULLPTR(PTR_NAME)

// TRY_INIT_PTR but for Behaviour Tree tasks
#define TRY_INIT_BTTASK_PTR(PTR_NAME, PTR_EXPR)                \
    auto PTR_NAME{PTR_EXPR};                                   \
    if (!PTR_NAME) {                                           \
        UE_LOGFMT(LogTemp, Warning, #PTR_NAME " is nullptr."); \
        return EBTNodeResult::Failed;                          \
    }

// With return value
#define RETURN_VALUE_IF_NULLPTR(PTR_NAME, VALUE)                       \
    do {                                                               \
        if (!PTR_NAME) {                                               \
            auto const loc{std::source_location::current()};           \
            FString const fn_name{ANSI_TO_TCHAR(loc.function_name())}; \
            FString const file_name{ANSI_TO_TCHAR(loc.file_name())};   \
            UE_LOGFMT(LogTemp,                                         \
                      Warning,                                         \
                      #PTR_NAME " is nullptr. [{0}, {1}, {2}]",        \
                      *fn_name,                                        \
                      loc.line(),                                      \
                      *file_name);                                     \
            return VALUE;                                              \
        }                                                              \
    } while (0)

#define INIT_PTR_OR_RETURN_VALUE(PTR_NAME, PTR_EXPR, RETURN_VALUE) \
    auto PTR_NAME{PTR_EXPR};                                       \
    RETURN_VALUE_IF_NULLPTR(PTR_NAME, RETURN_VALUE)

// For objects that require calls to IsValid()
#define RETURN_IF_INVALID(VAR_NAME)                                  \
    do {                                                             \
        if (!VAR_NAME.IsValid()) {                                   \
            UE_LOGFMT(LogTemp, Warning, #VAR_NAME " is not valid."); \
            return;                                                  \
        }                                                            \
    } while (0)

#define TRY_INIT_VALID(VAR_NAME, VAR_EXPR) \
    auto VAR_NAME{VAR_EXPR};               \
    RETURN_IF_INVALID(VAR_NAME)

// std::optional
#define RETURN_IF_NULLOPT(VAR_NAME)                                \
    do {                                                           \
        if (!VAR_NAME) {                                           \
            UE_LOGFMT(LogTemp, Warning, #VAR_NAME " is nullopt."); \
            return;                                                \
        }                                                          \
    } while (0)

#define TRY_INIT_OPTIONAL(VAR_NAME, VAR_EXPR) \
    auto VAR_NAME{VAR_EXPR};                  \
    RETURN_IF_NULLOPT(VAR_NAME)
