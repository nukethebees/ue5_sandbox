#pragma once

#include <cassert>
#include <cstdint>
#include <memory>
#include <vector>

namespace lispb {

template <typename Id>
struct OneBasedIndex {
    using id_type = Id;

    static constexpr auto offset(Id const id) noexcept -> std::uint32_t {
        assert(id.value != 0);
        return id.value - 1;
    }
};

template <typename Id, std::uint32_t InvalidValue>
struct ZeroBasedIndex {
    using id_type = Id;

    static constexpr auto offset(Id const id) noexcept -> std::uint32_t {
        assert(id.value != InvalidValue);
        return id.value;
    }
};

template <typename Id, auto ExpectedKind>
struct TaggedIndex {
    using id_type = Id;

    static constexpr auto offset(Id const id) noexcept -> std::uint32_t {
        assert(id.kind() == ExpectedKind);
        return id.index();
    }
};

// Value-owned vector adapter; never delete through a vector pointer. Typed access requires a
// valid handle of the policy's kind and an existing row, checked by assertions in debug builds.
template <typename T, typename Policy, typename Allocator = std::allocator<T>>
class IndexedColumn final : public std::vector<T, Allocator> {
    using Base = std::vector<T, Allocator>;
    using Id = typename Policy::id_type;
  public:
    using Base::Base;

    using Base::operator=;
    using Base::operator[];

    constexpr auto operator[](Id const id) noexcept -> typename Base::reference {
        auto const index{Policy::offset(id)};
        assert(index < this->size());
        return Base::operator[](index);
    }
    constexpr auto operator[](Id const id) const noexcept -> typename Base::const_reference {
        auto const index{Policy::offset(id)};
        assert(index < this->size());
        return Base::operator[](index);
    }
};

}
