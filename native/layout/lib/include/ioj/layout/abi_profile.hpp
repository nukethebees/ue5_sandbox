#pragma once

#include <ioj/layout/abi_profile_identity.hpp>
#include <ioj/layout/abi_profile_parse_error.hpp>
#include <ioj/layout/memory_facts.hpp>
#include <ioj/layout/type_facts.hpp>

#include <expected>
#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <string_view>

namespace ioj::layout {

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
auto save_abi_profile(std::filesystem::path const& path, AbiProfile const& profile)
    -> std::expected<void, std::string>;

} // namespace ioj::layout
