#pragma once

#include "container_concepts.h"
#include "soa_vector_concepts.h"

#include "Containers/StaticArray.h"
#include "HAL/Platform.h"

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

namespace detail {
template <typename T>
concept ArrayCheckInput = HasNumAndGetData<T> || is_readable_vec3f<T>;

template <ArrayCheckInput T>
consteval auto array_check_pointer_count() -> int32 {
    if constexpr (is_readable_vec3f<T>) {
        return 3;
    } else {
        return 1;
    }
}
}

template <detail::ArrayCheckInput... Inputs>
    requires((detail::array_check_pointer_count<Inputs>() + ...) >= 2)
[[nodiscard]] auto all_num_equal_and_pointers_not_equal(Inputs const&... inputs) -> bool {
    constexpr int32 pointer_capacity{(detail::array_check_pointer_count<Inputs>() + ...)};
    TStaticArray<void const*, pointer_capacity> pointers{};
    int32 pointer_count{0};
    int32 expected_num{0};
    bool has_expected_num{false};

    auto const validate_container = [&](auto const& container) {
        auto const count{container.Num()};
        if (!has_expected_num) {
            expected_num = count;
            has_expected_num = true;
        } else if (count != expected_num) {
            return false;
        }

        if (count == 0) {
            return true;
        }

        void const* const pointer{container.GetData()};
        for (int32 i{0}; i < pointer_count; ++i) {
            if (pointers[i] == pointer) {
                return false;
            }
        }

        pointers[pointer_count++] = pointer;
        return true;
    };

    auto const validate_input = [&](auto const& input) {
        using Input = std::remove_cvref_t<decltype(input)>;
        if constexpr (is_readable_vec3f<Input>) {
            return validate_container(input.xs) && validate_container(input.ys) &&
                   validate_container(input.zs);
        } else {
            return validate_container(input);
        }
    };

    return (validate_input(inputs) && ...);
}
}
