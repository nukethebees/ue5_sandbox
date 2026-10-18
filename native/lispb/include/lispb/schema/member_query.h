#pragma once

#include <codegen/schema/indices.h>

#include <optional>
#include <string_view>
namespace lispb::codegen {
class Declarations;
}

namespace lispb::schema {

class MemberQuery {
  public:
    MemberQuery(codegen::Declarations const& store, std::optional<codegen::SoaMemberId> member);
    auto exists() const -> bool;
    auto name() const -> std::optional<std::string_view>;
    auto type() const -> std::optional<std::string_view>;
  private:
    codegen::Declarations const* store_;
    std::optional<codegen::SoaMemberId> member_;
};

}
