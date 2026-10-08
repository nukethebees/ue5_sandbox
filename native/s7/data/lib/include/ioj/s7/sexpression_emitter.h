#pragma once
#include <cstdint>
#include <string>
#include <string_view>

namespace ioj::s7 {
// Tokens must already be valid Scheme atoms. Strings are escaped here.
class SexpressionEmitter {
  public:
    void begin_list(std::string_view head = {});
    void begin_quoted_list();
    void end_list();
    void token(std::string_view value);
    void string(std::string_view value);
    void newline();
    void comment(std::string_view value);
    [[nodiscard]] auto finish() && -> std::string;
  private:
    void separate();
    std::string output_{};
    std::uint32_t depth_{};
    bool separated_{true};
    bool line_start_{true};
};
}
