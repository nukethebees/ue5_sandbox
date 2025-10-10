#pragma once

#include <type_traits>
#include <utility>

#include "CoreMinimal.h"

#include "Sandbox/utilities/string_literal_wrapper.h"

#define LOG_MSG_TO(VERBOSITY, LOG_CATEGORY) \
    log_to<ELogVerbosity::VERBOSITY, LOG_CATEGORY>(std::move(fmt), std::forward<Args>(args)...);

#define LOG_MSG(VERBOSITY) \
    log_to<ELogVerbosity::VERBOSITY, log_category>(std::move(fmt), std::forward<Args>(args)...);

namespace ml {
template <StaticTCharString tag, auto& log_category_ = LogTemp>
struct LogMsgMixin {
    static constexpr auto* get_tag() { return tag.data(); }
    static constexpr auto& log_category{log_category_};

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

    template <ELogVerbosity::Type verbosity, auto& log_cat, typename... Args>
        requires (verbosity < ELogVerbosity::NumVerbosity)
    void log_to(DecayedFormatString<Args...>&& fmt, Args&&... args) const {
#define LOG_BRANCH(VERBOSITY)                                                 \
    else if constexpr (verbosity == ELogVerbosity::VERBOSITY) {               \
        UE_LOG(log_cat,                                                       \
               VERBOSITY,                                                     \
               TEXT("%s: %s"),                                                \
               get_tag(),                                                     \
               *FString::Printf(std::move(fmt), std::forward<Args>(args)...)) \
    }

        if constexpr (verbosity == ELogVerbosity::Fatal) {
            LOG_MSG(Fatal);
        }
        LOG_BRANCH(Error)
        LOG_BRANCH(Warning)
        LOG_BRANCH(Log)
        LOG_BRANCH(Display)
        LOG_BRANCH(Verbose)
        LOG_BRANCH(VeryVerbose)
#undef LOG_BRANCH
    }

    template <ELogVerbosity::Type verbosity, auto& log_cat, typename... Args>
        requires (verbosity < ELogVerbosity::NumVerbosity)
    void log(DecayedFormatString<Args...>&& fmt, Args&&... args) const {
        log_to<verbosity, log_cat>(std::move(fmt), std::forward<Args>(args)...);
    }

    template <auto& log_cat, typename... Args>
    void log_fatal_to(DecayedFormatString<Args...>&& fmt, Args&&... args) const {
        LOG_MSG_TO(Fatal, log_cat);
    }

    template <typename... Args>
    void log_fatal(DecayedFormatString<Args...>&& fmt, Args&&... args) const {
        LOG_MSG(Fatal);
    }

    template <auto& log_cat, typename... Args>
    void log_error_to(DecayedFormatString<Args...>&& fmt, Args&&... args) const {
        LOG_MSG_TO(Error, log_cat);
    }

    template <typename... Args>
    void log_error(DecayedFormatString<Args...>&& fmt, Args&&... args) const {
        LOG_MSG(Error);
    }

    template <auto& log_cat, typename... Args>
    void log_warning_to(DecayedFormatString<Args...>&& fmt, Args&&... args) const {
        LOG_MSG_TO(Warning, log_cat);
    }

    template <typename... Args>
    void log_warning(DecayedFormatString<Args...>&& fmt, Args&&... args) const {
        LOG_MSG(Warning);
    }

    template <auto& log_cat, typename... Args>
    void log_display_to(DecayedFormatString<Args...>&& fmt, Args&&... args) const {
        LOG_MSG_TO(Display, log_cat);
    }

    template <typename... Args>
    void log_display(DecayedFormatString<Args...>&& fmt, Args&&... args) const {
        LOG_MSG(Display);
    }

    template <auto& log_cat, typename... Args>
    void log_log_to(DecayedFormatString<Args...>&& fmt, Args&&... args) const {
#if UE_BUILD_DEVELOPMENT
        LOG_MSG_TO(Log, log_cat);
#endif
    }

    template <typename... Args>
    void log_log(DecayedFormatString<Args...>&& fmt, Args&&... args) const {
        LOG_MSG(Log);
    }

    template <auto& log_cat, typename... Args>
    void log_verbose_to(DecayedFormatString<Args...>&& fmt, Args&&... args) const {
#if UE_BUILD_DEVELOPMENT
        LOG_MSG_TO(Verbose, log_cat);
#endif
    }

    template <typename... Args>
    void log_verbose(DecayedFormatString<Args...>&& fmt, Args&&... args) const {
        LOG_MSG(Verbose);
    }

    template <auto& log_cat, typename... Args>
    void log_very_verbose_to(DecayedFormatString<Args...>&& fmt, Args&&... args) const {
#if UE_BUILD_DEVELOPMENT
        LOG_MSG_TO(VeryVerbose, log_cat);
#endif
    }

    template <typename... Args>
    void log_very_verbose(DecayedFormatString<Args...>&& fmt, Args&&... args) const {
        LOG_MSG(VeryVerbose);
    }
};
}

#undef LOG_MSG
