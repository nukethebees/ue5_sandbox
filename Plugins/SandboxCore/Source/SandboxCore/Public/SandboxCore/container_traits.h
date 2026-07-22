#pragma once

#include "Containers/AllowShrinking.h"
#include "Containers/ArrayView.h"
#include "HAL/Platform.h"

#include <concepts>
#include <type_traits>

class UInstancedStaticMeshComponent;

namespace ml {
/* -------------------------------------------------------------------------- */
// Num
/* -------------------------------------------------------------------------- */
template <typename T>
struct NumTraits;

template <typename T>
    requires requires(T const& t) {
        { t.Num() } -> std::convertible_to<int32>;
    }
struct NumTraits<T> {
    static auto num(T const& value) noexcept -> int32 { return value.Num(); }
};

template <typename T>
    requires requires(T const& t) {
        { t.num() } -> std::convertible_to<int32>;
    }
struct NumTraits<T> {
    static auto num(T const& value) noexcept -> int32 { return value.num(); }
};

template <>
struct SANDBOXCORE_API NumTraits<UInstancedStaticMeshComponent> {
    static auto num(UInstancedStaticMeshComponent const& value) -> int32;
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

template <typename T>
    requires requires(T& t) {
        { t.Reset() } -> std::convertible_to<void>;
    }
struct ResetTraits<T> {
    static auto reset(T& value) -> void { value.Reset(); }
};

template <typename T>
    requires requires(T& t) {
        { t.reset() } -> std::convertible_to<void>;
    }
struct ResetTraits<T> {
    static auto reset(T& value) -> void { return value.reset(); }
};

/* -------------------------------------------------------------------------- */
// Reserve
/* -------------------------------------------------------------------------- */
template <typename T>
struct ReserveTraits;

template <typename T>
    requires requires(T& t) {
        { t.Reserve(0) } -> std::convertible_to<void>;
    }
struct ReserveTraits<T> {
    static auto reserve(T& value, int32 count) -> void { value.Reserve(count); }
};

template <typename T>
    requires requires(T& t) {
        { t.reserve(0) } -> std::convertible_to<void>;
    }
struct ReserveTraits<T> {
    static auto reserve(T& value, int32 count) -> void { value.reserve(count); }
};

/* -------------------------------------------------------------------------- */
// AddUninitialized
/* -------------------------------------------------------------------------- */
template <typename T>
struct AddUninitialisedTraits;

template <typename T>
    requires requires(T& t) {
        { t.AddUninitialized(0) } -> std::convertible_to<int32>;
    }
struct AddUninitialisedTraits<T> {
    static auto add_uninitialised(T& value, int32 count) -> void { value.AddUninitialized(count); }
};

template <typename T>
    requires requires(T& t) {
        { t.add_uninitialised(0) } -> std::convertible_to<void>;
    }
struct AddUninitialisedTraits<T> {
    static auto add_uninitialised(T& value, int32 count) -> void { value.add_uninitialised(count); }
};

/* -------------------------------------------------------------------------- */
// AddDefaulted
/* -------------------------------------------------------------------------- */
template <typename T>
struct AddDefaultedTraits;

template <typename T>
    requires requires(T& t) {
        { t.AddDefaulted(0) } -> std::convertible_to<int32>;
    }
struct AddDefaultedTraits<T> {
    static auto add_defaulted(T& value, int32 count) -> void { value.AddDefaulted(count); }
};

template <typename T>
    requires requires(T& t) {
        { t.add_defaulted(0) } -> std::convertible_to<void>;
    }
struct AddDefaultedTraits<T> {
    static auto add_defaulted(T& value, int32 count) -> void { value.add_defaulted(count); }
};

/* -------------------------------------------------------------------------- */
// RemoveAtSwap
/* -------------------------------------------------------------------------- */
template <typename T>
struct RemoveAtSwapTraits;

template <typename T>
    requires requires(T& t) {
        { t.RemoveAtSwap(0, 0, EAllowShrinking::No) } -> std::convertible_to<void>;
    }
struct RemoveAtSwapTraits<T> {
    static auto remove_at_swap(T& value, int32 index, int32 count, EAllowShrinking as) -> void {
        value.RemoveAtSwap(index, count, as);
    }
};

template <typename T>
    requires requires(T& t) {
        { t.remove_at_swap(0, 0, EAllowShrinking::No) } -> std::convertible_to<void>;
    }
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

template <typename T>
    requires requires(T& t) {
        { t.SetNum(0, EAllowShrinking::No) } -> std::convertible_to<void>;
    }
struct SetNumTraits<T> {
    static auto set_num(T& value, int32 count, EAllowShrinking const allow_shrinking) -> void {
        value.SetNum(count, allow_shrinking);
    }
};

template <typename T>
    requires requires(T& t) {
        { t.set_num(0, EAllowShrinking::No) } -> std::convertible_to<void>;
    }
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

template <typename T>
    requires requires(T& t0, T const& t1) {
        { t0[0] } -> std::same_as<typename std::remove_cvref_t<T>::ElementType&>;
        { t1[0] } -> std::same_as<typename std::remove_cvref_t<T>::ElementType const&>;
    }
struct CopyElementTraits<T> {
    static void copy_element(T& dst, int32 const dst_i, T const& src, int32 const src_i) {
        dst[dst_i] = src[src_i];
    }
};

template <typename T>
    requires requires(T& t0, T const& t1) {
        { t0.copy_element(0, t1, 0) } -> std::same_as<void>;
    }
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

template <typename T>
    requires requires(T& t) {
        { TArrayView<std::remove_reference_t<decltype(t[0])>>{t} };
    }
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

template <typename T>
    requires requires(T& t, T const& const_t, int32 const offset, int32 const count) {
        { t.get_view(offset, count) };
        { const_t.get_view(offset, count) };
        { const_t.get_const_view(offset, count) };
    }
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

template <typename T>
    requires requires(T& t0, T const& t1) {
        { t0.Append(t1) } -> std::same_as<void>;
    }
struct AppendFromTraits<T> {
    static void append_from(T& dst, T const& src) { dst.Append(src); }
};
}
