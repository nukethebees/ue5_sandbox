#pragma once

#include "container_traits.h"

#include "HAL/Platform.h"

#include <initializer_list>
#include <type_traits>

namespace ioj {
struct NamedNum {
    int32 num{};
    TCHAR const* name{};
};

#define SANDBOX_NAMED_NUM(value)                                                       \
    ioj::NamedNum {                                                                    \
        ioj::NumTraits<std::remove_cvref_t<decltype(value)>>::num(value), TEXT(#value) \
    }

void SANDBOXCORE_API fatal_if_nums_not_equal(std::initializer_list<NamedNum> const values);
void SANDBOXCORE_API fatal_if_nums_not_equal(std::initializer_list<int32> const values);

}
