#pragma once

#include "container_concepts.h"
#include "container_traits.h"

#include <Containers/AllowShrinking.h>

#include <type_traits>

namespace ml {
template <SupportsNum T>
auto num(T const& value) noexcept -> int32 {
    return NumTraits<T>::num(value);
}

template <SupportsRemoveAtSwap T>
void remove_at_swap(T& array,
                    int32 const index,
                    int32 const count,
                    EAllowShrinking const allow_shrinking) noexcept {
    RemoveAtSwapTraits<T>::remove_at_swap(array, index, count, allow_shrinking);
}

template <SupportsRemoveAtSwap... T>
void remove_at_swap(int32 const index,
                    int32 const count,
                    EAllowShrinking const allow_shrinking,
                    T&... arrays) noexcept {
    (remove_at_swap(arrays, index, count, allow_shrinking), ...);
}

template <SupportsReset... Arrays>
auto reset(Arrays&... arrays) -> void {
    return (ResetTraits<Arrays>::reset(arrays), ...);
}

template <SupportsReserve Array>
auto reserve(Array& array, int32 count) -> void {
    ReserveTraits<Array>::reserve(array, count);
}

template <SupportsReserve... Containers>
auto reserve(int32 count, Containers&... containers) -> void {
    (reserve(containers, count), ...);
}

template <SupportsAddUninitialised Array>
auto add_uninitialised(Array& array, int32 count) -> void {
    AddUninitialisedTraits<Array>::add_uninitialised(array, count);
}

template <SupportsAddUninitialised... Containers>
auto add_uninitialised(int32 count, Containers&... containers) -> void {
    (add_uninitialised(containers, count), ...);
}

template <SupportsAddDefaulted Array>
auto add_defaulted(Array& array, int32 count) -> void {
    AddDefaultedTraits<Array>::add_defaulted(array, count);
}

template <SupportsAddDefaulted... Containers>
auto add_defaulted(int32 count, Containers&... containers) -> void {
    (add_defaulted(containers, count), ...);
}

template <SupportsSetNum Array>
auto set_num(Array& array, int32 count, EAllowShrinking const allow_shrinking) -> void {
    SetNumTraits<Array>::set_num(array, count, allow_shrinking);
}

template <SupportsSetNum... Containers>
auto set_num(int32 count, EAllowShrinking const allow_shrinking, Containers&... containers)
    -> void {
    (set_num(containers, count, allow_shrinking), ...);
}

template <SupportsCopyElement Container>
void copy_element(Container& dst, int32 const dst_i, Container const& src, int32 const src_i) {
    CopyElementTraits<Container>::copy_element(dst, dst_i, src, src_i);
}

template <typename Container, typename... Rest>
    requires (sizeof...(Rest) % 2 == 0) && SupportsCopyElement<std::remove_cvref_t<Container>>
void copy_element(
    int32 const dst_i, int32 const src_i, Container& dst, Container const& src, Rest&&... rest) {
    CopyElementTraits<Container>::copy_element(dst, dst_i, src, src_i);

    if constexpr (sizeof...(rest)) {
        copy_element(dst_i, src_i, rest...);
    }
}

template <SupportsGetView... Containers>
auto get_view(int32 const offset, int32 const count, Containers&... containers) {
    (GetViewTraits<Containers>::get_view(containers, offset, count), ...);
}

template <SupportsGetView... Containers>
auto get_const_view(int32 const offset, int32 const count, Containers&... containers) {
    (GetViewTraits<Containers>::get_const_view(containers, offset, count), ...);
}

template <typename Dst, typename Src, typename... Rest>
    requires (sizeof...(Rest) % 2 == 0) &&
             SupportsAppendFrom<std::remove_cvref_t<Dst>, std::remove_cvref_t<Src>>
void append_from(Dst& dst, Src const& src, Rest&&... rest) {
    AppendFromTraits<std::remove_cvref_t<Dst>>::append_from(dst, src);

    if constexpr (sizeof...(rest)) {
        append_from(rest...);
    }
}
}
