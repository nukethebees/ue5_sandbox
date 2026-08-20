// A set of macros for generating large switch statements
// CASE_MACRO is a macro that is expanded for each case

#define SWITCH_STAMP4(n, CASE_MACRO) \
    CASE_MACRO(n);                   \
    CASE_MACRO(n + 1);               \
    CASE_MACRO(n + 2);               \
    CASE_MACRO(n + 3)

#define SWITCH_STAMP16(n, CASE_MACRO) \
    SWITCH_STAMP4(n, CASE_MACRO);     \
    SWITCH_STAMP4(n + 4, CASE_MACRO); \
    SWITCH_STAMP4(n + 8, CASE_MACRO); \
    SWITCH_STAMP4(n + 12, CASE_MACRO)

#define SWITCH_STAMP64(n, CASE_MACRO)   \
    SWITCH_STAMP16(n, CASE_MACRO);      \
    SWITCH_STAMP16(n + 16, CASE_MACRO); \
    SWITCH_STAMP16(n + 32, CASE_MACRO); \
    SWITCH_STAMP16(n + 48, CASE_MACRO)

#define SWITCH_STAMP256(n, CASE_MACRO)   \
    SWITCH_STAMP64(n, CASE_MACRO);       \
    SWITCH_STAMP64(n + 64, CASE_MACRO);  \
    SWITCH_STAMP64(n + 128, CASE_MACRO); \
    SWITCH_STAMP64(n + 192, CASE_MACRO)

// Unfolds into a large switch statement
// N_CASES: the number of cases to generate
// TOP_LEVEL_STAMP: A macro for the top of the switch statement
//                  It should instantiate the case macro
#define SWITCH_STAMP(N_CASES, TOP_LEVEL_STAMP) TOP_LEVEL_STAMP(SWITCH_STAMP##N_CASES, N_CASES)

/*
Example:

#define TRIGGER_CASE(i)                                                                     \
    case i: {                                                                               \
        if constexpr (i < N_TYPES) {                                                        \
            std::get<i>(foo).bar()                                                          \
        }                                                                                   \
        break;                                                                              \
    }

#define TRIGGER_VISIT_STAMP(stamper, N_CASES)                                            \
    do {                                                                                 \
        static_assert(N_TYPES <= (N_CASES), "n is too large for this expansion.");       \
        switch (id.tuple_index()) {                                                      \
            stamper(0, TRIGGER_CASE);                                                    \
            default: {                                                                   \
                self.log_warning(TEXT("Unhandled trigger type: %d."), id.tuple_index()); \
                break;                                                                   \
            }                                                                            \
        }                                                                                \
    } while (0)

if constexpr (N_TYPES <= 4) {
    SWITCH_STAMP(4, TRIGGER_VISIT_STAMP);
} else if constexpr (N_TYPES <= 16) {
    SWITCH_STAMP(16, TRIGGER_VISIT_STAMP);
} else if constexpr (N_TYPES <= 64) {
    SWITCH_STAMP(64, TRIGGER_VISIT_STAMP);
} else if constexpr (N_TYPES <= 256) {
    SWITCH_STAMP(256, TRIGGER_VISIT_STAMP);
} else {
    self.log_warning(TEXT("Too many types for branch. This should never hit."));
    return;
}

Expansion:
----------------------------------------------------------------------------------------------------
SWITCH_STAMP(256, TRIGGER_VISIT_STAMP)

----------------------------------------------------------------------------------------------------
TRIGGER_VISIT_STAMP(SWITCH_STAMP256, 256)

----------------------------------------------------------------------------------------------------
do {
    static_assert(N_TYPES <= (256), "n is too large for this expansion.");
    switch (id.tuple_index()) {
        SWITCH_STAMP256(0, TRIGGER_CASE); \
        default: {
            self.log_warning(TEXT("Unhandled trigger type: %d."), id.tuple_index());
            break;
        }
    }
} while (0)

----------------------------------------------------------------------------------------------------
do {
    static_assert(N_TYPES <= (256), "n is too large for this expansion.");
    switch (id.tuple_index()) {
        case 0: {
            if constexpr (0 < N_TYPES) {
                std::get<0>(foo).bar)()
            }
            break;
        }
        // And so on...
        default: {
            self.log_warning(TEXT("Unhandled trigger type: %d."), id.tuple_index());
            break;
        }
    }
} while (0)
*/
