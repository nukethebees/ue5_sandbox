#include <codegen/manifest.h>

#include <codegen/manifest_error.h>
#include <codegen/sexpr/reader.h>

#include <charconv>
#include <cmath>
#include <fstream>
#include <initializer_list>
#include <map>
#include <set>
#include <span>
#include <string_view>
#include <utility>

namespace codegen {
namespace {

using sexpr::Form;
using sexpr::SourceSpan;
using sexpr::TokenKind;

[[noreturn]] void fail(SourceSpan const& span, std::string const& message) {
    throw ManifestError{span.path + ":" + std::to_string(span.line) + ":" +
                        std::to_string(span.column) + ": error: " + message};
}

auto read_file(std::filesystem::path const& path) -> std::string {
    std::ifstream input{path, std::ios::binary};
    if (!input) {
        throw ManifestError{"Cannot open manifest file: " + path.string()};
    }
    return std::string{std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{}};
}

auto read_document(std::filesystem::path const& path) -> std::vector<Form> {
    try {
        return sexpr::read_forms(path.string(), read_file(path));
    } catch (sexpr::SourceError const& error) {
        throw ManifestError{error.what()};
    }
}

class Fields {
  public:
    Fields(Form const& form, std::string_view const expected_head, std::size_t const positionals)
        : form_{form} {
        if (!form.is_list() || form.head() != expected_head) {
            fail(form.token.span, "expected '" + std::string{expected_head} + "' form");
        }
        if (form.children.size() < positionals + 1) {
            fail(form.token.span,
                 "'" + std::string{expected_head} + "' requires " + std::to_string(positionals) +
                     " positional value" + (positionals == 1 ? "" : "s"));
        }

        for (std::size_t index{positionals + 1}; index < form.children.size();) {
            auto const& child{form.children[index]};
            if (child.token.kind == TokenKind::keyword) {
                if (index + 1 >= form.children.size() ||
                    form.children[index + 1].token.kind == TokenKind::keyword) {
                    fail(child.token.span, "property ':" + child.token.text + "' requires a value");
                }
                if (!properties_.emplace(child.token.text, &form.children[index + 1]).second) {
                    fail(child.token.span, "duplicate property ':" + child.token.text + "'");
                }
                property_spans_.emplace(child.token.text, child.token.span);
                index += 2;
                continue;
            }
            if (!child.is_list()) {
                fail(child.token.span, "expected a property or nested declaration");
            }
            declarations_.push_back(&child);
            ++index;
        }
    }

    auto positional(std::size_t const index) const -> Form const& {
        return form_.children.at(index + 1);
    }

    auto optional(std::string_view const name) const -> Form const* {
        auto const found{properties_.find(name)};
        return found == properties_.end() ? nullptr : found->second;
    }

    auto required(std::string_view const name) const -> Form const& {
        auto const* value{optional(name)};
        if (value == nullptr) {
            fail(form_.token.span, "missing required property ':" + std::string{name} + "'");
        }
        return *value;
    }

    auto declarations() const -> std::span<Form const* const> { return declarations_; }

    void validate(std::initializer_list<std::string_view> const properties,
                  std::initializer_list<std::string_view> const declarations = {}) const {
        std::set<std::string_view> const allowed_properties{properties};
        for (auto const& [name, unused] : properties_) {
            static_cast<void>(unused);
            if (!allowed_properties.contains(name)) {
                fail(property_spans_.at(name), "unknown property ':" + name + "'");
            }
        }

        std::set<std::string_view> const allowed_declarations{declarations};
        for (auto const* declaration : declarations_) {
            auto const head{declaration->head()};
            if (head.empty()) {
                fail(declaration->token.span, "nested declaration must begin with a symbol");
            }
            if (!allowed_declarations.contains(head)) {
                fail(declaration->token.span,
                     "unexpected nested declaration '" + std::string{head} + "'");
            }
        }
    }
  private:
    Form const& form_;
    std::map<std::string, Form const*, std::less<>> properties_;
    std::map<std::string, SourceSpan, std::less<>> property_spans_;
    std::vector<Form const*> declarations_;
};

auto text(Form const& form, std::string_view const purpose) -> std::string {
    if (form.is_list() ||
        (form.token.kind != TokenKind::atom && form.token.kind != TokenKind::string)) {
        fail(form.token.span, std::string{purpose} + " must be text or a symbol");
    }
    return form.token.text;
}

auto boolean(Form const& form, std::string_view const purpose) -> bool {
    if (!form.is_list() && form.token.kind == TokenKind::atom) {
        if (form.token.text == "true") {
            return true;
        }
        if (form.token.text == "false") {
            return false;
        }
    }
    fail(form.token.span, std::string{purpose} + " must be 'true' or 'false'");
}

auto integer(Form const& form, std::string_view const purpose) -> int {
    auto const value{text(form, purpose)};
    int result{};
    auto const [position,
                error]{std::from_chars(value.data(), value.data() + value.size(), result)};
    if (error != std::errc{} || position != value.data() + value.size()) {
        fail(form.token.span, std::string{purpose} + " must be an integer");
    }
    return result;
}

auto number(Form const& form, std::string_view const purpose) -> double {
    auto const value{text(form, purpose)};
    double result{};
    auto const [position,
                error]{std::from_chars(value.data(), value.data() + value.size(), result)};
    if (error != std::errc{} || position != value.data() + value.size() || !std::isfinite(result)) {
        fail(form.token.span, std::string{purpose} + " must be a finite number");
    }
    return result;
}

auto text_list(Form const& form, std::string_view const purpose) -> std::vector<std::string> {
    if (!form.is_list()) {
        fail(form.token.span, std::string{purpose} + " must be a list");
    }
    std::vector<std::string> result;
    result.reserve(form.children.size());
    for (auto const& child : form.children) {
        result.push_back(text(child, purpose));
    }
    return result;
}

auto optional_text(Fields const& fields, std::string_view const name)
    -> std::optional<std::string> {
    auto const* value{fields.optional(name)};
    return value == nullptr ? std::nullopt : std::optional<std::string>{text(*value, name)};
}

auto text_list_or(Fields const& fields, std::string_view const name) -> std::vector<std::string> {
    auto const* value{fields.optional(name)};
    return value == nullptr ? std::vector<std::string>{} : text_list(*value, name);
}

auto boolean_or(Fields const& fields, std::string_view const name, bool const fallback = false)
    -> bool {
    auto const* value{fields.optional(name)};
    return value == nullptr ? fallback : boolean(*value, name);
}

auto parse_type_ref(Form const& form) -> TypeRef {
    if (!form.is_list()) {
        return TypeRef{text(form, "type reference"), {}, std::nullopt};
    }
    Fields const fields{form, "type-ref", 1};
    fields.validate({"suffix", "nested"});
    return TypeRef{
        .name = text(fields.positional(0), "type reference name"),
        .suffix = optional_text(fields, "suffix").value_or(""),
        .nested = optional_text(fields, "nested"),
    };
}

auto type_ref_list(Form const& form, std::string_view const purpose) -> std::vector<TypeRef> {
    if (!form.is_list()) {
        fail(form.token.span, std::string{purpose} + " must be a list");
    }
    std::vector<TypeRef> result;
    result.reserve(form.children.size());
    for (auto const& child : form.children) {
        result.push_back(parse_type_ref(child));
    }
    return result;
}

auto parse_parameter_passing(Form const& form) -> ParameterPassing {
    auto const value{text(form, "parameter passing")};
    if (value == "const-ref") {
        return ParameterPassing::const_reference;
    }
    if (value == "value") {
        return ParameterPassing::value;
    }
    fail(form.token.span, "expected 'const-ref' or 'value'; got '" + value + "'");
}

auto parse_parameter(Form const& form) -> ParameterSchema {
    Fields const fields{form, "parameter", 2};
    fields.validate({"default"});
    return ParameterSchema{
        .type = parse_type_ref(fields.positional(1)),
        .name = text(fields.positional(0), "parameter name"),
        .default_value = optional_text(fields, "default"),
    };
}

auto parse_function(Form const& form) -> FunctionSchema {
    Fields const fields{form, "function", 2};
    fields.validate({"body",
                     "dependencies",
                     "trailing-return-type",
                     "const",
                     "noexcept",
                     "static",
                     "inline",
                     "definition-in-source",
                     "template-parameters",
                     "requires"},
                    {"parameter"});
    std::vector<ParameterSchema> parameters;
    for (auto const* declaration : fields.declarations()) {
        parameters.push_back(parse_parameter(*declaration));
    }
    std::optional<TypeRef> trailing_return_type;
    if (auto const* value{fields.optional("trailing-return-type")}) {
        trailing_return_type = parse_type_ref(*value);
    }
    return FunctionSchema{
        .name = text(fields.positional(0), "function name"),
        .return_type = parse_type_ref(fields.positional(1)),
        .parameters = std::move(parameters),
        .body_lines = text_list_or(fields, "body"),
        .dependencies = text_list_or(fields, "dependencies"),
        .trailing_return_type = std::move(trailing_return_type),
        .is_const = boolean_or(fields, "const"),
        .is_noexcept = boolean_or(fields, "noexcept"),
        .is_static = boolean_or(fields, "static"),
        .is_inline = boolean_or(fields, "inline"),
        .definition_in_source = boolean_or(fields, "definition-in-source"),
        .template_parameters = optional_text(fields, "template-parameters"),
        .requires_clause = optional_text(fields, "requires"),
    };
}

auto parse_operation(Form const& form) -> StorageOperation {
    auto const value{text(form, "storage operation")};
    static std::map<std::string, StorageOperation> const operations{
        {"reset", StorageOperation::reset},
        {"reserve", StorageOperation::reserve},
        {"add-uninitialised", StorageOperation::add_uninitialised},
        {"add-defaulted", StorageOperation::add_defaulted},
        {"remove-at-swap", StorageOperation::remove_at_swap},
        {"set-num", StorageOperation::set_num},
        {"copy-element", StorageOperation::copy_element},
        {"append-from", StorageOperation::append_from},
    };
    auto const found{operations.find(value)};
    if (found == operations.end()) {
        fail(form.token.span, "unknown storage operation '" + value + "'");
    }
    return found->second;
}

auto parse_operations(Form const& form) -> std::vector<StorageOperation> {
    if (!form.is_list()) {
        fail(form.token.span, "operations must be a list");
    }
    if (form.children.size() == 1 && text(form.children.front(), "storage operation") == "all") {
        return all_storage_operations();
    }
    std::vector<StorageOperation> result;
    result.reserve(form.children.size());
    for (auto const& child : form.children) {
        if (text(child, "storage operation") == "all") {
            fail(child.token.span, "'all' must be the only storage operation");
        }
        result.push_back(parse_operation(child));
    }
    return result;
}

auto parse_fixed(Form const& form) -> FixedSoaSchema {
    Fields const fields{form, "fixed", 1};
    fields.validate({"containers"});
    return FixedSoaSchema{
        .storage_name = text(fields.positional(0), "fixed storage name"),
        .containers = text_list_or(fields, "containers"),
    };
}

auto parse_member(Form const& form) -> SoaMemberSchema {
    Fields const fields{form, "member", 3};
    fields.validate({"fixed-schema", "nested-schema"});
    auto const kind_name{text(fields.positional(1), "member kind")};
    SoaMemberKind kind;
    if (kind_name == "array") {
        kind = SoaMemberKind::array;
    } else if (kind_name == "nested") {
        kind = SoaMemberKind::nested;
    } else {
        fail(fields.positional(1).token.span, "unknown SOA member kind '" + kind_name + "'");
    }
    return SoaMemberSchema{
        .name = text(fields.positional(0), "member name"),
        .kind = kind,
        .type = parse_type_ref(fields.positional(2)),
        .fixed_schema = optional_text(fields, "fixed-schema"),
        .nested_schema = optional_text(fields, "nested-schema"),
    };
}

auto parse_single_allocation(Form const& form, std::vector<SingleAllocationVariant>& variants)
    -> std::string {
    Fields const fields{form, "single-allocation", 1};
    fields.validate({}, {"variant"});
    for (auto const* declaration : fields.declarations()) {
        Fields const variant{*declaration, "variant", 2};
        variant.validate({});
        variants.push_back(SingleAllocationVariant{
            text(variant.positional(0), "single-allocation variant name"),
            parse_type_ref(variant.positional(1)),
        });
    }
    return text(fields.positional(0), "single-allocation name");
}

auto parse_soa(Form const& form) -> SoaSchema {
    Fields const fields{form, "struct", 1};
    fields.validate({"view-name",
                     "const-view-name",
                     "operations",
                     "export-specifier",
                     "using-declarations",
                     "equivalent-type",
                     "copy-element-memberwise"},
                    {"member", "function", "fixed", "single-allocation"});

    std::vector<SoaMemberSchema> members;
    std::vector<FunctionSchema> functions;
    std::optional<FixedSoaSchema> fixed;
    std::optional<std::string> single_allocation;
    std::vector<SingleAllocationVariant> variants;
    for (auto const* declaration : fields.declarations()) {
        auto const head{declaration->head()};
        if (head == "member") {
            members.push_back(parse_member(*declaration));
        } else if (head == "function") {
            functions.push_back(parse_function(*declaration));
        } else if (head == "fixed") {
            if (fixed.has_value()) {
                fail(declaration->token.span, "duplicate 'fixed' declaration");
            }
            fixed = parse_fixed(*declaration);
        } else if (head == "single-allocation") {
            if (single_allocation.has_value()) {
                fail(declaration->token.span, "duplicate 'single-allocation' declaration");
            }
            single_allocation = parse_single_allocation(*declaration, variants);
        }
    }

    std::vector<StorageOperation> operations;
    if (auto const* value{fields.optional("operations")}) {
        operations = parse_operations(*value);
    }
    std::optional<TypeRef> equivalent_type;
    if (auto const* value{fields.optional("equivalent-type")}) {
        equivalent_type = parse_type_ref(*value);
    }
    return SoaSchema{
        .name = text(fields.positional(0), "struct name"),
        .view_name = optional_text(fields, "view-name"),
        .const_view_name = optional_text(fields, "const-view-name"),
        .members = std::move(members),
        .operations = std::move(operations),
        .export_specifier = optional_text(fields, "export-specifier"),
        .functions = std::move(functions),
        .using_declarations = text_list_or(fields, "using-declarations"),
        .equivalent_type = std::move(equivalent_type),
        .copy_element_memberwise = boolean_or(fields, "copy-element-memberwise"),
        .fixed = std::move(fixed),
        .single_allocation = std::move(single_allocation),
        .single_allocation_variants = std::move(variants),
    };
}

auto parse_module_settings(Fields const& fields) -> ModuleSettings {
    std::optional<std::filesystem::path> source;
    if (auto const value{optional_text(fields, "source")}) {
        source = std::filesystem::path{*value};
    }
    return ModuleSettings{
        .name = text(fields.positional(0), "module name"),
        .header = text(fields.required("header"), "module header"),
        .source = std::move(source),
        .header_include = optional_text(fields, "header-include"),
        .namespace_name = optional_text(fields, "namespace"),
        .include_order = text_list_or(fields, "include-order"),
        .prelude_lines = text_list_or(fields, "prelude"),
    };
}

auto parse_enum_reflection(Form const& form) -> EnumReflection {
    auto const value{text(form, "enum reflection")};
    if (value == "none") {
        return EnumReflection::none;
    }
    if (value == "uenum") {
        return EnumReflection::uenum;
    }
    if (value == "blueprint") {
        return EnumReflection::blueprint;
    }
    fail(form.token.span, "unknown enum reflection mode '" + value + "'");
}

auto parse_enum_conversion(Form const& form) -> EnumConversion {
    auto const value{text(form, "enum conversion")};
    static std::map<std::string, EnumConversion> const conversions{
        {"lex-to-string", EnumConversion::lex_to_string},
        {"string-view", EnumConversion::string_view},
        {"string", EnumConversion::string},
        {"lex-to-display-string", EnumConversion::lex_to_display_string},
        {"display-string-view", EnumConversion::display_string_view},
        {"display-string", EnumConversion::display_string},
        {"lex-to-serialized-string", EnumConversion::lex_to_serialized_string},
        {"try-parse-serialized", EnumConversion::try_parse_serialized},
    };
    auto const found{conversions.find(value)};
    if (found == conversions.end()) {
        fail(form.token.span, "unknown enum conversion '" + value + "'");
    }
    return found->second;
}

auto parse_enum(Form const& form) -> EnumSchema {
    Fields const fields{form, "enum", 2};
    fields.validate({"reflection", "enum-array", "count", "conversions", "export-specifier"},
                    {"value"});
    std::vector<EnumeratorSchema> values;
    for (auto const* declaration : fields.declarations()) {
        Fields const value{*declaration, "value", 1};
        value.validate({"value", "display-name", "hidden", "serialized-name"});
        values.push_back(EnumeratorSchema{
            .name = text(value.positional(0), "enumerator name"),
            .initializer = optional_text(value, "value"),
            .display_name = optional_text(value, "display-name"),
            .hidden = boolean_or(value, "hidden"),
            .serialized_name = optional_text(value, "serialized-name"),
        });
    }
    std::vector<EnumConversion> conversions;
    if (auto const* list{fields.optional("conversions")}) {
        if (!list->is_list()) {
            fail(list->token.span, "conversions must be a list");
        }
        for (auto const& child : list->children) {
            conversions.push_back(parse_enum_conversion(child));
        }
    }
    auto reflection{EnumReflection::none};
    if (auto const* value{fields.optional("reflection")}) {
        reflection = parse_enum_reflection(*value);
    }
    return EnumSchema{
        .name = text(fields.positional(0), "enum name"),
        .underlying_type = parse_type_ref(fields.positional(1)),
        .reflection = reflection,
        .values = std::move(values),
        .enum_array = boolean_or(fields, "enum-array"),
        .count = optional_text(fields, "count"),
        .conversions = std::move(conversions),
        .export_specifier = optional_text(fields, "export-specifier"),
    };
}

auto parse_table(Form const& form) -> StaticTableSchema {
    Fields const fields{form, "table", 1};
    fields.validate({"export-specifier"}, {"row", "column", "group"});
    std::vector<StaticTableRowSchema> rows;
    std::vector<StaticTableColumnSchema> columns;
    std::vector<StaticTableGroupSchema> groups;
    for (auto const* declaration : fields.declarations()) {
        auto const head{declaration->head()};
        if (head == "row") {
            Fields const row{*declaration, "row", 1};
            row.validate({});
            rows.push_back({text(row.positional(0), "row name")});
        } else if (head == "column") {
            Fields const column{*declaration, "column", 2};
            column.validate({});
            columns.push_back(
                {text(column.positional(0), "column name"), parse_type_ref(column.positional(1))});
        } else if (head == "group") {
            Fields const group{*declaration, "group", 2};
            group.validate({"columns"});
            groups.push_back({text(group.positional(0), "group name"),
                              parse_type_ref(group.positional(1)),
                              text_list(group.required("columns"), "group columns")});
        }
    }
    return StaticTableSchema{
        .name = text(fields.positional(0), "table name"),
        .rows = std::move(rows),
        .columns = std::move(columns),
        .groups = std::move(groups),
        .export_specifier = optional_text(fields, "export-specifier"),
    };
}

auto parse_facade_target_storage(Form const& form) -> bool {
    auto const value{text(form, "facade target storage")};
    if (value == "pointer") {
        return false;
    }
    if (value == "reference") {
        return true;
    }
    fail(form.token.span, "expected 'pointer' or 'reference'; got '" + value + "'");
}

auto parse_facade(Form const& form) -> FacadeSchema {
    Fields const fields{form, "facade", 3};
    fields.validate({"validation",
                     "validation-dependencies",
                     "export-specifier",
                     "bind-access",
                     "method-access",
                     "friends",
                     "friend-kind",
                     "definitions-in-source",
                     "target-storage"},
                    {"method"});
    std::vector<FacadeMethodSchema> methods;
    for (auto const* declaration : fields.declarations()) {
        Fields const method{*declaration, "method", 2};
        method.validate({"const", "noexcept", "target-name"}, {"parameter"});
        std::vector<ParameterSchema> parameters;
        for (auto const* parameter : method.declarations()) {
            parameters.push_back(parse_parameter(*parameter));
        }
        methods.push_back(FacadeMethodSchema{
            .name = text(method.positional(0), "facade method name"),
            .return_type = parse_type_ref(method.positional(1)),
            .parameters = std::move(parameters),
            .is_const = boolean_or(method, "const"),
            .is_noexcept = boolean_or(method, "noexcept"),
            .target_name = optional_text(method, "target-name"),
        });
    }
    auto reference_target{false};
    if (auto const* value{fields.optional("target-storage")}) {
        reference_target = parse_facade_target_storage(*value);
    }
    return FacadeSchema{
        .name = text(fields.positional(0), "facade name"),
        .target_type = parse_type_ref(fields.positional(1)),
        .target_member_name = text(fields.positional(2), "facade target member name"),
        .methods = std::move(methods),
        .validation_lines = text_list_or(fields, "validation"),
        .validation_dependencies = text_list_or(fields, "validation-dependencies"),
        .export_specifier = optional_text(fields, "export-specifier"),
        .bind_access = optional_text(fields, "bind-access").value_or("public"),
        .method_access = optional_text(fields, "method-access").value_or("public"),
        .friends = text_list_or(fields, "friends"),
        .friend_kind = optional_text(fields, "friend-kind").value_or("class"),
        .definitions_in_source = boolean_or(fields, "definitions-in-source"),
        .reference_target = reference_target,
    };
}

auto parse_setting_apply_mode(Form const& form) -> SettingApplyMode {
    auto const value{text(form, "setting apply mode")};
    if (value == "immediate") {
        return SettingApplyMode::immediate;
    }
    if (value == "deferred") {
        return SettingApplyMode::deferred;
    }
    if (value == "confirm") {
        return SettingApplyMode::confirm;
    }
    fail(form.token.span, "unknown settings apply mode '" + value + "'");
}

auto parse_control(Form const& form) -> SettingControlSchema {
    Fields const fields{form, "control", 1};
    fields.validate(
        {"options-provider", "availability-provider", "min", "max", "step", "custom-row"});
    auto const kind_name{text(fields.positional(0), "setting control kind")};
    SettingControlKind kind;
    if (kind_name == "toggle") {
        kind = SettingControlKind::toggle;
    } else if (kind_name == "choice") {
        kind = SettingControlKind::choice;
    } else if (kind_name == "float-range") {
        kind = SettingControlKind::float_range;
    } else if (kind_name == "integer-range") {
        kind = SettingControlKind::integer_range;
    } else if (kind_name == "custom") {
        kind = SettingControlKind::custom;
    } else {
        fail(fields.positional(0).token.span, "unknown setting control kind '" + kind_name + "'");
    }
    auto optional_number = [&](std::string_view const name) -> std::optional<double> {
        auto const* value{fields.optional(name)};
        return value == nullptr ? std::nullopt : std::optional<double>{number(*value, name)};
    };
    return SettingControlSchema{
        .kind = kind,
        .options_provider = optional_text(fields, "options-provider"),
        .availability_provider = optional_text(fields, "availability-provider"),
        .minimum = optional_number("min"),
        .maximum = optional_number("max"),
        .step = optional_number("step"),
        .custom_row = optional_text(fields, "custom-row"),
    };
}

auto parse_setting(Form const& form) -> SettingSchema {
    Fields const fields{form, "setting", 2};
    fields.validate({"tooltip", "category", "value-type", "backend", "apply"}, {"control"});
    if (fields.declarations().size() != 1) {
        fail(form.token.span, "setting requires exactly one control declaration");
    }
    return SettingSchema{
        .name = text(fields.positional(0), "setting name"),
        .label = text(fields.positional(1), "setting label"),
        .tooltip = optional_text(fields, "tooltip"),
        .category = text(fields.required("category"), "setting category"),
        .value_type = parse_type_ref(fields.required("value-type")),
        .backend = text(fields.required("backend"), "setting backend"),
        .apply_mode = parse_setting_apply_mode(fields.required("apply")),
        .control = parse_control(*fields.declarations().front()),
    };
}

auto parse_module(Form const& form) -> ModuleSchema {
    auto const head{form.head()};
    if (head == "soa-module") {
        Fields const fields{form, head, 1};
        fields.validate({"header",
                         "source",
                         "header-include",
                         "namespace",
                         "include-order",
                         "prelude",
                         "experimental-stdlib"},
                        {"array-allocator", "struct"});
        std::vector<SoaAllocatorVariant> allocators;
        std::vector<SoaSchema> structs;
        for (auto const* declaration : fields.declarations()) {
            if (declaration->head() == "array-allocator") {
                Fields const allocator{*declaration, "array-allocator", 2};
                allocator.validate({});
                allocators.push_back({text(allocator.positional(0), "allocator prefix"),
                                      parse_type_ref(allocator.positional(1))});
            } else {
                structs.push_back(parse_soa(*declaration));
            }
        }
        return SoaModuleSchema{parse_module_settings(fields),
                               std::move(structs),
                               boolean_or(fields, "experimental-stdlib"),
                               std::move(allocators)};
    }
    if (head == "enum-module") {
        Fields const fields{form, head, 1};
        fields.validate({"header",
                         "source",
                         "header-include",
                         "namespace",
                         "include-order",
                         "prelude",
                         "helper-namespace"},
                        {"enum"});
        std::vector<EnumSchema> enums;
        for (auto const* declaration : fields.declarations()) {
            enums.push_back(parse_enum(*declaration));
        }
        return EnumModuleSchema{parse_module_settings(fields),
                                optional_text(fields, "helper-namespace"),
                                std::move(enums)};
    }
    if (head == "static-table-module") {
        Fields const fields{form, head, 1};
        fields.validate(
            {"header", "source", "header-include", "namespace", "include-order", "prelude"},
            {"table"});
        std::vector<StaticTableSchema> tables;
        for (auto const* declaration : fields.declarations()) {
            tables.push_back(parse_table(*declaration));
        }
        return StaticTableModuleSchema{parse_module_settings(fields), std::move(tables)};
    }
    if (head == "facade-module") {
        Fields const fields{form, head, 1};
        fields.validate(
            {"header", "source", "header-include", "namespace", "include-order", "prelude"},
            {"facade"});
        if (fields.declarations().size() != 1) {
            fail(form.token.span, "facade module requires exactly one facade declaration");
        }
        return FacadeModuleSchema{parse_module_settings(fields),
                                  parse_facade(*fields.declarations().front())};
    }
    if (head == "settings-module") {
        Fields const fields{form, head, 1};
        fields.validate({"header",
                         "source",
                         "header-include",
                         "namespace",
                         "include-order",
                         "prelude",
                         "api-name",
                         "state-name",
                         "export-specifier"},
                        {"category", "setting"});
        std::vector<SettingCategorySchema> categories;
        std::vector<SettingSchema> settings;
        for (auto const* declaration : fields.declarations()) {
            if (declaration->head() == "category") {
                Fields const category{*declaration, "category", 2};
                category.validate({});
                categories.push_back({text(category.positional(0), "category name"),
                                      text(category.positional(1), "category label")});
            } else {
                settings.push_back(parse_setting(*declaration));
            }
        }
        return SettingsModuleSchema{
            .settings = parse_module_settings(fields),
            .api_name = text(fields.required("api-name"), "settings API name"),
            .state_name = text(fields.required("state-name"), "settings state name"),
            .export_specifier = optional_text(fields, "export-specifier"),
            .categories = std::move(categories),
            .settings_list = std::move(settings),
        };
    }
    if (head == "umbrella-module") {
        Fields const fields{form, head, 1};
        fields.validate({"header",
                         "source",
                         "header-include",
                         "namespace",
                         "include-order",
                         "prelude",
                         "headers"});
        return UmbrellaModuleSchema{parse_module_settings(fields),
                                    text_list(fields.required("headers"), "umbrella headers")};
    }
    if (head == "vector-soa-module") {
        Fields const fields{form, head, 1};
        fields.validate({"header",
                         "source",
                         "header-include",
                         "namespace",
                         "include-order",
                         "prelude",
                         "storage-name",
                         "value-type",
                         "components",
                         "equivalent-type",
                         "export-specifier"},
                        {"fixed"});
        std::optional<FixedSoaSchema> fixed;
        if (!fields.declarations().empty()) {
            if (fields.declarations().size() != 1) {
                fail(form.token.span, "vector SOA module accepts at most one fixed declaration");
            }
            fixed = parse_fixed(*fields.declarations().front());
        }
        return VectorModuleSchema{
            .settings = parse_module_settings(fields),
            .storage_name = text(fields.required("storage-name"), "vector storage name"),
            .value_type = parse_type_ref(fields.required("value-type")),
            .components = text_list(fields.required("components"), "vector components"),
            .equivalent_type = parse_type_ref(fields.required("equivalent-type")),
            .export_specifier = optional_text(fields, "export-specifier"),
            .fixed = std::move(fixed),
        };
    }
    if (head == "homogeneous-soa-module") {
        Fields const fields{form, head, 1};
        fields.validate(
            {"header", "source", "header-include", "namespace", "include-order", "prelude"},
            {"layout"});
        std::vector<HomogeneousLayoutSchema> layouts;
        for (auto const* declaration : fields.declarations()) {
            Fields const layout{*declaration, "layout", 1};
            layout.validate({"components", "input-members", "export-specifier"}, {"value-type"});
            std::vector<HomogeneousValueSchema> value_types;
            for (auto const* value_declaration : layout.declarations()) {
                Fields const value{*value_declaration, "value-type", 2};
                value.validate({"equivalent-type", "input-types"});
                std::optional<TypeRef> equivalent;
                if (auto const* equivalent_value{value.optional("equivalent-type")}) {
                    equivalent = parse_type_ref(*equivalent_value);
                }
                std::vector<TypeRef> inputs;
                if (auto const* input_values{value.optional("input-types")}) {
                    inputs = type_ref_list(*input_values, "input types");
                }
                value_types.push_back(HomogeneousValueSchema{
                    .type = parse_type_ref(value.positional(0)),
                    .suffix = text(value.positional(1), "homogeneous type suffix"),
                    .equivalent_type = std::move(equivalent),
                    .input_types = std::move(inputs),
                });
            }
            layouts.push_back(HomogeneousLayoutSchema{
                .name = text(layout.positional(0), "homogeneous layout name"),
                .components = text_list(layout.required("components"), "layout components"),
                .input_members = text_list_or(layout, "input-members"),
                .value_types = std::move(value_types),
                .export_specifier = optional_text(layout, "export-specifier"),
            });
        }
        return HomogeneousModuleSchema{parse_module_settings(fields), std::move(layouts)};
    }
    fail(form.token.span, "unknown module declaration '" + std::string{head} + "'");
}

auto parse_type_definition(Form const& form) -> std::pair<std::string, CppType> {
    Fields const fields{form, "type", 1};
    fields.validate({"spelling", "header", "pass-by"}, {"operation"});
    auto type{CppType{text(fields.required("spelling"), "type spelling")}};
    if (auto const* pass_by{fields.optional("pass-by")}) {
        type.parameter_passing = parse_parameter_passing(*pass_by);
    }
    if (auto header{optional_text(fields, "header")}) {
        type.dependencies.push_back(TypeDependency{type.spelling, std::move(*header), {}});
    }
    for (auto const* declaration : fields.declarations()) {
        Fields const operation{*declaration, "operation", 2};
        operation.validate({"pass-by"});
        auto const operation_name{text(operation.positional(0), "type operation")};
        TypeOperation kind;
        if (operation_name == "add-element") {
            kind = TypeOperation::add_element;
        } else if (operation_name == "remove-at-swap") {
            kind = TypeOperation::remove_at_swap;
        } else if (operation_name == "set-element") {
            kind = TypeOperation::set_element;
        } else {
            fail(operation.positional(0).token.span,
                 "unknown type operation '" + operation_name + "'");
        }
        if (!type.member_operations
                 .emplace(kind, text(operation.positional(1), "type operation function"))
                 .second) {
            fail(declaration->token.span, "duplicate type operation '" + operation_name + "'");
        }
        if (kind != TypeOperation::remove_at_swap) {
            auto const* pass_by{operation.optional("pass-by")};
            type.member_operation_parameter_passing.emplace(
                kind,
                pass_by == nullptr ? ParameterPassing::const_reference
                                   : parse_parameter_passing(*pass_by));
        } else if (operation.optional("pass-by") != nullptr) {
            fail(operation.optional("pass-by")->token.span,
                 "remove-at-swap does not accept ':pass-by'");
        }
    }
    return {text(fields.positional(0), "type name"), std::move(type)};
}

auto load_types(std::filesystem::path const& path) -> std::map<std::string, CppType> {
    auto const forms{read_document(path)};
    std::map<std::string, CppType> result;
    for (auto const& form : forms) {
        auto [name, type]{parse_type_definition(form)};
        if (!result.emplace(name, std::move(type)).second) {
            fail(form.token.span, "duplicate type definition '" + name + "'");
        }
    }
    return result;
}

} // namespace

auto load_manifest(std::filesystem::path const& path) -> Manifest {
    auto const forms{read_document(path)};
    if (forms.size() != 1 || forms.front().head() != "codegen-manifest") {
        auto const span{forms.empty() ? SourceSpan{1, 1, path.string(), {}}
                                      : forms.front().token.span};
        fail(span, "manifest must contain exactly one 'codegen-manifest' form");
    }
    Fields const fields{forms.front(), "codegen-manifest", 0};
    fields.validate({"schema-version", "types", "modules"});
    auto const version{integer(fields.required("schema-version"), "schema version")};
    if (version != manifest_schema_version) {
        if (version == 1) {
            fail(fields.required("schema-version").token.span,
                 "schema version 1 is obsolete; version " +
                     std::to_string(manifest_schema_version) +
                     " replaces function 'suffix' with 'const' and 'noexcept' properties");
        }
        fail(fields.required("schema-version").token.span,
             "unsupported schema version " + std::to_string(version) + "; expected " +
                 std::to_string(manifest_schema_version));
    }

    auto const directory{path.parent_path()};
    auto types{load_types(directory / text(fields.required("types"), "types file"))};
    std::vector<ModuleSchema> modules;
    for (auto const& module_file : text_list(fields.required("modules"), "module files")) {
        auto const module_path{directory / module_file};
        for (auto const& form : read_document(module_path)) {
            modules.push_back(parse_module(form));
        }
    }
    return Manifest{version, std::move(types), std::move(modules)};
}

} // namespace codegen
