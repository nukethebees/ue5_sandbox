#pragma once

#include <codegen/schema/declaration_handle.h>

#include <optional>
#include <string>
#include <string_view>

namespace lispb::codegen {
class Declarations;
}

namespace lispb::schema {
class DeclarationQuery {
  public:
    DeclarationQuery(codegen::Declarations const& declarations,
                     std::optional<codegen::DeclarationHandle> declaration);
    auto exists() const -> bool;
    auto name() const -> std::string_view;
    auto kind() const -> std::optional<codegen::DeclarationKind>;
    auto source() const -> std::optional<std::string>;
  private:
    codegen::Declarations const* declarations_;
    std::optional<codegen::DeclarationHandle> declaration_;
};
}
