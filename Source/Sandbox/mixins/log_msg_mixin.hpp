#pragma once

#include <type_traits>
#include <utility>

#include "CoreMinimal.h"
#include "Sandbox/utilities/string_literal_wrapper.h"

namespace ml {
template <StaticTCharString tag>
struct LogMsgMixin {
    static constexpr auto* get_tag() { return tag.data(); }

    template <StaticTCharString inner_tag>
    static consteval auto NestedLogger() {
        using Str = StaticTCharString<3>;
        constexpr Str sep{": "};
        return LogMsgMixin<tag.concatenate_all<tag, sep, inner_tag>()>{};
    }

    // Strip references because the format string requires value types
    template <typename... Args>
    using DecayedFormatString =
        UE::Core::TCheckedFormatString<typename FString::FmtCharType,
                                       std::remove_cv_t<std::remove_reference_t<Args>>...>;

    template <typename... Args>
    void log_fatal(DecayedFormatString<Args...>&& fmt, Args&&... args) const {
        UE_LOG(LogTemp,
               Fatal,
               TEXT("%s: %s"),
               get_tag(),
               *FString::Printf(std::move(fmt), std::forward<Args>(args)...));
    }

    template <typename... Args>
    void log_error(DecayedFormatString<Args...>&& fmt, Args&&... args) const {
        UE_LOG(LogTemp,
               Error,
               TEXT("%s: %s"),
               get_tag(),
               *FString::Printf(std::move(fmt), std::forward<Args>(args)...));
    }

    template <typename... Args>
    void log_warning(DecayedFormatString<Args...>&& fmt, Args&&... args) const {
        UE_LOG(LogTemp,
               Warning,
               TEXT("%s: %s"),
               get_tag(),
               *FString::Printf(std::move(fmt), std::forward<Args>(args)...));
    }

    template <typename... Args>
    void log_display(DecayedFormatString<Args...>&& fmt, Args&&... args) const {
        UE_LOG(LogTemp,
               Display,
               TEXT("%s: %s"),
               get_tag(),
               *FString::Printf(std::move(fmt), std::forward<Args>(args)...));
    }

    template <typename... Args>
    void log_log(DecayedFormatString<Args...>&& fmt, Args&&... args) const {
#if UE_BUILD_DEVELOPMENT
        UE_LOG(LogTemp,
               Log,
               TEXT("%s: %s"),
               get_tag(),
               *FString::Printf(std::move(fmt), std::forward<Args>(args)...));
#endif
    }

    template <typename... Args>
    void log_verbose(DecayedFormatString<Args...>&& fmt, Args&&... args) const {
#if UE_BUILD_DEVELOPMENT
        UE_LOG(LogTemp,
               Verbose,
               TEXT("%s: %s"),
               get_tag(),
               *FString::Printf(std::move(fmt), std::forward<Args>(args)...));
#endif
    }

    template <typename... Args>
    void log_very_verbose(DecayedFormatString<Args...>&& fmt, Args&&... args) const {
#if UE_BUILD_DEVELOPMENT
        UE_LOG(LogTemp,
               VeryVerbose,
               TEXT("%s: %s"),
               get_tag(),
               *FString::Printf(std::move(fmt), std::forward<Args>(args)...));
#endif
    }
};
}
