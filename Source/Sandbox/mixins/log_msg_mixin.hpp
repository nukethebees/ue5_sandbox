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
#define LOG_BRANCH(VERBOSITY)                                                 \
    if constexpr (verbosity == ELogVerbosity::VERBOSITY) {                    \
        UE_LOG(LOCAL_CATEGORY,                                                \
               VERBOSITY,                                                     \
               TEXT("%s: %s"),                                                \
               get_tag(),                                                     \
               *FString::Printf(std::move(fmt), std::forward<Args>(args)...)) \
    }
#define LOG_ELSE_BRANCH(VERBOSITY) else LOG_BRANCH(VERBOSITY)

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

    template <auto& LOCAL_CATEGORY, typename... Args>
    void log_fatal_to(DecayedFormatString<Args...>&& fmt, Args&&... args) const {
        LOG_MSG_TO(Fatal);
    }

    template <typename... Args>
    void log_fatal(DecayedFormatString<Args...>&& fmt, Args&&... args) const {
        LOG_MSG(Fatal);
    }

    template <auto& LOCAL_CATEGORY, typename... Args>
    void log_error_to(DecayedFormatString<Args...>&& fmt, Args&&... args) const {
        LOG_MSG_TO(Error);
    }

    template <typename... Args>
    void log_error(DecayedFormatString<Args...>&& fmt, Args&&... args) const {
        LOG_MSG(Error);
    }

    template <auto& LOCAL_CATEGORY, typename... Args>
    void log_warning_to(DecayedFormatString<Args...>&& fmt, Args&&... args) const {
        LOG_MSG_TO(Warning);
    }

    template <typename... Args>
    void log_warning(DecayedFormatString<Args...>&& fmt, Args&&... args) const {
        LOG_MSG(Warning);
    }

    template <auto& LOCAL_CATEGORY, typename... Args>
    void log_display_to(DecayedFormatString<Args...>&& fmt, Args&&... args) const {
        LOG_MSG_TO(Display);
    }

    template <typename... Args>
    void log_display(DecayedFormatString<Args...>&& fmt, Args&&... args) const {
        LOG_MSG(Display);
    }

    template <auto& LOCAL_CATEGORY, typename... Args>
    void log_log_to(DecayedFormatString<Args...>&& fmt, Args&&... args) const {
#if UE_BUILD_DEVELOPMENT
        LOG_MSG_TO(Log);
#endif
    }

    template <typename... Args>
    void log_log(DecayedFormatString<Args...>&& fmt, Args&&... args) const {
#if UE_BUILD_DEVELOPMENT
        LOG_MSG(Log);
#endif
    }

    template <auto& LOCAL_CATEGORY, typename... Args>
    void log_verbose_to(DecayedFormatString<Args...>&& fmt, Args&&... args) const {
#if UE_BUILD_DEVELOPMENT
        LOG_MSG_TO(Verbose);
#endif
    }

    template <typename... Args>
    void log_verbose(DecayedFormatString<Args...>&& fmt, Args&&... args) const {
#if UE_BUILD_DEVELOPMENT
        LOG_MSG(Verbose);
#endif
    }

    template <auto& LOCAL_CATEGORY, typename... Args>
    void log_very_verbose_to(DecayedFormatString<Args...>&& fmt, Args&&... args) const {
#if UE_BUILD_DEVELOPMENT
        LOG_MSG_TO(VeryVerbose);
#endif
    }

    template <typename... Args>
    void log_very_verbose(DecayedFormatString<Args...>&& fmt, Args&&... args) const {
#if UE_BUILD_DEVELOPMENT
        LOG_MSG(VeryVerbose);
#endif
    }
};
}

#undef LOG_MSG
#undef GLOBAL_CATEGORY
#undef LOCAL_CATEGORY
