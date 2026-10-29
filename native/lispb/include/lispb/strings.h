#pragma once

#include <lispb/indices.h>
#include <lispb/string_slice.h>

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace lispb {
class TemporaryStringScope;

class Strings {
  public:
    Strings();

    // Equal text has one ID within this store, including slices of source text.
    [[nodiscard]] auto intern(std::string_view text) -> StringId;
    [[nodiscard]] auto intern(StringId text) -> StringId;
    [[nodiscard]] auto intern_retained(std::string_view text) -> StringId;
    [[nodiscard]] auto slice(StringId source, std::uint32_t offset, std::uint32_t length)
        -> StringId;
    // Return the empty ID when the spelling is absent.
    [[nodiscard]] auto find(std::string_view text) const -> StringId;
    // Reacquire views after growing the same character buffer.
    [[nodiscard]] auto view(StringId id) const -> std::string_view;

    // Release temporary handles first. Retained source/document handles remain valid.
    void reset_temporary();
    // Invalidate every handle after all consumers have released their rows.
    void clear();

    [[nodiscard]] auto byte_size() const -> std::size_t;
    [[nodiscard]] auto slice_count() const -> std::size_t;
  private:
    friend class TemporaryStringScope;
    inline static constexpr std::uint32_t temporary_bit{std::uint32_t{1} << 31};
    inline static constexpr std::uint32_t offset_mask{temporary_bit - 1};
    inline static constexpr std::uint32_t unused_length{~std::uint32_t{}};

    [[nodiscard]] auto slice_record(StringId id) const -> StringSlice;
    [[nodiscard]] auto append_text(std::string_view text, bool temporary) -> StringSlice;
    [[nodiscard]] auto intern(std::string_view text, bool temporary, StringSlice source = {})
        -> StringId;
    [[nodiscard]] auto find(std::size_t hash, std::string_view text) const -> StringId;

    std::string retained_bytes_;
    std::string temporary_bytes_;
    std::vector<StringSlice> slices_;
    std::vector<StringId> free_slices_;
    std::unordered_multimap<std::size_t, StringId> interned_;
    bool temporary_{};
};

}
