#pragma once

#include <ioj/levels/identifiers.h>

#include <string_view>
#include <unordered_set>

namespace ioj::levels::authoring {
[[nodiscard]] auto is_canonical_symbol(std::string_view value) -> bool;
// The fallback must already be canonical. Non-ASCII bytes are separators, not letters.
[[nodiscard]] auto canonical_symbol(std::string_view text, std::string_view fallback)
    -> std::string;
// Reserve the result in used_ids, choosing the first available suffix starting at -2.
[[nodiscard]] auto allocate_entity_id(std::string_view text,
                                      std::string_view fallback,
                                      std::unordered_set<EntityId>& used_ids) -> EntityId;
}
