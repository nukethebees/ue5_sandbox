#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <string>

namespace ioj::layout {

struct TypeFacts {
    std::uint64_t size_bytes{};
    std::uint64_t alignment_bytes{};
    std::optional<std::uint32_t> unsigned_value_bits;

    auto operator==(TypeFacts const&) const -> bool = default;
};

class AbiProfile {
  public:
    explicit AbiProfile(std::string name = {});

    static auto host_common() -> AbiProfile;

    void set(std::string spelling, TypeFacts facts);
    void set_representation(std::string spelling, std::string represented_by);
    auto find(std::string const& spelling) const -> std::optional<TypeFacts>;
    auto name() const -> std::string const&;
    auto types() const -> std::map<std::string, TypeFacts, std::less<>> const&;
  private:
    std::string name_;
    std::map<std::string, TypeFacts, std::less<>> types_;
    std::map<std::string, std::string, std::less<>> representations_;
};

} // namespace ioj::layout
