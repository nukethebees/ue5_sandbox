#pragma once

#include <codegen/schema/indices.h>

#include <optional>
#include <string_view>
namespace lispb::codegen {
class Declarations;
}

namespace lispb::schema {

class ChildQuery {
  public:
    ChildQuery(codegen::Declarations const& store, std::optional<codegen::ChildOccurrenceId> child);
    auto exists() const -> bool;
    auto name() const -> std::optional<std::string_view>;
    auto bits() const -> std::optional<std::uint32_t>;
    auto type() const -> std::optional<std::string_view>;
  private:
    codegen::Declarations const* store_;
    std::optional<codegen::ChildOccurrenceId> child_;
};

}
