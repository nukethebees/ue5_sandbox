#pragma once

#include <type_traits>
#include <utility>

#include "CoreMinimal.h"

#include "Sandbox/utilities/string_literal_wrapper.h"

#define GLOBAL_CATEGORY log_category
#define LOCAL_CATEGORY log_cat

#define LOG_MSG_TO(VERBOSITY) \
    log_to<ELogVerbosity::VERBOSITY, LOCAL_CATEGORY>(std::move(fmt), std::forward<Args>(args)...);

#define LOG_MSG(VERBOSITY) \
    log_to<ELogVerbosity::VERBOSITY, GLOBAL_CATEGORY>(std::move(fmt), std::forward<Args>(args)...);

namespace ml {
template <StaticTCharString tag, auto& log_category_ = LogTemp>
struct LogMsgMixin {
    static constexpr auto* get_tag() { return tag.data(); }
    static constexpr auto& GLOBAL_CATEGORY{log_category_};

    template <StaticTCharString inner_tag>
    static consteval auto NestedLogger() {
        using Str = StaticTCharString<3>;
        constexpr Str sep{": "};
        return LogMsgMixin<tag.concatenate_all<tag, sep, inner_tag>(), log_category_>{};
    }

    // Strip references because the format string requires value types
    template <typename... Args>
    using DecayedFormatString =
        UE::Core::TCheckedFormatString<typename FString::FmtCharType,
                                       std::remove_cv_t<std::remove_reference_t<Args>>...>;

    template <ELogVerbosity::Type verbosity, auto& LOCAL_CATEGORY, typename... Args>
        requires (verbosity < ELogVerbosity::NumVerbosity)
    void log_to(DecayedFormatString<Args...>&& fmt, Args&&... args) const {
#define LOG_BRANCH(VERBOSITY)                                              \
    if constexpr (verbosity == ELogVerbosity::VERBOSITY) {                 \
        UE_LOG(LOCAL_CATEGORY, VERBOSITY, TEXT("%s: %s"), get_tag(), *msg) \
    }
#define LOG_ELSE_BRANCH(VERBOSITY) else LOG_BRANCH(VERBOSITY)

        if constexpr (verbosity > LOCAL_CATEGORY.GetCompileTimeVerbosity()) {
            return;
        }

        if (LOCAL_CATEGORY.IsSuppressed(verbosity)) {
            return;
        }

        auto msg{FString::Printf(std::move(fmt), std::forward<Args>(args)...)};

        LOG_BRANCH(Fatal)
        LOG_ELSE_BRANCH(Error)
        LOG_ELSE_BRANCH(Warning)
        LOG_ELSE_BRANCH(Log)
        LOG_ELSE_BRANCH(Display)
        LOG_ELSE_BRANCH(Verbose)
        LOG_ELSE_BRANCH(VeryVerbose)
#undef LOG_BRANCH
#undef LOG_ELSE_BRANCH
    }

    template <ELogVerbosity::Type verbosity, typename... Args>
        requires (verbosity < ELogVerbosity::NumVerbosity)
    void log(DecayedFormatString<Args...>&& fmt, Args&&... args) const {
        log_to<verbosity, GLOBAL_CATEGORY>(std::move(fmt), std::forward<Args>(args)...);
    }

    // Helper macro for development-only code
#if UE_BUILD_DEVELOPMENT
#define LOG_DEV_CODE(code) code
#else
#define LOG_DEV_CODE(code)
#endif

    // Macro to generate logging functions
#define LOG_X_FN(LOWERCASE, VERBOSITY)                                               \
    template <typename... Args>                                                      \
    void log_##LOWERCASE(DecayedFormatString<Args...>&& fmt, Args&&... args) const { \
        LOG_MSG(VERBOSITY);                                                          \
    }

    // Macro to generate _to variant logging functions
#define LOG_X_TO_FN(LOWERCASE, VERBOSITY)                                                 \
    template <auto& LOCAL_CATEGORY, typename... Args>                                     \
    void log_##LOWERCASE##_to(DecayedFormatString<Args...>&& fmt, Args&&... args) const { \
        LOG_MSG_TO(VERBOSITY);                                                            \
    }

    // Development-only variants
#define LOG_X_FN_DEV(LOWERCASE, VERBOSITY)                                           \
    template <typename... Args>                                                      \
    void log_##LOWERCASE(DecayedFormatString<Args...>&& fmt, Args&&... args) const { \
        LOG_DEV_CODE(LOG_MSG(VERBOSITY);)                                            \
    }

#define LOG_X_TO_FN_DEV(LOWERCASE, VERBOSITY)                                             \
    template <auto& LOCAL_CATEGORY, typename... Args>                                     \
    void log_##LOWERCASE##_to(DecayedFormatString<Args...>&& fmt, Args&&... args) const { \
        LOG_DEV_CODE(LOG_MSG_TO(VERBOSITY);)                                              \
    }

    // Generate all logging functions
    LOG_X_TO_FN(fatal, Fatal);
    LOG_X_FN(fatal, Fatal);

    LOG_X_TO_FN(error, Error);
    LOG_X_FN(error, Error);

    LOG_X_TO_FN(warning, Warning);
    LOG_X_FN(warning, Warning);

    LOG_X_TO_FN(display, Display);
    LOG_X_FN(display, Display);

    LOG_X_TO_FN_DEV(log, Log);
    LOG_X_FN_DEV(log, Log);

    LOG_X_TO_FN_DEV(verbose, Verbose);
    LOG_X_FN_DEV(verbose, Verbose);

    LOG_X_TO_FN_DEV(very_verbose, VeryVerbose);
    LOG_X_FN_DEV(very_verbose, VeryVerbose);

#undef LOG_X_FN
#undef LOG_X_TO_FN
#undef LOG_X_FN_DEV
#undef LOG_X_TO_FN_DEV
#undef LOG_DEV_CODE
#undef LOG_MSG
#undef GLOBAL_CATEGORY
#undef LOCAL_CATEGORY
};
}
