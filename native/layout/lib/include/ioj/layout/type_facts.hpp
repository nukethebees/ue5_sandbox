#pragma once

#include <ioj/layout/fact_origin.hpp>

#include <cstdint>
#include <expected>
#include <optional>
#include <string>
#include <utility>

namespace ioj::layout {

class TypeFacts {
  public:
    struct Definition {
        auto operator==(Definition const&) const -> bool = default;

        // Complete-object sizeof, including tail padding; also the stride in an array.
        std::uint64_t size_bytes{};
        std::uint64_t alignment_bytes{};
        std::optional<bool> integer_signed{};
        std::optional<std::uint32_t> unsigned_value_bits{};
        std::string provenance{};
        FactOrigin origin{FactOrigin::unspecified};
    };

  private:
    explicit TypeFacts(Definition facts)
        : facts_{std::move(facts)} {}

  public:
    auto operator==(TypeFacts const&) const -> bool = default;

    [[nodiscard]] static auto try_make(Definition facts) -> std::expected<TypeFacts, std::string>;
    [[nodiscard]] static auto make(Definition facts) -> TypeFacts;
    [[nodiscard]] auto data() const noexcept -> Definition const& { return facts_; }

  private:
    Definition facts_;
};

} // namespace ioj::layout
