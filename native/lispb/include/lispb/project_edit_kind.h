#pragma once

#include <cstdint>
namespace lispb {

enum class ProjectEditKind : std::uint8_t {
    add_source,
    remove_source,
    rename_source,
    create_source,
    delete_pending_source
};

}
