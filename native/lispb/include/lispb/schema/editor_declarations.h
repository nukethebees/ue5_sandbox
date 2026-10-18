#pragma once

#include <lispb/schema/indices.h>
#include <lispb/schema/source_range.h>
#include <lispb/schema/type_identity.h>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace lispb::schema {

class EditorDeclarations {
  public:
    auto allocate() -> DeclarationId;
    void reserve(DeclarationId id);
    void activate(DeclarationId id);
    void remove(DeclarationId id);
    void clear();
    auto contains(DeclarationId id) const -> bool;
    auto find(DeclarationId id) const -> std::optional<DeclarationId>;
    auto index(DeclarationId id) const -> std::uint32_t;

    std::vector<DeclarationId> ordered;
    std::vector<TypeIdentity> identity;
    std::vector<std::size_t> module_index;
    std::vector<std::size_t> declaration_index;
    std::vector<std::optional<SourceRange>> source;
  private:
    std::vector<std::uint8_t> active_;
};

}
