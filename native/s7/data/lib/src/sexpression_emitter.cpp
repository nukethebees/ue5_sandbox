#include <ioj/s7/sexpression_emitter.h>

#include <cassert>
#include <format>
#include <utility>

namespace ioj::s7 {
void SexpressionEmitter::separate() {
    if (line_start_) {
        output_.append(static_cast<std::size_t>(depth_) * 2, ' ');
        line_start_ = false;
    } else if (!separated_) {
        output_ += ' ';
    }
    separated_ = false;
}
void SexpressionEmitter::begin_list(std::string_view const head) {
    separate();
    output_ += '(';
    ++depth_;
    separated_ = true;
    if (!head.empty()) {
        token(head);
    }
}
void SexpressionEmitter::end_list() {
    assert(depth_ > 0);
    --depth_;
    if (line_start_) {
        output_.append(static_cast<std::size_t>(depth_) * 2, ' ');
        line_start_ = false;
    }
    output_ += ')';
    separated_ = false;
}
void SexpressionEmitter::begin_quoted_list() {
    separate();
    output_ += "'(";
    ++depth_;
    separated_ = true;
}
void SexpressionEmitter::token(std::string_view const value) {
    separate();
    output_.append(value);
}
void SexpressionEmitter::string(std::string_view const value) {
    separate();
    output_ += '"';
    for (auto const character : value) {
        switch (character) {
            case '\\':
                output_ += "\\\\";
                break;
            case '"':
                output_ += "\\\"";
                break;
            case '\n':
                output_ += "\\n";
                break;
            case '\r':
                output_ += "\\r";
                break;
            case '\t':
                output_ += "\\t";
                break;
            default:
                if (static_cast<unsigned char>(character) < 32) {
                    std::format_to(std::back_inserter(output_),
                                   "\\x{:02x};",
                                   static_cast<unsigned char>(character));
                } else {
                    output_ += character;
                }
                break;
        }
    }
    output_ += '"';
}
void SexpressionEmitter::newline() {
    output_ += '\n';
    line_start_ = true;
    separated_ = true;
}
void SexpressionEmitter::comment(std::string_view const value) {
    if (!line_start_) {
        newline();
    }
    separate();
    output_ += ";; ";
    output_.append(value);
    newline();
}
auto SexpressionEmitter::finish() && -> std::string {
    assert(depth_ == 0);
    if (!line_start_) {
        newline();
    }
    return std::move(output_);
}
}
