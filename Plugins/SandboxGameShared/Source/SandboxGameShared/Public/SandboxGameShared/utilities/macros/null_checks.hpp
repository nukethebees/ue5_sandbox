#include "SandboxGameShared/logging/logging.h"

#define RETURN_VALUE_AND_WARN_IF_EXPR(VAR_NAME, BOOL_EXPR, NULL_FORM, RETURN_VALUE) \
    do {                                                                            \
        if (BOOL_EXPR) {                                                            \
            ioj::log_value_is_x(TEXT(#VAR_NAME), TEXT(NULL_FORM));                   \
            return RETURN_VALUE;                                                    \
        }                                                                           \
    } while (0)

#define RETURN_IF_NULLPTR(VAR_NAME) \
    RETURN_VALUE_AND_WARN_IF_EXPR(VAR_NAME, !(VAR_NAME), "nullptr", )

#define TRY_INIT_PTR(VAR_NAME, PTR_EXPR) \
    auto VAR_NAME{PTR_EXPR};             \
    RETURN_IF_NULLPTR(VAR_NAME)

#define RETURN_IF_INVALID(VAR_NAME) \
    RETURN_VALUE_AND_WARN_IF_EXPR(VAR_NAME, !(VAR_NAME.IsValid()), "invalid", )
