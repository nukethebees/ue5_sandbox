#pragma once

#include <utility>

#include "CoreMinimal.h"

namespace ml {
template <wchar_t const* tag>
struct LogMsgMixin {
    static constexpr wchar_t const* get_tag() { return tag; }

    // Fatal (always active, crashes the app)
    template <typename... Args>
    void log_fatal(UE::Core::TCheckedFormatString<typename FString::FmtCharType, Args...>&& fmt,
                   Args&&... args) const {
        UE_LOG(LogTemp,
               Fatal,
               TEXT("%s: %s"),
               tag,
               *FString::Printf(std::move(fmt), std::forward<Args>(args)...));
    }

    // Error (always active)
    template <typename... Args>
    void log_error(UE::Core::TCheckedFormatString<typename FString::FmtCharType, Args...>&& fmt,
                   Args&&... args) const {
        UE_LOG(LogTemp,
               Error,
               TEXT("%s: %s"),
               tag,
               *FString::Printf(std::move(fmt), std::forward<Args>(args)...));
    }

    // Warning (always active)
    template <typename... Args>
    void log_warning(UE::Core::TCheckedFormatString<typename FString::FmtCharType, Args...>&& fmt,
                     Args&&... args) const {
        UE_LOG(LogTemp,
               Warning,
               TEXT("%s: %s"),
               tag,
               *FString::Printf(std::move(fmt), std::forward<Args>(args)...));
    }

    // Display (always active)
    template <typename... Args>
    void log_display(UE::Core::TCheckedFormatString<typename FString::FmtCharType, Args...>&& fmt,
                     Args&&... args) const {
        UE_LOG(LogTemp,
               Display,
               TEXT("%s: %s"),
               tag,
               *FString::Printf(std::move(fmt), std::forward<Args>(args)...));
    }

    // Log (dev-only)
    template <typename... Args>
    void log_log(UE::Core::TCheckedFormatString<typename FString::FmtCharType, Args...>&& fmt,
                 Args&&... args) const {
#if UE_BUILD_DEVELOPMENT
        UE_LOG(LogTemp,
               Log,
               TEXT("%s: %s"),
               tag,
               *FString::Printf(std::move(fmt), std::forward<Args>(args)...));
#endif
    }

    // Verbose (dev-only)
    template <typename... Args>
    void log_verbose(UE::Core::TCheckedFormatString<typename FString::FmtCharType, Args...>&& fmt,
                     Args&&... args) const {
#if UE_BUILD_DEVELOPMENT
        UE_LOG(LogTemp,
               Verbose,
               TEXT("%s: %s"),
               tag,
               *FString::Printf(std::move(fmt), std::forward<Args>(args)...));
#endif
    }

    // VeryVerbose (dev-only)
    template <typename... Args>
    void log_very_verbose(
        UE::Core::TCheckedFormatString<typename FString::FmtCharType, Args...>&& fmt,
        Args&&... args) const {
#if UE_BUILD_DEVELOPMENT
        UE_LOG(LogTemp,
               VeryVerbose,
               TEXT("%s: %s"),
               tag,
               *FString::Printf(std::move(fmt), std::forward<Args>(args)...));
#endif
    }
};
}
