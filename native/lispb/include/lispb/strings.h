#pragma once

#include <lispb/indices.h>
#include <lispb/string_pool.h>
#include <lispb/string_slice.h>

#include <cstddef>
#include <cstdint>
#include <string_view>
namespace lispb {
class TemporaryStringScope;

class Strings {
  public:
    [[nodiscard]] auto add(std::string_view const text) -> StringId;

    [[nodiscard]] auto retain(std::string_view const text) -> StringId { return add(text, false); }

    [[nodiscard]] auto slice(StringId const source,
                             std::uint32_t const offset,
                             std::uint32_t const length) -> StringId;

    [[nodiscard]] auto intern(std::string_view const text) -> IdentifierId;

    [[nodiscard]] auto intern_retained(std::string_view const text) -> IdentifierId;

    // An absent spelling returns the empty identifier.
    [[nodiscard]] auto find(std::string_view const text) const -> IdentifierId;

    [[nodiscard]] auto intern(StringId const text) -> IdentifierId;

    [[nodiscard]] auto view(StringId const id) const -> std::string_view;

    [[nodiscard]] auto view(IdentifierId const id) const -> std::string_view;

    // Release temporary handles first. Retained source/document handles remain valid.
    void reset_temporary();

    [[nodiscard]] auto byte_size() const -> std::size_t;
    [[nodiscard]] auto slice_count() const -> std::size_t;
  private:
    friend class TemporaryStringScope;
    inline static constexpr std::uint32_t temporary_bit{std::uint32_t{1} << 31};
    inline static constexpr std::uint32_t index_mask{temporary_bit - 1};

    [[nodiscard]] auto pool(bool const temporary) -> StringPool&;
    [[nodiscard]] auto pool(bool const temporary) const -> StringPool const&;
    [[nodiscard]] auto slice_record(StringId const id) const -> StringSlice;
    [[nodiscard]] auto add_slice(StringSlice const slice, bool const temporary) -> StringId;

    [[nodiscard]] auto add(std::string_view const text, bool const temporary) -> StringId;
    [[nodiscard]] auto intern(std::string_view const text, bool const temporary) -> IdentifierId;
    [[nodiscard]] auto find_interned(StringPool const& storage,
                                     std::size_t const hash,
                                     std::string_view const text) const -> IdentifierId;

    StringPool retained_;
    StringPool transient_;
    bool temporary_{};
};

}
