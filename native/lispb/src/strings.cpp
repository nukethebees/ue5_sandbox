#include <lispb/strings.h>

#include <lispb/indices.h>
#include <lispb/string_pool.h>
#include <lispb/string_slice.h>

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <string_view>

namespace lispb {

auto Strings::add(std::string_view const text) -> StringId {
    return add(text, temporary_);
}
auto Strings::slice(StringId const source, std::uint32_t const offset, std::uint32_t const length)
    -> StringId {
    auto const parent{slice_record(source)};
    assert(offset <= parent.length && length <= parent.length - offset);
    if (length == 0) {
        return {};
    }
    if (offset == 0 && length == parent.length) {
        return source;
    }
    return add_slice({parent.offset + offset, length}, temporary_);
}
auto Strings::intern(std::string_view const text) -> IdentifierId {
    return intern(text, temporary_);
}
auto Strings::intern_retained(std::string_view const text) -> IdentifierId {
    return intern(text, false);
}
auto Strings::find(std::string_view const text) const -> IdentifierId {
    auto const hash{std::hash<std::string_view>{}(text)};
    auto const retained{find_interned(retained_, hash, text)};
    return retained.value ? retained : find_interned(transient_, hash, text);
}
auto Strings::intern(StringId const text) -> IdentifierId {
    auto const spelling{view(text)};
    auto const hash{std::hash<std::string_view>{}(spelling)};
    auto const retained{find_interned(retained_, hash, spelling)};
    auto const found{retained.value || !temporary_ ? retained
                                                   : find_interned(transient_, hash, spelling)};
    if (found.value != 0 || spelling.empty()) {
        return found;
    }

    auto id{text};
    if (!temporary_ && (text.value & temporary_bit) != 0) {
        auto const record{slice_record(text)};
        id = (record.offset & temporary_bit) != 0 ? retain(spelling) : add_slice(record, false);
    }
    pool(temporary_).identifiers.emplace(hash, id);
    return IdentifierId{id.value};
}
auto Strings::view(StringId const id) const -> std::string_view {
    auto const slice{slice_record(id)};
    return std::string_view{pool((slice.offset & temporary_bit) != 0).bytes}.substr(
        slice.offset & index_mask, slice.length);
}
auto Strings::view(IdentifierId const id) const -> std::string_view {
    return view(string_id(id));
}
void Strings::reset_temporary() {
    assert(!temporary_);
    transient_.bytes.clear();
    transient_.slices.resize(1);
    transient_.identifiers.clear();
}
void Strings::clear() {
    reset_temporary();
    retained_.bytes.clear();
    retained_.slices.resize(1);
    retained_.identifiers.clear();
}
auto Strings::byte_size() const -> std::size_t {
    return retained_.bytes.size() + transient_.bytes.size();
}
auto Strings::slice_count() const -> std::size_t {
    return retained_.slices.size() + transient_.slices.size() - 1;
}
auto Strings::pool(bool const temporary) -> StringPool& {
    return temporary ? transient_ : retained_;
}
auto Strings::pool(bool const temporary) const -> StringPool const& {
    return temporary ? transient_ : retained_;
}
auto Strings::slice_record(StringId const id) const -> StringSlice {
    auto const& slices{pool((id.value & temporary_bit) != 0).slices};
    auto const index{id.value & index_mask};
    assert(index < slices.size());
    return slices[index];
}
auto Strings::add_slice(StringSlice const slice, bool const temporary) -> StringId {
    auto& slices{pool(temporary).slices};
    assert(slices.size() < temporary_bit);
    auto const id{
        StringId{static_cast<std::uint32_t>(slices.size()) | (temporary ? temporary_bit : 0)}};
    slices.push_back(slice);
    return id;
}
auto Strings::add(std::string_view const text, bool const temporary) -> StringId {
    if (text.empty()) {
        return {};
    }
    auto& bytes{pool(temporary).bytes};
    assert(bytes.size() + text.size() < temporary_bit);
    auto const offset{static_cast<std::uint32_t>(bytes.size()) | (temporary ? temporary_bit : 0)};
    auto const length{static_cast<std::uint32_t>(text.size())};
    bytes.append(text.data(), text.size());
    return add_slice({offset, length}, temporary);
}
auto Strings::intern(std::string_view const text, bool const temporary) -> IdentifierId {
    auto const hash{std::hash<std::string_view>{}(text)};
    auto const retained{find_interned(retained_, hash, text)};
    auto const found{retained.value || !temporary ? retained
                                                  : find_interned(transient_, hash, text)};
    if (found.value != 0 || text.empty()) {
        return found;
    }
    auto const id{add(text, temporary)};
    pool(temporary).identifiers.emplace(hash, id);
    return IdentifierId{id.value};
}
auto Strings::find_interned(StringPool const& storage,
                            std::size_t const hash,
                            std::string_view const text) const -> IdentifierId {
    auto const [begin, end]{storage.identifiers.equal_range(hash)};
    for (auto current{begin}; current != end; ++current) {
        if (view(current->second) == text) {
            return IdentifierId{current->second.value};
        }
    }
    return {};
}

}
