#pragma once

#include <lispb/schema/schema_edit_kind.h>

#include <cassert>
#include <compare>
#include <cstddef>
#include <cstdint>

namespace lispb::schema {

// A command belongs to the document that created it and remains valid for its lifetime.
class SchemaEditCommand {
  public:
    SchemaEditCommand() = default;

    [[nodiscard]] auto kind() const -> SchemaEditKind {
        assert(value_ != 0);
        return static_cast<SchemaEditKind>((value_ >> 24) - 1);
    }
    [[nodiscard]] auto index() const -> std::uint32_t {
        assert(value_ != 0);
        return value_ & 0x00ffffff;
    }
    auto operator<=>(SchemaEditCommand const&) const = default;
  private:
    friend class EditableSchemaDocument;

    SchemaEditCommand(SchemaEditKind kind, std::size_t index)
        : value_{((static_cast<std::uint32_t>(kind) + 1) << 24) |
                 static_cast<std::uint32_t>(index)} {
        assert(index <= 0x00ffffff);
    }

    std::uint32_t value_{};
};

} // namespace lispb::schema
