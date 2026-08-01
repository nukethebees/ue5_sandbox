#pragma once

#include "container_detection_concepts.h"

#include "Containers/AllowShrinking.h"
#include "Containers/ArrayView.h"
#include "HAL/Platform.h"

#include <concepts>
#include <type_traits>

namespace ml {
/* -------------------------------------------------------------------------- */
// Num
/* -------------------------------------------------------------------------- */
template <typename T>
struct NumTraits;

template <details::HasUnrealNum T>
struct NumTraits<T> {
    static auto num(T const& value) noexcept -> int32 { return value.Num(); }
};

template <details::HasNum T>
struct NumTraits<T> {
    static auto num(T const& value) noexcept -> int32 { return value.num(); }
};

template <>
struct NumTraits<int32> {
    static auto num(int32 const value) noexcept -> int32 { return value; }
};

/* -------------------------------------------------------------------------- */
// Reset
/* -------------------------------------------------------------------------- */
template <typename T>
struct ResetTraits;

template <details::HasUnrealReset T>
struct ResetTraits<T> {
    static auto reset(T& value) -> void { value.Reset(); }
};

template <details::HasReset T>
struct ResetTraits<T> {
    static auto reset(T& value) -> void { return value.reset(); }
};

/* -------------------------------------------------------------------------- */
// Reserve
/* -------------------------------------------------------------------------- */
template <typename T>
struct ReserveTraits;

template <details::HasUnrealReserve T>
struct ReserveTraits<T> {
    static auto reserve(T& value, int32 count) -> void { value.Reserve(count); }
};

template <details::HasReserve T>
struct ReserveTraits<T> {
    static auto reserve(T& value, int32 count) -> void { value.reserve(count); }
};

/* -------------------------------------------------------------------------- */
// AddUninitialized
/* -------------------------------------------------------------------------- */
template <typename T>
struct AddUninitialisedTraits;

template <details::HasUnrealAddUninitialised T>
struct AddUninitialisedTraits<T> {
    static auto add_uninitialised(T& value, int32 count) -> void { value.AddUninitialized(count); }
};

template <details::HasAddUninitialised T>
struct AddUninitialisedTraits<T> {
    static auto add_uninitialised(T& value, int32 count) -> void { value.add_uninitialised(count); }
};

/* -------------------------------------------------------------------------- */
// AddDefaulted
/* -------------------------------------------------------------------------- */
template <typename T>
struct AddDefaultedTraits;

template <details::HasUnrealAddDefaulted T>
struct AddDefaultedTraits<T> {
    static auto add_defaulted(T& value, int32 count) -> void { value.AddDefaulted(count); }
};

template <details::HasAddDefaulted T>
struct AddDefaultedTraits<T> {
    static auto add_defaulted(T& value, int32 count) -> void { value.add_defaulted(count); }
};

/* -------------------------------------------------------------------------- */
// RemoveAtSwap
/* -------------------------------------------------------------------------- */
template <typename T>
struct RemoveAtSwapTraits;

template <details::HasUnrealRemoveAtSwap T>
struct RemoveAtSwapTraits<T> {
    static auto remove_at_swap(T& value, int32 index, int32 count, EAllowShrinking as) -> void {
        value.RemoveAtSwap(index, count, as);
    }
};

template <details::HasRemoveAtSwap T>
struct RemoveAtSwapTraits<T> {
    static auto remove_at_swap(T& value, int32 index, int32 count, EAllowShrinking as) -> void {
        value.remove_at_swap(index, count, as);
    }
};

/* -------------------------------------------------------------------------- */
// SetNum
/* -------------------------------------------------------------------------- */
template <typename T>
struct SetNumTraits;

template <details::HasUnrealSetNum T>
struct SetNumTraits<T> {
    static auto set_num(T& value, int32 count, EAllowShrinking const allow_shrinking) -> void {
        value.SetNum(count, allow_shrinking);
    }
};

template <details::HasSetNum T>
struct SetNumTraits<T> {
    static auto set_num(T& value, int32 count, EAllowShrinking const allow_shrinking) -> void {
        value.set_num(count, allow_shrinking);
    }
};

/* -------------------------------------------------------------------------- */
// copy_element
/* -------------------------------------------------------------------------- */
template <typename T>
struct CopyElementTraits;

template <details::HasSubscriptCopy T>
struct CopyElementTraits<T> {
    static void copy_element(T& dst, int32 const dst_i, T const& src, int32 const src_i) {
        dst[dst_i] = src[src_i];
    }
};

template <details::HasCopyElement T>
struct CopyElementTraits<T> {
    static void copy_element(T& dst, int32 const dst_i, T const& src, int32 const src_i) {
        dst.copy_element(dst_i, src, src_i);
    }
};

/* -------------------------------------------------------------------------- */
// get_view
/* -------------------------------------------------------------------------- */
template <typename T>
struct GetViewTraits;

template <details::SupportsUnrealArrayView T>
struct GetViewTraits<T> {
    using Container = std::remove_cvref_t<T>;
    using Element = std::remove_reference_t<decltype(std::declval<Container>()[0])>;
    using ConstElement = std::add_const_t<std::remove_const_t<Element>>;

    static auto get_view(Container& container, int32 const offset, int32 const count) {
        return TArrayView<Element>{container}.Slice(offset, count);
    }
    static auto get_view(Container const& container, int32 const offset, int32 const count) {
        return TArrayView<ConstElement>{container}.Slice(offset, count);
    }
    static auto get_const_view(Container const& container, int32 const offset, int32 const count) {
        return TArrayView<ConstElement>{container}.Slice(offset, count);
    }
};

template <details::HasGetView T>
struct GetViewTraits<T> {
    using Container = std::remove_cvref_t<T>;

    static auto get_view(Container& container, int32 const offset, int32 const count) {
        return container.get_view(offset, count);
    }
    static auto get_view(Container const& container, int32 const offset, int32 const count) {
        return container.get_view(offset, count);
    }
    static auto get_const_view(Container const& container, int32 const offset, int32 const count) {
        return container.get_const_view(offset, count);
    }
};

/* -------------------------------------------------------------------------- */
// append_from
/* -------------------------------------------------------------------------- */
template <typename T>
struct AppendFromTraits;

template <details::HasUnrealAppend T>
struct AppendFromTraits<T> {
    static void append_from(T& dst, T const& src) { dst.Append(src); }
};
}
