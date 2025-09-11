#pragma once

#include <utility>

#include "CoreMinimal.h"

namespace ml {
template <wchar_t const* tag>
struct LogMsgMixin {
    static constexpr wchar_t const* get_tag() { return tag; }

    template <typename... Args>
    void log_warning(UE::Core::TCheckedFormatString<typename FString::FmtCharType, Args...>&& fmt,
                     Args&&... args) const {
        UE_LOG(LogTemp,
               Warning,
               TEXT("%s: %s"),
               tag,
               *FString::Printf(std::move(fmt), std::forward<Args>(args)...));
    }
    template <typename... Args>
    void log_info(UE::Core::TCheckedFormatString<typename FString::FmtCharType, Args...>&& fmt,
                  Args&&... args) const {
        UE_LOG(LogTemp,
               Log,
               TEXT("%s: %s"),
               tag,
               *FString::Printf(std::move(fmt), std::forward<Args>(args)...));
    }
};
}
