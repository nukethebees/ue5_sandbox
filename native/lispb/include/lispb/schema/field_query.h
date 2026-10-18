#pragma once

#include <codegen/schema/packed_segment_id.h>

#include <optional>
#include <string_view>
namespace lispb::codegen {
class Declarations;
}

namespace lispb::schema {

class FieldQuery {
  public:
    FieldQuery(codegen::Declarations const& store, std::optional<codegen::PackedSegmentId> field);
    auto exists() const -> bool;
    auto name() const -> std::optional<std::string_view>;
    auto bits() const -> std::optional<int>;
    auto type() const -> std::optional<std::string_view>;
  private:
    codegen::Declarations const* store_;
    std::optional<codegen::PackedSegmentId> field_;
};

}
