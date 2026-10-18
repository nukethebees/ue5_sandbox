#include <ioj/levels/authoring/identifiers.h>

#include <ioj/ascii.h>

#include <algorithm>
#include <cassert>

namespace ioj::levels::authoring {
auto is_canonical_symbol(std::string_view const value) -> bool {
    return !value.empty() && is_ascii_lower(value.front()) &&
           std::ranges::all_of(value, [](char const c) {
               return is_ascii_lower(c) || is_ascii_digit(c) || c == '-';
           });
}

auto canonical_symbol(std::string_view const text, std::string_view const fallback) -> std::string {
    assert(is_canonical_symbol(fallback));
    std::string result;
    for (auto const byte : text) {
        auto const c{to_ascii_lower(byte)};
        if (is_ascii_lower(c) || is_ascii_digit(c)) {
            result.push_back(c);
        } else if (!result.empty() && result.back() != '-') {
            result.push_back('-');
        }
    }
    if (result.ends_with('-')) {
        result.pop_back();
    }
    if (result.empty()) {
        return std::string{fallback};
    }
    if (!is_ascii_lower(result.front())) {
        result = std::string{fallback} + '-' + result;
    }
    return result;
}

auto allocate_entity_id(std::string_view const text,
                        std::string_view const fallback,
                        std::unordered_set<EntityId>& used_ids) -> EntityId {
    auto const base{canonical_symbol(text, fallback)};
    EntityId id{base};
    for (unsigned suffix{2}; !used_ids.insert(id).second; ++suffix) {
        id = EntityId{base + '-' + std::to_string(suffix)};
    }
    return id;
}
}
