#pragma once

#include <type_traits>
#include <utility>

#include "CoreMinimal.h"

#include "Sandbox/utilities/string_literal_wrapper.h"

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

    template <auto& log_cat, typename... Args>
    void log_fatal_to(DecayedFormatString<Args...>&& fmt, Args&&... args) const {
        UE_LOG(log_cat,
               Fatal,
               TEXT("%s: %s"),
               get_tag(),
               *FString::Printf(std::move(fmt), std::forward<Args>(args)...));
    }

    template <typename... Args>
    void log_fatal(DecayedFormatString<Args...>&& fmt, Args&&... args) const {
        log_fatal_to<log_category>(std::move(fmt), std::forward<Args>(args)...);
    }

    template <auto& log_cat, typename... Args>
    void log_error_to(DecayedFormatString<Args...>&& fmt, Args&&... args) const {
        UE_LOG(log_cat,
               Error,
               TEXT("%s: %s"),
               get_tag(),
               *FString::Printf(std::move(fmt), std::forward<Args>(args)...));
    }

    template <typename... Args>
    void log_error(DecayedFormatString<Args...>&& fmt, Args&&... args) const {
        log_error_to<log_category>(std::move(fmt), std::forward<Args>(args)...);
    }

    template <auto& log_cat, typename... Args>
    void log_warning_to(DecayedFormatString<Args...>&& fmt, Args&&... args) const {
        UE_LOG(log_cat,
               Warning,
               TEXT("%s: %s"),
               get_tag(),
               *FString::Printf(std::move(fmt), std::forward<Args>(args)...));
    }

    template <typename... Args>
    void log_warning(DecayedFormatString<Args...>&& fmt, Args&&... args) const {
        log_warning_to<log_category>(std::move(fmt), std::forward<Args>(args)...);
    }

    template <auto& log_cat, typename... Args>
    void log_display_to(DecayedFormatString<Args...>&& fmt, Args&&... args) const {
        UE_LOG(log_cat,
               Display,
               TEXT("%s: %s"),
               get_tag(),
               *FString::Printf(std::move(fmt), std::forward<Args>(args)...));
    }

    template <typename... Args>
    void log_display(DecayedFormatString<Args...>&& fmt, Args&&... args) const {
        log_display_to<log_category>(std::move(fmt), std::forward<Args>(args)...);
    }

    template <auto& log_cat, typename... Args>
    void log_log_to(DecayedFormatString<Args...>&& fmt, Args&&... args) const {
#if UE_BUILD_DEVELOPMENT
        UE_LOG(log_cat,
               Log,
               TEXT("%s: %s"),
               get_tag(),
               *FString::Printf(std::move(fmt), std::forward<Args>(args)...));
#endif
    }

    template <typename... Args>
    void log_log(DecayedFormatString<Args...>&& fmt, Args&&... args) const {
        log_log_to<log_category>(std::move(fmt), std::forward<Args>(args)...);
    }

    template <auto& log_cat, typename... Args>
    void log_verbose_to(DecayedFormatString<Args...>&& fmt, Args&&... args) const {
#if UE_BUILD_DEVELOPMENT
        UE_LOG(log_cat,
               Verbose,
               TEXT("%s: %s"),
               get_tag(),
               *FString::Printf(std::move(fmt), std::forward<Args>(args)...));
#endif
    }

    template <typename... Args>
    void log_verbose(DecayedFormatString<Args...>&& fmt, Args&&... args) const {
        log_verbose_to<log_category>(std::move(fmt), std::forward<Args>(args)...);
    }

    template <auto& log_cat, typename... Args>
    void log_very_verbose_to(DecayedFormatString<Args...>&& fmt, Args&&... args) const {
#if UE_BUILD_DEVELOPMENT
        UE_LOG(log_cat,
               VeryVerbose,
               TEXT("%s: %s"),
               get_tag(),
               *FString::Printf(std::move(fmt), std::forward<Args>(args)...));
#endif
    }

    template <typename... Args>
    void log_very_verbose(DecayedFormatString<Args...>&& fmt, Args&&... args) const {
        log_very_verbose_to<log_category>(std::move(fmt), std::forward<Args>(args)...);
    }
};
}
