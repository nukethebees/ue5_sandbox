#include "build_identity.h"

#include <ioj/files.h>

#include <array>
#include <cstdint>
#include <format>
#include <fstream>
#include <string_view>
#include <utility>

namespace ioj::sandbox_host {
namespace {
class BinaryReader {
  public:
    explicit BinaryReader(std::filesystem::path const& path)
        : file_{path, std::ios::binary} {}

    template <std::size_t Size>
    auto bytes(std::uint64_t offset) -> std::array<unsigned char, Size> {
        std::array<unsigned char, Size> result{};
        file_.seekg(static_cast<std::streamoff>(offset));
        file_.read(reinterpret_cast<char*>(result.data()), result.size());
        return result;
    }
    auto word(std::uint64_t offset) -> std::uint16_t {
        auto value{bytes<2>(offset)};
        return static_cast<std::uint16_t>(value[0] | value[1] << 8);
    }
    auto dword(std::uint64_t offset) -> std::uint32_t {
        auto value{bytes<4>(offset)};
        return value[0] | value[1] << 8 | value[2] << 16 |
               static_cast<std::uint32_t>(value[3]) << 24;
    }
    auto valid() const -> bool { return static_cast<bool>(file_); }

  private:
    std::ifstream file_;
};

// PE32+ stores directory addresses relative to the image, and CodeView records by file offset.
auto codeview_identity(BinaryReader& file) -> std::string {
    if (file.word(0) != 0x5a4d) {
        return {};
    }
    std::uint64_t const pe{file.dword(0x3c)};
    auto const optional{pe + 24};
    if (file.dword(pe) != 0x4550 || file.word(optional) != 0x20b || file.word(pe + 20) < 168 ||
        file.dword(optional + 108) <= 6) {
        return {};
    }
    auto const debug_directory{optional + 112 + 6 * 8};
    auto const debug_rva{file.dword(debug_directory)};
    auto const debug_size{file.dword(debug_directory + 4)};
    auto const sections{file.word(pe + 6)};
    auto const section_table{optional + file.word(pe + 20)};
    for (unsigned index{}; index < sections && file.valid(); ++index) {
        auto const section{section_table + index * 40};
        auto const rva{file.dword(section + 12)};
        auto const size{file.dword(section + 16)};
        if (debug_rva < rva || debug_rva - rva > size || debug_size > size - (debug_rva - rva)) {
            continue;
        }
        std::uint64_t const raw{file.dword(section + 20)};
        auto const directory{raw + (debug_rva - rva)};
        auto const count{debug_size / 28};
        for (std::uint32_t entry{}; entry < count && file.valid(); ++entry) {
            auto const debug{directory + entry * 28};
            if (file.dword(debug + 12) != 2 || file.dword(debug + 16) < 24) {
                continue;
            }
            std::uint64_t const record{file.dword(debug + 24)};
            if (file.dword(record) != 0x53445352) { // RSDS
                continue;
            }
            auto const guid_and_age{file.bytes<20>(record + 4)};
            std::string result;
            for (auto byte : guid_and_age) {
                result += std::format("{:02x}", byte);
            }
            return file.valid() ? result : std::string{};
        }
    }
    return {};
}
}

auto read_build_identity(std::filesystem::path const& path)
    -> std::expected<std::string, std::string> {
    BinaryReader file{path};
    auto identity{codeview_identity(file)};
    if (identity.empty()) {
        return std::unexpected{"Cannot read the CodeView build identity from " +
                               path_to_utf8(path)};
    }
    return identity;
}

auto verify_build_identity(std::filesystem::path const& path, std::string_view expected)
    -> std::expected<void, std::string> {
    auto identity{read_build_identity(path)};
    if (!identity) {
        return std::unexpected{std::move(identity.error())};
    }
    if (*identity != expected) {
        return std::unexpected{"Launcher and game DLL have different build identities: " +
                               path_to_utf8(path)};
    }
    return {};
}
}
