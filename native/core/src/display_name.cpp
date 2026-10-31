#include <ioj/display_name.h>

#include <ioj/ascii.h>

namespace ioj {
auto format_level_display_name(std::string_view const identifier) -> std::string {
    std::string result;
    result.reserve(identifier.size());
    bool word_start{true};
    for (auto const character : identifier) {
        auto const previous{result.empty() ? '\0' : result.back()};
        if (character == '_') {
            if (previous != '\0' && previous != ' ') {
                result.push_back(' ');
            }
            word_start = true;
            continue;
        }
        if ((isupper(character) && !isupper(previous)) ||
            (isdigit(character) && !isdigit(previous))) {
            if (previous != '\0' && previous != ' ') {
                result.push_back(' ');
            }
        }
        result.push_back(word_start ? toupper(character) : character);
        word_start = false;
    }
    return result;
}
}
