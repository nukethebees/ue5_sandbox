#include <codegen/schema/type_reference_role.h>

#include <codegen/schema/type_reference_category.h>
#include <codegen/schema/type_reference_part.h>
#include <lispb/strings.h>

#include <string>

namespace lispb::codegen {

auto type_reference_role_text(Strings const& strings, TypeReferenceRole const& role)
    -> std::string {
    std::string result;
    switch (role.category) {
        case TypeReferenceCategory::underlying_type:
            result = "underlying type";
            break;
        case TypeReferenceCategory::cpp_type:
            result = "C++ type";
            break;
        case TypeReferenceCategory::value:
            result = "value";
            break;
        case TypeReferenceCategory::source:
            result = "source";
            break;
        case TypeReferenceCategory::storage:
            result = "storage";
            break;
        case TypeReferenceCategory::field:
            result = "field";
            break;
        case TypeReferenceCategory::member:
            result = "member";
            break;
        case TypeReferenceCategory::function:
            result = "function";
            break;
        case TypeReferenceCategory::const_view_function:
            result = "const view function";
            break;
        case TypeReferenceCategory::view_function:
            result = "view function";
            break;
        case TypeReferenceCategory::discriminant:
            result = "discriminant";
            break;
        case TypeReferenceCategory::alternative:
            result = "alternative";
            break;
        case TypeReferenceCategory::value_type:
            result = "value type";
            break;
        case TypeReferenceCategory::equivalent_type:
            result = "equivalent type";
            break;
        case TypeReferenceCategory::array_allocator:
            result = "array allocator";
            break;
        case TypeReferenceCategory::column:
            result = "column";
            break;
        case TypeReferenceCategory::group:
            result = "group";
            break;
        case TypeReferenceCategory::target:
            result = "target";
            break;
        case TypeReferenceCategory::method:
            result = "method";
            break;
        case TypeReferenceCategory::layout_value:
            result = "value";
            break;
        case TypeReferenceCategory::validation_dependency:
            result = "validation dependency";
            break;
        case TypeReferenceCategory::module_allocator:
            result = "module allocator";
            break;
        case TypeReferenceCategory::setting:
            result = "setting";
            break;
    }
    if (role.subject.value != 0) {
        result += ' ';
        result += strings.view(role.subject);
    }
    if (role.part != TypeReferencePart::dependency &&
        (role.category == TypeReferenceCategory::function ||
         role.category == TypeReferenceCategory::view_function ||
         role.category == TypeReferenceCategory::method)) {
        result += " [" + std::to_string(role.index) + "]";
    }
    switch (role.part) {
        case TypeReferencePart::direct:
            break;
        case TypeReferencePart::relationship:
            result += " relationship";
            break;
        case TypeReferencePart::return_type:
            result += " return";
            break;
        case TypeReferencePart::parameter:
            result += " parameter ";
            result += strings.view(role.parameter);
            break;
        case TypeReferencePart::trailing_return:
            result += " trailing return";
            break;
        case TypeReferencePart::equivalent_type:
            result += " equivalent type";
            break;
        case TypeReferencePart::input:
            result += " input " + std::to_string(role.secondary_index);
            break;
        case TypeReferencePart::dependency:
            result += " dependency";
            break;
    }
    return result;
}

} // namespace lispb::codegen
