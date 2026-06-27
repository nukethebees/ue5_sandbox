#pragma once

#include <SandboxCore/container_traits.h>

#include <HAL/Platform.h>

#include <initializer_list>
#include <type_traits>

namespace ml {

struct NamedNum {
    int32 num{};
    TCHAR const* name{};
};

#define SANDBOX_NAMED_NUM(value)                                                      \
    ml::NamedNum {                                                                    \
        ml::NumTraits<std::remove_cvref_t<decltype(value)>>::num(value), TEXT(#value) \
    }

void SANDBOXCORE_API fatal_if_nums_not_equal(std::initializer_list<NamedNum> const values);
void SANDBOXCORE_API fatal_if_nums_not_equal(std::initializer_list<int32> const values);
}
