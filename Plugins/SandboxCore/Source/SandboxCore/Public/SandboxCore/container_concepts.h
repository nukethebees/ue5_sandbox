#pragma once

#include "container_traits.h"

#include <HAL/Platform.h>

#include <concepts>

namespace ml {
template <typename T>
concept HasNumAndGetData = requires(T const& value) {
    { value.Num() } -> std::convertible_to<int32>;
    { value.GetData() } -> std::convertible_to<void const*>;
};

template <typename T>
concept HasMutableNumAndGetData = requires(T const& value) {
    { value.Num() } -> std::convertible_to<int32>;
    { value.GetData() } -> std::convertible_to<void*>;
};

// num
template <typename T>
concept SupportsNum = requires(T const& value) {
    { NumTraits<T>::num(value) } -> std::same_as<int32>;
};

// reset
template <typename T>
concept SupportsReset = requires(T& value) {
    { ResetTraits<T>::reset(value) } -> std::same_as<void>;
};

// reserve
template <typename T>
concept SupportsReserve = requires(T& value, int32 count) {
    { ReserveTraits<T>::reserve(value, count) } -> std::same_as<void>;
};

// add_uninitialized
template <typename T>
concept SupportsAddUninitialised = requires(T& value, int32 count) {
    { AddUninitialisedTraits<T>::add_uninitialised(value, count) } -> std::same_as<void>;
};

// add_defaulted
template <typename T>
concept SupportsAddDefaulted = requires(T& value, int32 count) {
    { AddDefaultedTraits<T>::add_defaulted(value, count) } -> std::same_as<void>;
};

// remove_at_swap
template <typename T>
concept SupportsRemoveAtSwap =
    requires(T& value, int32 index, int32 count, EAllowShrinking const as) {
        { RemoveAtSwapTraits<T>::remove_at_swap(value, index, count, as) } -> std::same_as<void>;
    };

// set_num
template <typename T>
concept SupportsSetNum = requires(T& value, int32 count, EAllowShrinking const as) {
    { SetNumTraits<T>::set_num(value, count, as) } -> std::same_as<void>;
};

// copy_element
template <typename Dst, typename Src = Dst>
concept SupportsCopyElement =
    requires(Dst& dst, int32 const dst_i, Src const& src, int32 const src_i) {
        {
            CopyElementTraits<std::remove_cvref_t<Dst>, std::remove_cvref_t<Src>>::copy_element(
                dst, dst_i, src, src_i)
        } -> std::same_as<void>;
    };

template <typename T>
consteval bool diagnose_supports_copy_element() {
    if constexpr (!SupportsCopyElement<std::remove_cvref_t<T>>) {
        static_assert(!sizeof(T), "Pack element doesn't satisfy SupportsCopyElement");
    }

    return true;
}

template <typename... T>
concept AllSupportCopyElement = (diagnose_supports_copy_element<std::remove_cvref_t<T>>() && ...);

// copy_elements
template <typename Dst, typename Src = Dst>
concept SupportsCopyElements =
    requires(Dst& dst, int32 const dst_i, Src const& src, int32 const src_i, int32 const count) {
        {
            CopyElementsTraits<std::remove_cvref_t<Dst>, std::remove_cvref_t<Src>>::copy_elements(
                dst, dst_i, src, src_i, count)
        } -> std::same_as<void>;
    };

// get_view
template <typename T>
concept SupportsGetView = requires(T& container, int32 const offset, int32 const count) {
    { GetViewTraits<T>::get_view(container, offset, count) };
    { GetViewTraits<T>::get_const_view(container, offset, count) };
};

// append_from
template <typename Dst, typename Src>
concept SupportsAppendFrom = requires(Dst& dst, Src const& src) {
    { AppendFromTraits<std::remove_cvref_t<Dst>>::append_from(dst, src) } -> std::same_as<void>;
};
}
