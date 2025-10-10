#include "Sandbox/utilities/logging.h"

#define RETURN_VALUE_IF_BANG_VAR_BASE_MACRO(VAR_NAME, NULL_FORM, RETURN_VALUE) \
    do {                                                                       \
        if (!VAR_NAME) {                                                       \
            ml::log_value_is_x(TEXT(#VAR_NAME), NULL_FORM);                    \
            return RETURN_VALUE;                                               \
        }                                                                      \
    } while (0)

#define RETURN_VALUE_IF_NULLPTR(PTR_NAME, VALUE) \
    RETURN_VALUE_IF_BANG_VAR_BASE_MACRO(PTR_NAME, TEXT("nullptr"), VALUE)

#define RETURN_IF_BANG_VAR_BASE_MACRO(VAR_NAME, NULL_FORM) \
    RETURN_VALUE_IF_BANG_VAR_BASE_MACRO(VAR_NAME, NULL_FORM, )

#define RETURN_IF_NULLPTR(PTR_NAME) RETURN_IF_BANG_VAR_BASE_MACRO(PTR_NAME, TEXT("nullptr"))

// Initialse a pointer and return early with a log message if it's null
#define TRY_INIT_PTR(PTR_NAME, PTR_EXPR) \
    auto PTR_NAME{PTR_EXPR};             \
    RETURN_IF_NULLPTR(PTR_NAME)

#define INIT_PTR_OR_RETURN_VALUE(PTR_NAME, PTR_EXPR, RETURN_VALUE) \
    auto PTR_NAME{PTR_EXPR};                                       \
    RETURN_VALUE_IF_NULLPTR(PTR_NAME, RETURN_VALUE)

// TRY_INIT_PTR but for Behaviour Tree tasks
#define TRY_INIT_BTTASK_PTR(PTR_NAME, PTR_EXPR) \
    INIT_PTR_OR_RETURN_VALUE(PTR_NAME, PTR_EXPR, EBTNodeResult::Failed)

// For objects that require calls to IsValid()
#define RETURN_IF_INVALID(VAR_NAME)                               \
    do {                                                          \
        if (!VAR_NAME.IsValid()) {                                \
            ml::log_value_is_x(TEXT(#VAR_NAME), TEXT("invalid")); \
            return;                                               \
        }                                                         \
    } while (0)

#define TRY_INIT_VALID(VAR_NAME, VAR_EXPR) \
    auto VAR_NAME{VAR_EXPR};               \
    RETURN_IF_INVALID(VAR_NAME)

// std::optional
#define RETURN_IF_NULLOPT(VAR_NAME) RETURN_IF_BANG_VAR_BASE_MACRO(VAR_NAME, TEXT("nullopt"))

#define TRY_INIT_OPTIONAL(VAR_NAME, VAR_EXPR) \
    auto VAR_NAME{VAR_EXPR};                  \
    RETURN_IF_NULLOPT(VAR_NAME)
