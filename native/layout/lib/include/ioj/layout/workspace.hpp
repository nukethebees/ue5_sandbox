#pragma once

#include <ioj/layout/resolved_variant.hpp>

#include <cstdint>
#include <vector>

namespace lispb::schema {
class TypeGraph;
}

namespace ioj::layout {
class Document;

// Resolved analysis inputs. Durable variant state belongs to Document.
class LayoutWorkspace {
  public:
    LayoutWorkspace();
    explicit LayoutWorkspace(lispb::schema::TypeGraph const& types,
                             std::uint64_t default_capacity = 65'536);
    LayoutWorkspace(lispb::schema::TypeGraph const&&, std::uint64_t = 65'536) = delete;

    auto types() const -> lispb::schema::TypeGraph const&;
    auto default_capacity() const -> std::uint64_t;
    auto element_count() const -> std::uint64_t;
    auto variants() const -> std::vector<ResolvedVariant> const&;
    auto variant(VariantId id) const -> ResolvedVariant const*;
    auto active_variant() const -> ResolvedVariant const&;
    auto active_variant_id() const -> VariantId;
    // Rebind immediately after Document publishes replacement state.
    void bind(Document const& document);
    // id must name a currently resolved variant.
    auto select_variant(VariantId id) -> bool;
    // Analysis populations contain at least one element.
    auto set_element_count(std::uint64_t count) -> bool;
  private:
    lispb::schema::TypeGraph const* types_{};
    std::uint64_t default_capacity_{65'536};
    std::uint64_t element_count_{1};
    std::vector<ResolvedVariant> variants_;
    VariantId active_variant_id_{};
};

}
