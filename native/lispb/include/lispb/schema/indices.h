#pragma once

#include <lispb/index.h>

#include <cstdint>
#include <limits>

namespace lispb::schema {

struct DeclarationTag;
class TypeGraph;
struct EnumDomains;
struct RecordMembers;
struct Relationships;
struct ResolvedOccurrences;
struct SoaColumns;
struct TaggedUnionAlternatives;
struct TypeUseTag;
struct UnionAlternatives;

using DeclarationId = Index<DeclarationTag>;
using EnumDomainId = Index<EnumDomains>;
using RecordMemberId = Index<RecordMembers>;
using RelationshipId = Index<Relationships>;
using ResolvedOccurrenceId = Index<ResolvedOccurrences>;
using SoaColumnId = Index<SoaColumns>;
using TaggedUnionAlternativeId = Index<TaggedUnionAlternatives>;
using TypeId = Index<TypeGraph, std::numeric_limits<std::uint32_t>::max()>;
using TypeUseId = Index<TypeUseTag>;
using UnionAlternativeId = Index<UnionAlternatives>;

[[nodiscard]] constexpr auto is_valid(DeclarationId const id) -> bool {
    return id.value != 0;
}
[[nodiscard]] constexpr auto is_valid(RelationshipId const id) -> bool {
    return id.value != 0;
}
[[nodiscard]] constexpr auto is_valid(ResolvedOccurrenceId const id) -> bool {
    return id.value != 0;
}
[[nodiscard]] constexpr auto is_valid(TypeId const id) -> bool {
    return id.value != std::numeric_limits<std::uint32_t>::max();
}

}
