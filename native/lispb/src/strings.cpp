#include <lispb/strings.h>

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <string_view>

namespace lispb {

Strings::Strings()
    : slices_{{}} {}

auto Strings::intern(std::string_view const text) -> StringId {
    return intern(text, temporary_);
}
auto Strings::intern(StringId const text) -> StringId {
    return intern(view(text), temporary_, slice_record(text));
}
auto Strings::intern_retained(std::string_view const text) -> StringId {
    return intern(text, false);
}
auto Strings::slice(StringId const source, std::uint32_t const offset, std::uint32_t const length)
    -> StringId {
    auto const parent{slice_record(source)};
    assert(offset <= parent.length && length <= parent.length - offset);
    return intern(
        view(source).substr(offset, length), temporary_, {parent.offset + offset, length});
}
auto Strings::find(std::string_view const text) const -> StringId {
    return find(std::hash<std::string_view>{}(text), text);
}
auto Strings::view(StringId const id) const -> std::string_view {
    auto const slice{slice_record(id)};
    auto const& bytes{(slice.offset & temporary_bit) != 0 ? temporary_bytes_ : retained_bytes_};
    return std::string_view{bytes}.substr(slice.offset & offset_mask, slice.length);
}
void Strings::reset_temporary() {
    assert(!temporary_);
    for (auto entry{interned_.begin()}; entry != interned_.end();) {
        auto& slice{slices_[entry->second.value]};
        if ((slice.offset & temporary_bit) != 0) {
            slice = {0, unused_length};
            free_slices_.push_back(entry->second);
            entry = interned_.erase(entry);
        } else {
            ++entry;
        }
    }
    temporary_bytes_.clear();
}
void Strings::clear() {
    assert(!temporary_);
    interned_.clear();
    slices_.resize(1);
    free_slices_.clear();
    retained_bytes_.clear();
    temporary_bytes_.clear();
}
auto Strings::byte_size() const -> std::size_t {
    return retained_bytes_.size() + temporary_bytes_.size();
}
auto Strings::slice_count() const -> std::size_t {
    return slices_.size() - free_slices_.size();
}
auto Strings::slice_record(StringId const id) const -> StringSlice {
    assert(id.value < slices_.size());
    auto const result{slices_[id.value]};
    assert(result.length != unused_length);
    return result;
}
auto Strings::append_text(std::string_view const text, bool const temporary) -> StringSlice {
    auto& bytes{temporary ? temporary_bytes_ : retained_bytes_};
    assert(text.size() < temporary_bit && bytes.size() < temporary_bit - text.size());
    auto const offset{static_cast<std::uint32_t>(bytes.size()) | (temporary ? temporary_bit : 0)};
    auto const length{static_cast<std::uint32_t>(text.size())};
    bytes.append(text.data(), text.size());
    return {offset, length};
}
auto Strings::intern(std::string_view const text, bool const temporary, StringSlice source)
    -> StringId {
    if (text.empty()) {
        return {};
    }
    auto const hash{std::hash<std::string_view>{}(text)};
    if (auto const found{find(hash, text)}; found.value != 0) {
        auto& slice{slices_[found.value]};
        if (!temporary && (slice.offset & temporary_bit) != 0) {
            // Preserve the canonical ID when its text must outlive temporary storage.
            slice = append_text(text, false);
        }
        return found;
    }

    if (source.length == 0 || (!temporary && (source.offset & temporary_bit) != 0)) {
        source = append_text(text, temporary);
    }
    StringId id;
    if (free_slices_.empty()) {
        assert(slices_.size() < std::numeric_limits<std::uint32_t>::max());
        id = StringId{static_cast<std::uint32_t>(slices_.size())};
        slices_.push_back(source);
    } else {
        id = free_slices_.back();
        free_slices_.pop_back();
        slices_[id.value] = source;
    }
    interned_.emplace(hash, id);
    return id;
}
auto Strings::find(std::size_t const hash, std::string_view const text) const -> StringId {
    auto const [begin, end]{interned_.equal_range(hash)};
    for (auto current{begin}; current != end; ++current) {
        if (view(current->second) == text) {
            return current->second;
        }
    }
    return {};
}

}
