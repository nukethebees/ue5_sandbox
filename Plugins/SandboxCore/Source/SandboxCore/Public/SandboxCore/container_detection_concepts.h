#pragma once

#include <Containers/AllowShrinking.h>
#include <Containers/ArrayView.h>
#include <HAL/Platform.h>

#include <concepts>
#include <type_traits>

namespace ml::details {
template <typename T>
concept HasUnrealNum = requires(T const& value) {
    { value.Num() } -> std::convertible_to<int32>;
};

template <typename T>
concept HasNum = requires(T const& value) {
    { value.num() } -> std::convertible_to<int32>;
};

template <typename T>
concept HasUnrealReset = requires(T& value) {
    { value.Reset() } -> std::convertible_to<void>;
};

template <typename T>
concept HasReset = requires(T& value) {
    { value.reset() } -> std::convertible_to<void>;
};

template <typename T>
concept HasUnrealReserve = requires(T& value) {
    { value.Reserve(0) } -> std::convertible_to<void>;
};

template <typename T>
concept HasReserve = requires(T& value) {
    { value.reserve(0) } -> std::convertible_to<void>;
};

template <typename T>
concept HasUnrealAddUninitialised = requires(T& value) {
    { value.AddUninitialized(0) } -> std::convertible_to<int32>;
};

template <typename T>
concept HasAddUninitialised = requires(T& value) {
    { value.add_uninitialised(0) } -> std::convertible_to<void>;
};

template <typename T>
concept HasUnrealAddDefaulted = requires(T& value) {
    { value.AddDefaulted(0) } -> std::convertible_to<int32>;
};

template <typename T>
concept HasAddDefaulted = requires(T& value) {
    { value.add_defaulted(0) } -> std::convertible_to<void>;
};

template <typename T>
concept HasUnrealRemoveAtSwap = requires(T& value) {
    { value.RemoveAtSwap(0, 0, EAllowShrinking::No) } -> std::convertible_to<void>;
};

template <typename T>
concept HasRemoveAtSwap = requires(T& value) {
    { value.remove_at_swap(0, 0, EAllowShrinking::No) } -> std::convertible_to<void>;
};

template <typename T>
concept HasUnrealSetNum = requires(T& value) {
    { value.SetNum(0, EAllowShrinking::No) } -> std::convertible_to<void>;
};

template <typename T>
concept HasSetNum = requires(T& value) {
    { value.set_num(0, EAllowShrinking::No) } -> std::convertible_to<void>;
};

template <typename Dst, typename Src>
concept HasSubscriptCopy = requires(Dst& dst, Src const& src) {
    dst[0] = src[0];
};

template <typename Dst, typename Src>
concept HasCopyElement = requires(Dst& dst, Src const& src) {
    { dst.copy_element(0, src, 0) } -> std::same_as<void>;
};

template <typename Dst, typename Src>
concept HasCopyElements = requires(Dst& dst, Src const& src) {
    { dst.copy_elements(0, src, 0, 0) } -> std::same_as<void>;
};

template <typename T>
concept SupportsUnrealArrayView = requires(T& value) {
    { TArrayView<std::remove_reference_t<decltype(value[0])>>{value} };
};

template <typename T>
concept HasGetView =
    requires(T& value, T const& const_value, int32 const offset, int32 const count) {
        { value.get_view(offset, count) };
        { const_value.get_view(offset, count) };
        { const_value.get_const_view(offset, count) };
    };

template <typename T>
concept HasUnrealAppend = requires(T& dst, T const& src) {
    { dst.Append(src) } -> std::same_as<void>;
};
}
