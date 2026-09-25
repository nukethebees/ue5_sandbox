#pragma once

#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <string_view>

namespace ioj::layout {

struct AbiProfileIdentity {
    std::optional<std::string> platform;
    std::optional<std::string> architecture;
    std::optional<std::string> abi;
    std::optional<std::string> compiler;
    std::optional<std::string> build_configuration;

    auto operator==(AbiProfileIdentity const&) const -> bool = default;
};

enum class FactOrigin { unspecified, target_abi, compiler_probe, manual_assumption, derived };

auto fact_origin_name(FactOrigin origin) -> std::string_view;

struct TypeFacts {
    // Complete-object sizeof, including tail padding; also the stride in an array.
    std::uint64_t size_bytes{};
    std::uint64_t alignment_bytes{};
    std::optional<bool> integer_signed;
    std::optional<std::uint32_t> unsigned_value_bits;
    std::string provenance;
    FactOrigin origin{FactOrigin::unspecified};

    auto operator==(TypeFacts const&) const -> bool = default;
};

auto validate_type_facts(TypeFacts const& facts) -> std::expected<void, std::string>;

struct MemoryFacts {
    std::optional<std::uint64_t> cache_line_bytes;
    std::optional<std::uint64_t> page_bytes;
    std::optional<std::uint64_t> l1_data_cache_bytes;
    std::optional<std::uint64_t> l2_cache_bytes;
    std::optional<std::uint64_t> l3_cache_bytes;
    std::string provenance;

    auto operator==(MemoryFacts const&) const -> bool = default;
};

struct AbiProfileParseError {
    std::size_t line{};
    std::string message;

    auto operator==(AbiProfileParseError const&) const -> bool = default;
};

class AbiProfile {
  public:
    explicit AbiProfile(std::string name = {}, AbiProfileIdentity identity = {});

    static auto host_common() -> AbiProfile;

    void set(std::string spelling, TypeFacts const& facts);
    void set_representation(std::string spelling, std::string represented_by);
    void set_memory_facts(MemoryFacts facts);
    void set_identity(AbiProfileIdentity identity);
    void set_object_pointer_representation(std::string spelling);
    auto object_pointer_representation() const -> std::optional<std::string> const&;
    auto find(std::string const& spelling) const -> std::optional<TypeFacts>;
    auto name() const -> std::string const&;
    auto identity() const -> AbiProfileIdentity const&;
    auto types() const -> std::map<std::string, TypeFacts, std::less<>> const&;
    auto representations() const -> std::map<std::string, std::string, std::less<>> const&;
    auto memory_facts() const -> MemoryFacts const&;
  private:
    std::string name_;
    AbiProfileIdentity identity_;
    std::map<std::string, TypeFacts, std::less<>> types_;
    std::map<std::string, std::string, std::less<>> representations_;
    MemoryFacts memory_facts_;
    std::optional<std::string> object_pointer_representation_;
};

auto parse_abi_profile(std::string_view source) -> std::expected<AbiProfile, AbiProfileParseError>;
auto load_abi_profile(std::filesystem::path const& path)
    -> std::expected<AbiProfile, AbiProfileParseError>;
auto serialize_abi_profile(AbiProfile const& profile) -> std::string;

} // namespace ioj::layout
