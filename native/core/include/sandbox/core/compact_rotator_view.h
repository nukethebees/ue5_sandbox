#pragma once

#include <sandbox/core/compact_vector_view.h>

namespace ml::soa_storage_detail {

template <typename T, template <typename> typename Span, typename Size = std::uint32_t>
class RotatorSoAView {
    using Columns = VectorView<T, 3, Span, Size>;
  public:
    using size_type = Size;
    using View = RotatorSoAView<std::remove_const_t<T>, Span, Size>;
    using ConstView = RotatorSoAView<std::add_const_t<T>, Span, Size>;

    /* **************************************** */
    // Lifetime
    /* **************************************** */
    RotatorSoAView() = default;
    RotatorSoAView(T* first, std::size_t byte_stride, size_type count)
        : columns_{first, byte_stride, count} {}
    template <typename U>
        requires (std::is_const_v<T> && std::is_same_v<U, std::remove_const_t<T>>)
    RotatorSoAView(RotatorSoAView<U, Span, Size> const& other)
        : columns_{other.columns_} {}

    /* **************************************** */
    // Columns
    /* **************************************** */
    auto num() const noexcept -> size_type { return columns_.num(); }
    auto is_empty() const noexcept -> bool { return columns_.is_empty(); }
    auto byte_stride() const noexcept -> std::size_t { return columns_.byte_stride(); }
    auto pitches() const -> Span<T> { return columns_.xs(); }
    auto yaws() const -> Span<T> { return columns_.ys(); }
    auto rolls() const -> Span<T> { return columns_.zs(); }
    template <typename Func>
    auto apply_arrays(Func&& func) const -> decltype(auto) {
        return columns_.apply_arrays(std::forward<Func>(func));
    }
    template <typename Func>
    void each_column(Func&& func) const {
        columns_.each_column(std::forward<Func>(func));
    }

    /* **************************************** */
    // Subviews
    /* **************************************** */
    auto get_view() const -> RotatorSoAView { return *this; }
    auto get_const_view() const -> ConstView { return *this; }
    auto slice(size_type offset, size_type count) const -> RotatorSoAView {
        return RotatorSoAView{columns_.slice(offset, count)};
    }
    auto get_view(size_type offset, size_type count) const -> RotatorSoAView {
        return slice(offset, count);
    }
    auto get_const_view(size_type offset, size_type count) const -> ConstView {
        return slice(offset, count);
    }
    auto left(size_type count) const -> RotatorSoAView {
        return RotatorSoAView{columns_.left(count)};
    }
    auto right(size_type count) const -> RotatorSoAView {
        return RotatorSoAView{columns_.right(count)};
    }
  private:
    template <typename, template <typename> typename, typename>
    friend class RotatorSoAView;
    explicit RotatorSoAView(Columns columns)
        : columns_{columns} {}
    Columns columns_{};
};

} // namespace ml::soa_storage_detail
