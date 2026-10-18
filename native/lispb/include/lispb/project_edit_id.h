#pragma once

#include <lispb/project_edit_kind.h>

#include <cstdint>
namespace lispb {

struct ProjectEditId {
    std::uint32_t value{};
    [[nodiscard]] auto kind() const -> ProjectEditKind {
        return static_cast<ProjectEditKind>(value >> 24);
    }
    [[nodiscard]] auto index() const -> std::uint32_t { return value & 0x00ffffffu; }
};

}
