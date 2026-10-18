#include <lispb/compilation.h>

#include <lispb/artifact_id.h>
#include <lispb/artifact_kind.h>

#include <cassert>
#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace lispb {

void Compilation::add_text(std::filesystem::path path, std::string content, bool format_generated) {
    assert(text_paths_.size() < 0x01000000);
    ArtifactId const id{static_cast<std::uint32_t>(text_paths_.size())};
    text_paths_.push_back(std::move(path));
    text_contents_.push_back(std::move(content));
    text_formatting_.push_back(format_generated);
    artifacts.push_back(id);
}
void Compilation::add_binary(std::filesystem::path path, std::vector<std::uint8_t> content) {
    assert(binary_paths_.size() < 0x01000000);
    ArtifactId const id{(static_cast<std::uint32_t>(ArtifactKind::binary) << 24) |
                        static_cast<std::uint32_t>(binary_paths_.size())};
    binary_paths_.push_back(std::move(path));
    binary_contents_.push_back(std::move(content));
    artifacts.push_back(id);
}
auto Compilation::path(ArtifactId const id) const -> std::filesystem::path const& {
    return id.kind() == ArtifactKind::text ? text_paths_.at(id.index())
                                           : binary_paths_.at(id.index());
}
auto Compilation::text(ArtifactId const id) const -> std::string const& {
    assert(id.kind() == ArtifactKind::text);
    return text_contents_.at(id.index());
}
auto Compilation::binary(ArtifactId const id) const -> std::span<std::uint8_t const> {
    assert(id.kind() == ArtifactKind::binary);
    return binary_contents_.at(id.index());
}
auto Compilation::needs_formatting(ArtifactId const id) const -> bool {
    assert(id.kind() == ArtifactKind::text);
    return text_formatting_.at(id.index());
}
void Compilation::set_formatted_text(ArtifactId const id, std::string content) {
    assert(id.kind() == ArtifactKind::text);
    text_contents_.at(id.index()) = std::move(content);
    text_formatting_.at(id.index()) = false;
}

}
