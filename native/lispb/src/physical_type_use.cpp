#include <codegen/schema/physical_type_use.h>

#include <cctype>

namespace codegen {
namespace physical_use_detail {

auto trim(std::string_view text) -> std::string_view {
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.front())) != 0) {
        text.remove_prefix(1);
    }
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.back())) != 0) {
        text.remove_suffix(1);
    }
    return text;
}

auto consume_cv(std::string_view& text) -> bool {
    text = trim(text);
    for (auto const qualifier : {std::string_view{"const"}, std::string_view{"volatile"}}) {
        if (text.starts_with(qualifier) &&
            (text.size() == qualifier.size() ||
             std::isspace(static_cast<unsigned char>(text[qualifier.size()])) != 0 ||
             text[qualifier.size()] == '*' || text[qualifier.size()] == '&')) {
            text.remove_prefix(qualifier.size());
            text = trim(text);
            return true;
        }
    }
    return false;
}

auto strip_cv(std::string_view text) -> std::string_view {
    while (consume_cv(text)) {}
    bool removed{true};
    while (removed) {
        removed = false;
        for (auto const qualifier : {std::string_view{" const"}, std::string_view{" volatile"}}) {
            if (text.ends_with(qualifier)) {
                text = trim(text.substr(0, text.size() - qualifier.size()));
                removed = true;
            }
        }
    }
    return text;
}

} // namespace physical_use_detail

auto classify_physical_type_use(std::string_view spelling) -> PhysicalTypeUse {
    using namespace physical_use_detail;
    auto unsupported = [&] {
        return PhysicalTypeUse{.form = PhysicalTypeForm::unsupported,
                               .object_spelling = {},
                               .diagnostic =
                                   "Unsupported physical type use '" + std::string{spelling} + "'.",
                               .names_semantic_type = false};
    };
    auto text{trim(spelling)};
    std::size_t modifier{text.size()};
    unsigned template_depth{};
    for (std::size_t index{}; index < text.size(); ++index) {
        auto const character{text[index]};
        if (character == '<') {
            ++template_depth;
        } else if (character == '>') {
            if (template_depth == 0) {
                return unsupported();
            }
            --template_depth;
        } else if (template_depth == 0) {
            if (character == '*' || character == '&') {
                modifier = index;
                break;
            }
            if (character == '(' || character == ')' || character == '[' || character == ']' ||
                character == ';' || character == '{' || character == '}') {
                return unsupported();
            }
        }
    }
    auto const object{strip_cv(text.substr(0, modifier))};
    if (object.empty() || template_depth != 0 || object.ends_with("::")) {
        return unsupported();
    }
    PhysicalTypeUse result{.form = PhysicalTypeForm::value,
                           .object_spelling = std::string{object},
                           .diagnostic = {},
                           .names_semantic_type = false};
    auto suffix{trim(text.substr(modifier))};
    while (suffix.starts_with('*')) {
        result.form = PhysicalTypeForm::object_pointer;
        suffix.remove_prefix(1);
        while (consume_cv(suffix)) {}
    }
    if (suffix == "&") {
        result.form = PhysicalTypeForm::lvalue_reference;
    } else if (suffix == "&&") {
        result.form = PhysicalTypeForm::rvalue_reference;
    } else if (!suffix.empty()) {
        return unsupported();
    }
    return result;
}

} // namespace codegen
