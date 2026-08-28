#pragma once

#include "Containers/AllowShrinking.h"
#include "CoreMinimal.h"

#include <algorithm>
#include <utility>
#include <vector>

template <typename T>
class TArray {
public:
    using SizeType = int32;

    auto GetData() -> T* { return values_.data(); }
    auto GetData() const -> T const* { return values_.data(); }
    auto Num() const -> SizeType { return static_cast<SizeType>(values_.size()); }
    void Reset() { values_.clear(); }
    void Empty() { values_.clear(); }
    void Reserve(SizeType const count) { values_.reserve(static_cast<std::size_t>(count)); }
    void SetNum(SizeType const count, EAllowShrinking const) {
        values_.resize(static_cast<std::size_t>(count));
    }
    void SetNumUninitialized(SizeType const count) {
        values_.resize(static_cast<std::size_t>(count));
    }
    void AddUninitialized(SizeType const count) {
        values_.resize(values_.size() + static_cast<std::size_t>(count));
    }
    void AddZeroed(SizeType const count) { AddUninitialized(count); }
    void AddDefaulted(SizeType const count) { AddUninitialized(count); }
    void RemoveAtSwap(SizeType const index,
                      SizeType const count,
                      EAllowShrinking const) {
        for (SizeType offset{}; offset < count; ++offset) {
            auto const position{static_cast<std::size_t>(index)};
            values_[position] = std::move(values_.back());
            values_.pop_back();
        }
    }

    auto Add(T const& value) -> SizeType {
        auto const index{Num()};
        values_.push_back(value);
        return index;
    }

    template <typename Other>
    void Append(Other const& other) {
        values_.insert(values_.end(), other.begin(), other.end());
    }

    auto begin() const { return values_.begin(); }
    auto end() const { return values_.end(); }
    auto operator[](SizeType const index) -> T& {
        return values_[static_cast<std::size_t>(index)];
    }
    auto operator[](SizeType const index) const -> T const& {
        return values_[static_cast<std::size_t>(index)];
    }

private:
    std::vector<T> values_;
};
