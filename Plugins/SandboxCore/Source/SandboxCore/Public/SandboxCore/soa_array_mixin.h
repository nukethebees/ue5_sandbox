#pragma once

#include <SandboxCore/array_checks.h>
#include <SandboxCore/array_utils.h>

#include <Containers/AllowShrinking.h>
#include <HAL/Platform.h>

namespace ml {
struct FSoACommonMixin {
    void validate_array_sizes(this auto const& self) {
        self.apply_arrays(
            [](auto const&... arrays) { ml::fatal_if_nums_not_equal({ml::num(arrays)...}); });
    }

    auto num(this auto const& self) noexcept -> int32 {
        return self.apply_arrays([](auto const& first, auto const&...) { return ml::num(first); });
    }
};

struct FSoAArrayMixin : public FSoACommonMixin {
    void reset(this auto& self) {
        self.apply_arrays([](auto&... arrays) { ml::reset(arrays...); });
    }

    void reserve(this auto& self, int32 const count) {
        self.apply_arrays([count](auto&... arrays) { ml::reserve(count, arrays...); });
    }

    void add_uninitialised(this auto& self, int32 const count) {
        self.apply_arrays([count](auto&... arrays) { ml::add_uninitialised(count, arrays...); });
    }

    void add_defaulted(this auto& self, int32 const count) {
        self.apply_arrays([count](auto&... arrays) { ml::add_defaulted(count, arrays...); });
    }

    void remove_at_swap(this auto& self,
                        int32 const index,
                        int32 const count,
                        EAllowShrinking const allow_shrinking) {
        self.apply_arrays([index, count, allow_shrinking](auto&... arrays) {
            ml::remove_at_swap(index, count, allow_shrinking, arrays...);
        });
    }

    void set_num(this auto& self, int32 const count, EAllowShrinking const allow_shrinking) {
        self.apply_arrays([count, allow_shrinking](auto&... arrays) {
            ml::set_num(count, allow_shrinking, arrays...);
        });
    }
};

struct FSoAViewMixin : public FSoACommonMixin {};
}
