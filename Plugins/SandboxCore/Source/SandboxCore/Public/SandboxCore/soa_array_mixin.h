#pragma once

#include <SandboxCore/array_checks.h>
#include <SandboxCore/container_ops.h>

#include <Containers/AllowShrinking.h>
#include <HAL/Platform.h>

#include <type_traits>

namespace ml {
struct FApplyArraysProbe {
    template <typename... T>
    void operator()(T&&...) const {}
};

struct FApplyArrayPairsProbe {
    template <typename... T>
    void operator()(T&&...) const {}
};

template <typename T>
concept SupportsApplyArrays =
    requires(std::remove_cvref_t<T>& value, std::remove_cvref_t<T> const& const_value) {
        value.apply_arrays(FApplyArraysProbe{});
        const_value.apply_arrays(FApplyArraysProbe{});
    };

template <typename T>
concept SupportsApplyArrayPairs =
    requires(std::remove_cvref_t<T>& value, std::remove_cvref_t<T> const& const_value) {
        value.apply_array_pairs(const_value, FApplyArrayPairsProbe{});
    };

template <typename T, typename Other>
concept SupportsApplyArrayPairsWith =
    requires(std::remove_cvref_t<T>& value, std::remove_cvref_t<Other> const& other) {
        value.apply_array_pairs(other, FApplyArrayPairsProbe{});
    };

struct FSoACommonMixin {
    template <typename T>
    using ViewFor = std::conditional_t<std::is_const_v<T>,
                                       typename std::remove_cv_t<T>::ConstView,
                                       typename std::remove_cv_t<T>::View>;

    template <typename T, SupportsApplyArrays Self>
    auto construct_object(this Self&& self, int32 const offset, int32 const count) -> T {
        return self.apply_arrays([offset, count](auto&... arrays) -> T {
            return T{
                {},
                GetViewTraits<std::remove_cvref_t<decltype(arrays)>>::get_view(
                    arrays, offset, count)...,
            };
        });
    }

    template <SupportsApplyArrays Self>
    auto get_view(this Self& self) {
        return self.template construct_object<ViewFor<Self>>(0, self.num());
    }

    template <SupportsApplyArrays Self>
    auto get_view(this Self& self, int32 const offset, int32 const count) {
        return self.template construct_object<ViewFor<Self>>(offset, count);
    }

    template <SupportsApplyArrays Self>
    auto get_const_view(this Self const& self) {
        return self.template construct_object<typename Self::ConstView>(0, self.num());
    }

    template <SupportsApplyArrays Self>
    auto get_const_view(this Self const& self, int32 const offset, int32 const count) {
        return self.template construct_object<typename Self::ConstView>(offset, count);
    }

    template <SupportsApplyArrays Self>
    void validate_array_sizes(this Self const& self) {
        self.apply_arrays(
            [](auto const&... arrays) { ml::fatal_if_nums_not_equal({ml::num(arrays)...}); });
    }

    template <SupportsApplyArrays Self>
    auto num(this Self const& self) noexcept -> int32 {
        return self.apply_arrays([](auto const& first, auto const&...) { return ml::num(first); });
    }

    template <SupportsApplyArrays Self>
    auto right(this Self& self, int32 const n_right) noexcept {
        auto const n_total{self.num()};
        auto const offset{n_total - n_right};
        return self.template construct_object<ViewFor<Self>>(offset, n_right);
    }
};

struct FSoAArrayMixin : public FSoACommonMixin {
    template <SupportsApplyArrays Self>
    void reset(this Self& self) {
        self.apply_arrays([](auto&... arrays) { ml::reset(arrays...); });
    }

    template <SupportsApplyArrays Self>
    void reserve(this Self& self, int32 const count) {
        self.apply_arrays([count](auto&... arrays) { ml::reserve(count, arrays...); });
    }

    template <SupportsApplyArrays Self>
    void add_uninitialised(this Self& self, int32 const count) {
        self.apply_arrays([count](auto&... arrays) { ml::add_uninitialised(count, arrays...); });
    }

    template <SupportsApplyArrays Self>
    void add_defaulted(this Self& self, int32 const count) {
        self.apply_arrays([count](auto&... arrays) { ml::add_defaulted(count, arrays...); });
    }

    template <SupportsApplyArrays Self>
    void remove_at_swap(this Self& self,
                        int32 const index,
                        int32 const count,
                        EAllowShrinking const allow_shrinking) {
        self.apply_arrays([index, count, allow_shrinking](auto&... arrays) {
            ml::remove_at_swap(index, count, allow_shrinking, arrays...);
        });
    }

    template <SupportsApplyArrays Self>
    void set_num(this Self& self, int32 const count, EAllowShrinking const allow_shrinking) {
        self.apply_arrays([count, allow_shrinking](auto&... arrays) {
            ml::set_num(count, allow_shrinking, arrays...);
        });
    }

    template <SupportsApplyArrayPairs Self>
    void copy_element(this Self& self, int32 const dst_i, Self const& other, int32 const src_i) {
        self.apply_array_pairs(other, [dst_i, src_i](auto&&... arrays) -> void {
            ml::copy_element(dst_i, src_i, arrays...);
        });
    }

    template <typename Other, typename Self>
        requires SupportsApplyArrayPairsWith<Self, Other>
    void append_from(this Self& self, Other const& other) {
        self.apply_array_pairs(other, [](auto&&... arrays) -> void { ml::append_from(arrays...); });
    }
};

struct FSoAViewMixin : public FSoACommonMixin {};
}
