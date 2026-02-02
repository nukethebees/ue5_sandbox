#include "Sandbox/logging/logging.h"

// Macros for testing if variables are in a valid state
// If not, the function returns early and logs a warning
// Mainly used for null checking pointers

// INIT macros initialise variables and then perform a null checks

#define WARN_IS_FALSE(CATEGORY, EXPR) \
    UE_LOG(CATEGORY, Warning, TEXT(#EXPR " is false"))

#define WARN_IF_EXPR(BOOL_EXPR)                                 \
    do {                                                        \
        if ((BOOL_EXPR)) {                                      \
            ml::log_value_is_x(TEXT(#BOOL_EXPR), TEXT("true")); \
        }                                                       \
    } while (0)

#define RETURN_VALUE_AND_WARN_IF_EXPR(VAR_NAME, BOOL_EXPR, NULL_FORM, RETURN_VALUE) \
    do {                                                                            \
        if (BOOL_EXPR) {                                                            \
            ml::log_value_is_x(TEXT(#VAR_NAME), TEXT(NULL_FORM));                   \
            return RETURN_VALUE;                                                    \
        }                                                                           \
    } while (0)

#define CONTINUE_AND_WARN_IF_EXPR(VAR_NAME, BOOL_EXPR, NULL_FORM) \
    if (BOOL_EXPR) {                                              \
        ml::log_value_is_x(TEXT(#VAR_NAME), TEXT(NULL_FORM));     \
        continue;                                                 \
    }

#define IF_EXPR_ELSE_WARN(EXPR)                         \
    if (!(EXPR)) {                                      \
        ml::log_value_is_x(TEXT(#EXPR), TEXT("false")); \
    } else
#define WARN_IF_EXPR_ELSE(EXPR)                         \
    if ((EXPR)) {                                       \
        ml::log_value_is_x(TEXT(#EXPR), TEXT("false")); \
    } else

// Pointers
// -------------------------------------------------------------------------------------------------
#define RETURN_IF_NULLPTR(VAR_NAME) \
    RETURN_VALUE_AND_WARN_IF_EXPR(VAR_NAME, !(VAR_NAME), "nullptr", )
#define RETURN_VALUE_IF_NULLPTR(VAR_NAME, RETURN_VALUE) \
    RETURN_VALUE_AND_WARN_IF_EXPR(VAR_NAME, !VAR_NAME, "nullptr", RETURN_VALUE)

#define TRY_INIT_PTR(VAR_NAME, PTR_EXPR) \
    auto VAR_NAME{PTR_EXPR};             \
    RETURN_IF_NULLPTR(VAR_NAME)
#define INIT_PTR_OR_RETURN_VALUE(VAR_NAME, PTR_EXPR, RETURN_VALUE) \
    auto VAR_NAME{PTR_EXPR};                                       \
    RETURN_VALUE_IF_NULLPTR(VAR_NAME, RETURN_VALUE)

#define CONTINUE_IF_NULLPTR(VAR_NAME) CONTINUE_AND_WARN_IF_EXPR(VAR_NAME, !(VAR_NAME), "nullptr")

// Objects with IsValid()
// -------------------------------------------------------------------------------------------------
#define RETURN_IF_INVALID(VAR_NAME) \
    RETURN_VALUE_AND_WARN_IF_EXPR(VAR_NAME, !(VAR_NAME.IsValid()), "invalid", )

#define TRY_INIT_VALID(VAR_NAME, VAR_EXPR) \
    auto VAR_NAME{VAR_EXPR};               \
    RETURN_IF_INVALID(VAR_NAME)

// std::optional
// -------------------------------------------------------------------------------------------------
#define RETURN_IF_NULLOPT(VAR_NAME) \
    RETURN_VALUE_AND_WARN_IF_EXPR(VAR_NAME, !(VAR_NAME), "nullopt", )

#define TRY_INIT_OPTIONAL(VAR_NAME, VAR_EXPR) \
    auto VAR_NAME{VAR_EXPR};                  \
    RETURN_IF_NULLOPT(VAR_NAME)

#define RETURN_VALUE_IF_NULLOPT(VAR_NAME, RETURN_VALUE) \
    RETURN_VALUE_AND_WARN_IF_EXPR(VAR_NAME, !(VAR_NAME), "nullopt", RETURN_VALUE)

#define INIT_OPTIONAL_OR_RETURN_VALUE(VAR_NAME, PTR_EXPR, RETURN_VALUE) \
    auto VAR_NAME{PTR_EXPR};                                            \
    RETURN_VALUE_IF_NULLOPT(VAR_NAME, RETURN_VALUE)

// Boolean
// -------------------------------------------------------------------------------------------------
#define RETURN_IF_TRUE(BOOLEAN_EXPR) \
    RETURN_VALUE_AND_WARN_IF_EXPR(#BOOLEAN_EXPR, BOOLEAN_EXPR, "true", )
#define RETURN_IF_FALSE(BOOLEAN_EXPR) \
    RETURN_VALUE_AND_WARN_IF_EXPR(#BOOLEAN_EXPR, !(BOOLEAN_EXPR), "false", )

#define RETURN_VALUE_IF_TRUE(BOOLEAN_EXPR, RETURN_VALUE) \
    RETURN_VALUE_AND_WARN_IF_EXPR(#BOOLEAN_EXPR, BOOLEAN_EXPR, "true", RETURN_VALUE)
#define RETURN_VALUE_IF_FALSE(BOOLEAN_EXPR, RETURN_VALUE) \
    RETURN_VALUE_AND_WARN_IF_EXPR(#BOOLEAN_EXPR, !(BOOLEAN_EXPR), "false", RETURN_VALUE)

#define INIT_OR_RETURN_VALUE_IF_TRUE(VAR_TYPE, VAR_NAME, INIT_EXPR, RETURN_VALUE) \
    VAR_TYPE VAR_NAME{INIT_EXPR};                                                 \
    RETURN_VALUE_IF_TRUE(VAR_NAME, RETURN_VALUE)

#define INIT_OR_RETURN_VALUE_IF_FALSE(VAR_TYPE, VAR_NAME, INIT_EXPR, RETURN_VALUE) \
    VAR_TYPE VAR_NAME{INIT_EXPR};                                                  \
    RETURN_VALUE_IF_FALSE(VAR_NAME, RETURN_VALUE)

#define CONTINUE_IF_TRUE(BOOLEAN_EXPR) \
    CONTINUE_AND_WARN_IF_EXPR(BOOLEAN_EXPR, (BOOLEAN_EXPR), "true")
#define CONTINUE_IF_FALSE(BOOLEAN_EXPR) \
    CONTINUE_AND_WARN_IF_EXPR(BOOLEAN_EXPR, !(BOOLEAN_EXPR), "false")
