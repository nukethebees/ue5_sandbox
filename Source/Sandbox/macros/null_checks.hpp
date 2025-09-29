// Initialse a pointer and return early with a log message if it's null
#define TRY_INIT_PTR(PTR_NAME, PTR_EXPR)                    \
    auto PTR_NAME{PTR_EXPR};                                \
    if (!PTR_NAME) {                                        \
        UE_LOGFMT(LogTemp, Warning, #PTR_NAME " is null."); \
        return;                                             \
    }
