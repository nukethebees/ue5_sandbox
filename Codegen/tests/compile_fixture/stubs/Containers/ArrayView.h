#pragma once

#include "CoreMinimal.h"

#include <algorithm>
#include <concepts>
#include <type_traits>

template <typename T>
class TArrayView {
  public:
    using SizeType = int32;

    TArrayView() = default;
    TArrayView(T* data, SizeType const count)
        : data_{data}
        , count_{count} {}

    template <typename Container>
        requires requires(Container& container) {
            { container.GetData() } -> std::convertible_to<T*>;
            { container.Num() } -> std::convertible_to<SizeType>;
        }
    TArrayView(Container& container)
        : data_{container.GetData()}
        , count_{container.Num()} {}

    auto GetData() const -> T* { return data_; }
    auto Num() const -> SizeType { return count_; }
    auto Slice(SizeType const offset, SizeType const count) const -> TArrayView {
        return TArrayView{data_ + offset, count};
    }
    auto Left(SizeType const count) const -> TArrayView { return Slice(0, count); }
    auto Right(SizeType const count) const -> TArrayView { return Slice(count_ - count, count); }

    template <typename Compare>
    void Sort(Compare&& compare) {
        std::sort(data_, data_ + count_, std::forward<Compare>(compare));
    }

    auto operator[](SizeType const index) const -> T& { return data_[index]; }
    auto begin() const -> T* { return data_; }
    auto end() const -> T* { return data_ + count_; }
  private:
    T* data_{};
    SizeType count_{};
};

template <typename T>
using TConstArrayView = TArrayView<T const>;
