#pragma once

#include <SandboxCore/array_checks.h>
#include <SandboxCore/array_utils.h>

#include <HAL/Platform.h>

namespace ml {
struct FSoAArrayMixin {
    void validate_array_sizes(this auto const& self) {
        self.apply_arrays(
            [](auto const&... arrays) { ml::fatal_if_nums_not_equal({ml::num(arrays)...}); });
    }

    void reset(this auto& self) {
        self.apply_arrays([](auto&... arrays) { ml::reset(arrays...); });
    }

    auto num(this auto const& self) noexcept -> int32 {
        return self.apply_arrays([](auto const& first, auto const&...) { return ml::num(first); });
    }

    void reserve(this auto& self, int32 const count) {
        self.apply_arrays([count](auto&... arrays) { ml::reserve(count, arrays...); });
    }

    void add_uninitialised(this auto& self, int32 const count) {
        self.apply_arrays([count](auto&... arrays) { ml::add_uninitialised(count, arrays...); });
    }
};
}
