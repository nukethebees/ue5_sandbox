#pragma once

#include <codegen/sexpr/source_location.h>
#include <codegen/sexpr/source_span.h>

#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>
namespace lispb::codegen::sexpr {
class Sources;

class SourceError final : public std::runtime_error {
  public:
    SourceError(Sources const& sources, SourceSpan span, std::string const& message);

    [[nodiscard]] auto path() const -> std::string_view { return path_; }
    [[nodiscard]] auto span() const -> SourceSpan const& { return span_; }
    [[nodiscard]] auto location() const -> SourceLocation;
    [[nodiscard]] auto message() const -> std::string_view { return message_; }
  private:
    std::string path_;
    SourceSpan span_;
    std::uint32_t line_;
    std::uint32_t column_;
    std::string message_;
};

}
