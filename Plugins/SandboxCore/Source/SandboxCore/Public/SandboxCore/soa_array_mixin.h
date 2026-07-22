#pragma once

#include <SandboxCore/array_checks.h>
#include <SandboxCore/array_utils.h>

#include <Containers/AllowShrinking.h>
#include <HAL/Platform.h>

#include <type_traits>

namespace ml {
struct FSoACommonMixin {
    template <typename T>
    auto construct_object(this auto&& self, int32 const offset, int32 const count) -> T {
        return self.apply_arrays([offset, count](auto&... arrays) -> T {
            return T{
                {},
                GetViewTraits<std::remove_cvref_t<decltype(arrays)>>::get_view(
                    arrays, offset, count)...,
            };
        });
    }

    auto get_view(this auto& self) {
        using View = typename std::remove_cvref_t<decltype(self)>::View;
        return self.template construct_object<View>(0, self.num());
    }

    auto get_view(this auto& self, int32 const offset, int32 const count) {
        using View = typename std::remove_cvref_t<decltype(self)>::View;
        return self.template construct_object<View>(offset, count);
    }

    auto get_view(this auto const& self) {
        using ConstView = typename std::remove_cvref_t<decltype(self)>::ConstView;
        return self.template construct_object<ConstView>(0, self.num());
    }

    auto get_view(this auto const& self, int32 const offset, int32 const count) {
        using ConstView = typename std::remove_cvref_t<decltype(self)>::ConstView;
        return self.template construct_object<ConstView>(offset, count);
    }

    auto get_const_view(this auto const& self) {
        using ConstView = typename std::remove_cvref_t<decltype(self)>::ConstView;
        return self.template construct_object<ConstView>(0, self.num());
    }

    auto get_const_view(this auto const& self, int32 const offset, int32 const count) {
        using ConstView = typename std::remove_cvref_t<decltype(self)>::ConstView;
        return self.template construct_object<ConstView>(offset, count);
    }

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

    template <typename Self>
    void copy_element(this Self& self, int32 const dst_i, Self const& other, int32 const src_i) {
        self.apply_array_pairs(other, [dst_i, src_i](auto&&... arrays) -> void {
            ml::copy_element(dst_i, src_i, arrays...);
        });
    }

    template <typename Self>
    void append_from(this Self& self, Self const& other) {
        self.apply_array_pairs(other, [](auto&&... arrays) -> void { ml::append_from(arrays...); });
    }
};

#define SANDBOX_SOA_MIXIN_APPLY_ARRAYS(MEMBER_NAME) self.MEMBER_NAME
#define SANDBOX_SOA_MIXIN_APPLY_ARRAYS_PAIRS(MEMBER_NAME) self.MEMBER_NAME, other.MEMBER_NAME

#define SANDBOX_SOA_MIXIN_MEMBER_TYPE_ALIAS(MEMBER_NAME) \
    using member_type_##MEMBER_NAME = decltype(MEMBER_NAME);
#define SANDBOX_SOA_MIXIN_VIEW_MEMBER(MEMBER_NAME) member_type_##MEMBER_NAME MEMBER_NAME;

/* SANDBOX_SOA_MEMBERS is a macro in the form

#define SANDBOX_PACK(STAMPER)  \
    STAMPER(entity_handles)    \
    , STAMPER(tasks)           \
    , ...

*/

#define SANDBOX_COMMA ,
#define SANDBOX_SEMICOLON ;
#define SANDBOX_EMPTY
#define SANDBOX_SOA_MAKE_APPLY_FNS(SANDBOX_SOA_MEMBERS)                                       \
    template <typename TFunc>                                                                 \
    auto apply_arrays(this auto&& self, TFunc&& func) -> decltype(auto) {                     \
        return std::forward<TFunc>(func)(                                                     \
            SANDBOX_SOA_MEMBERS(SANDBOX_SOA_MIXIN_APPLY_ARRAYS, SANDBOX_COMMA));              \
    }                                                                                         \
    template <typename Self, typename Other, typename TFunc>                                  \
        requires std::is_same_v<std::remove_cvref_t<Self>, std::remove_cvref_t<Other>>        \
    auto apply_array_pairs(this Self&& self, Other&& other, TFunc&& func) -> decltype(auto) { \
        return std::forward<TFunc>(func)(                                                     \
            SANDBOX_SOA_MEMBERS(SANDBOX_SOA_MIXIN_APPLY_ARRAYS_PAIRS, SANDBOX_COMMA));        \
    }

struct FSoAViewMixin : public FSoACommonMixin {};
}
