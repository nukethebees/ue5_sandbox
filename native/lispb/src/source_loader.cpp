#include <codegen/source_loader.h>

#include <codegen/manifest_error.h>
#include <codegen/sexpr/fields.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <fstream>
#include <limits>
#include <set>
#include <span>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace codegen {
namespace detail {
namespace {
thread_local ModuleSourceReadHook module_source_read_hook{};
}

auto set_module_source_read_hook_for_testing(ModuleSourceReadHook const hook) noexcept
    -> ModuleSourceReadHook {
    return std::exchange(module_source_read_hook, hook);
}

} // namespace detail

namespace {

using sexpr::Form;
using sexpr::SourceSpan;

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

class Fields : public sexpr::Fields {
  public:
    Fields(Form const& form, std::string_view const expected_head, std::size_t const positionals)
        : sexpr::Fields{form, expected_head, positionals, fail} {}
};

auto text(Form const& form, std::string_view const purpose) -> std::string {
    return sexpr::text(form, purpose, fail);
}

auto boolean(Form const& form, std::string_view const purpose) -> bool {
    return sexpr::boolean(form, purpose, fail);
}

auto number(Form const& form, std::string_view const purpose) -> double {
    return sexpr::number(form, purpose, fail);
}

auto integer(Form const& form, std::string_view const purpose) -> int {
    return sexpr::integer(form, purpose, fail);
}

auto unsigned_integer(Form const& form, std::string_view const purpose) -> std::uint64_t {
    auto const value{text(form, purpose)};
    if (value.empty() || value.front() == '-') {
        fail(form.token.span, std::string{purpose} + " must be an unsigned integer");
    }
    try {
        std::size_t parsed{};
        auto const result{std::stoull(value, &parsed, 0)};
        if (parsed != value.size()) {
            fail(form.token.span, std::string{purpose} + " must be an unsigned integer");
        }
        return result;
    } catch (std::invalid_argument const&) {
        fail(form.token.span, std::string{purpose} + " must be an unsigned integer");
    } catch (std::out_of_range const&) {
        fail(form.token.span, std::string{purpose} + " is out of range");
    }
}

auto packed_integer(Form const& form, std::string_view const purpose) -> PackedIntegerValue {
    auto value{text(form, purpose)};
    auto negative{false};
    if (!value.empty() && (value.front() == '-' || value.front() == '+')) {
        negative = value.front() == '-';
        value.erase(value.begin());
    }
    if (value.empty()) {
        fail(form.token.span, std::string{purpose} + " must be an integer");
    }
    try {
        std::size_t parsed{};
        auto const magnitude{std::stoull(value, &parsed, 0)};
        if (parsed != value.size()) {
            fail(form.token.span, std::string{purpose} + " must be an integer");
        }
        return PackedIntegerValue::from_parts(negative, magnitude);
    } catch (std::invalid_argument const&) {
        fail(form.token.span, std::string{purpose} + " must be an integer");
    } catch (std::out_of_range const&) {
        fail(form.token.span, std::string{purpose} + " is out of range");
    }
}

auto text_list(Form const& form, std::string_view const purpose) -> std::vector<std::string> {
    return sexpr::text_list(form, purpose, fail);
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

auto cpp_lines_or(Fields const& fields, std::string_view const name) -> std::vector<std::string> {
    auto const* value{fields.optional(name)};
    if (value == nullptr) {
        return {};
    }
    if (value->token.kind != sexpr::TokenKind::raw_literal) {
        return text_list(*value, name);
    }

    auto source{sexpr::raw_text(*value, "cpp", name, fail)};
    if (source.empty()) {
        return {};
    }
    return {std::move(source)};
}

auto boolean_or(Fields const& fields, std::string_view const name, bool const fallback = false)
    -> bool {
    auto const* value{fields.optional(name)};
    return value == nullptr ? fallback : boolean(*value, name);
}

auto optional_boolean(Fields const& fields, std::string_view const name) -> std::optional<bool> {
    auto const* value{fields.optional(name)};
    return value == nullptr ? std::nullopt : std::optional<bool>{boolean(*value, name)};
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
    Fields const fields{form, form.head(), 2};
    fields.validate({"body",
                     "dependencies",
                     "trailing-return-type",
                     "const",
                     "noexcept",
                     "static",
                     "inline",
                     "constexpr",
                     "nodiscard",
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
    auto const is_static{boolean_or(fields, "static")};
    auto const is_const{boolean_or(fields, "const", form.head() == "view-function" && !is_static)};
    if (form.head() == "view-function" && !is_static && !is_const) {
        fail(form.token.span,
             "view-function must be const; use mutable-view-function for a mutable receiver");
    }
    auto body_lines{cpp_lines_or(fields, "body")};
    if (fields.optional("body") != nullptr && body_lines.empty()) {
        body_lines.emplace_back();
    }
    return FunctionSchema{
        .name = text(fields.positional(0), "function name"),
        .return_type = parse_type_ref(fields.positional(1)),
        .parameters = std::move(parameters),
        .body_lines = std::move(body_lines),
        .dependencies = text_list_or(fields, "dependencies"),
        .trailing_return_type = std::move(trailing_return_type),
        .is_const = is_const,
        .is_noexcept = boolean_or(fields, "noexcept"),
        .is_static = is_static,
        .is_inline = boolean_or(fields, "inline"),
        .definition_in_source = boolean_or(fields, "definition-in-source"),
        .template_parameters = optional_text(fields, "template-parameters"),
        .requires_clause = optional_text(fields, "requires"),
        .is_constexpr = boolean_or(fields, "constexpr"),
        .is_nodiscard = boolean_or(fields, "nodiscard"),
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

auto parse_semantic_relation_kind(Form const& form) -> SemanticRelationKind;
auto parse_semantic_relation_unit(Form const& form) -> SemanticRelationUnit;

auto parse_member(Form const& form) -> SoaMemberSchema {
    Fields const fields{form, "member", 3};
    fields.validate({"fixed-schema", "nested-schema", "mask-field", "mask-dimensions"},
                    {"relation"});
    auto const kind_name{text(fields.positional(1), "member kind")};
    SoaMemberKind kind;
    if (kind_name == "array") {
        kind = SoaMemberKind::array;
    } else if (kind_name == "nested") {
        kind = SoaMemberKind::nested;
    } else {
        fail(fields.positional(1).token.span, "unknown SOA member kind '" + kind_name + "'");
    }
    std::vector<SoaMaskDimensionSchema> mask_dimensions;
    if (auto const* dimensions{fields.optional("mask-dimensions")}) {
        if (!dimensions->is_list()) {
            fail(dimensions->token.span, "mask-dimensions must be a list");
        }
        for (auto const& dimension : dimensions->children) {
            if (!dimension.is_list() || dimension.children.size() != 2) {
                fail(dimension.token.span,
                     "mask dimension must contain an index name and extent expression");
            }
            mask_dimensions.push_back({
                .index_name = text(dimension.children[0], "mask dimension index name"),
                .extent = text(dimension.children[1], "mask dimension extent"),
            });
        }
    }
    std::optional<SemanticRelationSchema> relationship;
    for (auto const* declaration : fields.declarations()) {
        if (relationship.has_value()) {
            fail(declaration->token.span, "SOA member may have only one relationship");
        }
        Fields const relation{*declaration, "relation", 2};
        relation.validate({"unit"});
        auto const* unit{relation.optional("unit")};
        relationship = {
            .kind = parse_semantic_relation_kind(relation.positional(0)),
            .target = parse_type_ref(relation.positional(1)),
            .unit =
                unit == nullptr ? std::nullopt : std::optional{parse_semantic_relation_unit(*unit)},
        };
    }
    return SoaMemberSchema{
        .name = text(fields.positional(0), "member name"),
        .kind = kind,
        .type = parse_type_ref(fields.positional(2)),
        .fixed_schema = optional_text(fields, "fixed-schema"),
        .nested_schema = optional_text(fields, "nested-schema"),
        .mask_field = boolean_or(fields, "mask-field"),
        .mask_dimensions = std::move(mask_dimensions),
        .relationship = std::move(relationship),
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
    fields.validate({"storage",
                     "view-name",
                     "const-view-name",
                     "operations",
                     "export-specifier",
                     "using-declarations",
                     "array-allocator",
                     "single-allocation-allocator",
                     "equivalent-type",
                     "vector-components",
                     "copy-element-memberwise",
                     "layout-only",
                     "field-mask-name",
                     "field-enum-name"},
                    {"member",
                     "function",
                     "view-function",
                     "mutable-view-function",
                     "fixed",
                     "single-allocation"});

    std::vector<SoaMemberSchema> members;
    std::vector<FunctionSchema> functions;
    std::vector<FunctionSchema> const_view_functions;
    std::vector<FunctionSchema> mutable_view_functions;
    std::optional<FixedSoaSchema> fixed;
    std::optional<std::string> single_allocation;
    std::vector<SingleAllocationVariant> variants;
    for (auto const* declaration : fields.declarations()) {
        auto const head{declaration->head()};
        if (head == "member") {
            members.push_back(parse_member(*declaration));
        } else if (head == "function") {
            functions.push_back(parse_function(*declaration));
        } else if (head == "view-function") {
            const_view_functions.push_back(parse_function(*declaration));
        } else if (head == "mutable-view-function") {
            mutable_view_functions.push_back(parse_function(*declaration));
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
    std::optional<SoaStorage> storage;
    if (auto const value{optional_text(fields, "storage")}) {
        if (*value == "vector") {
            storage = SoaStorage::vector;
        } else if (*value == "single-allocation") {
            storage = SoaStorage::single_allocation;
        } else if (*value == "both") {
            storage = SoaStorage::both;
        } else {
            fail(form.token.span, "storage must be vector, single-allocation, or both");
        }
    }
    return SoaSchema{
        .name = text(fields.positional(0), "struct name"),
        .view_name = optional_text(fields, "view-name"),
        .const_view_name = optional_text(fields, "const-view-name"),
        .members = std::move(members),
        .operations = std::move(operations),
        .export_specifier = optional_text(fields, "export-specifier"),
        .functions = std::move(functions),
        .mutable_view_functions = std::move(mutable_view_functions),
        .using_declarations = text_list_or(fields, "using-declarations"),
        .equivalent_type = std::move(equivalent_type),
        .copy_element_memberwise = boolean_or(fields, "copy-element-memberwise"),
        .layout_only = boolean_or(fields, "layout-only"),
        .fixed = std::move(fixed),
        .single_allocation = std::move(single_allocation),
        .array_allocator = fields.optional("array-allocator")
                             ? std::optional{parse_type_ref(*fields.optional("array-allocator"))}
                             : std::nullopt,
        .single_allocation_variants = std::move(variants),
        .single_allocation_allocator =
            fields.optional("single-allocation-allocator")
                ? std::optional{parse_type_ref(*fields.optional("single-allocation-allocator"))}
                : std::nullopt,
        .field_mask_name = optional_text(fields, "field-mask-name"),
        .field_enum_name = optional_text(fields, "field-enum-name"),
        .vector_components = text_list_or(fields, "vector-components"),
        .storage = storage,
        .const_view_functions = std::move(const_view_functions),
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
        .prelude_lines = cpp_lines_or(fields, "prelude"),
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

auto parse_enum_unreal_projection(Form const& form) -> EnumUnrealProjection {
    Fields const fields{form, "unreal-projection", 1};
    fields.validate(
        {"header", "header-include", "conversion-header", "native-header-include", "reflection"});

    auto reflection{EnumReflection::uenum};
    if (auto const* value{fields.optional("reflection")}) {
        reflection = parse_enum_reflection(*value);
    }

    return EnumUnrealProjection{
        .name = text(fields.positional(0), "Unreal projection name"),
        .header = text(fields.required("header"), "Unreal projection header"),
        .header_include =
            text(fields.required("header-include"), "Unreal projection header include"),
        .conversion_header =
            text(fields.required("conversion-header"), "Unreal projection conversion header"),
        .native_header_include =
            text(fields.required("native-header-include"), "native enum header include"),
        .reflection = reflection,
    };
}

auto parse_semantic_relation_kind(Form const& form) -> SemanticRelationKind {
    auto const name{text(form, "semantic relationship kind")};
    constexpr std::array kinds{
        std::pair{"index_into", SemanticRelationKind::index_into},
        std::pair{"count_of", SemanticRelationKind::count_of},
        std::pair{"offset_into", SemanticRelationKind::offset_into},
        std::pair{"discriminates", SemanticRelationKind::discriminates},
        std::pair{"contains", SemanticRelationKind::contains},
        std::pair{"member_of", SemanticRelationKind::member_of},
        std::pair{"quantises", SemanticRelationKind::quantises},
        std::pair{"encoded_as", SemanticRelationKind::encoded_as},
        std::pair{"references", SemanticRelationKind::references},
    };
    auto const found{std::ranges::find_if(
        kinds, [&](auto const& candidate) { return candidate.first == name; })};
    if (found == kinds.end()) {
        fail(form.token.span, "unknown semantic relationship kind: " + name);
    }
    return found->second;
}

auto parse_semantic_relation_unit(Form const& form) -> SemanticRelationUnit {
    auto const name{text(form, "semantic relationship unit")};
    if (name == "elements") {
        return SemanticRelationUnit::elements;
    }
    if (name == "bytes") {
        return SemanticRelationUnit::bytes;
    }
    fail(form.token.span, "semantic relationship unit must be 'elements' or 'bytes'");
}

auto parse_packed_byte_order(Form const& form) -> PackedByteOrder {
    auto const name{text(form, "packed byte order")};
    if (name == "little") {
        return PackedByteOrder::little_endian;
    }
    if (name == "big") {
        return PackedByteOrder::big_endian;
    }
    fail(form.token.span, "packed byte order must be 'little' or 'big'");
}

auto parse_packed_bit_order(Form const& form) -> PackedBitOrder {
    auto const name{text(form, "packed bit order")};
    if (name == "lsb-first") {
        return PackedBitOrder::least_significant_first;
    }
    if (name == "msb-first") {
        return PackedBitOrder::most_significant_first;
    }
    fail(form.token.span, "packed bit order must be 'lsb-first' or 'msb-first'");
}

auto parse_packed_field(Form const& form) -> PackedFieldSchema {
    Fields const fields{form, "field", 2};
    fields.validate({"bits", "kind", "range-helper", "minimum", "maximum", "default"},
                    {"code", "relation"});

    auto kind{PackedFieldKind::unsigned_integer};
    if (auto const* value{fields.optional("kind")}) {
        auto const name{text(*value, "packed field kind")};
        if (name == "unsigned") {
            kind = PackedFieldKind::unsigned_integer;
        } else if (name == "signed") {
            kind = PackedFieldKind::signed_integer;
        } else if (name == "enum") {
            kind = PackedFieldKind::enumeration;
        } else if (name == "linear-quantized") {
            kind = PackedFieldKind::linear_quantized;
        } else if (name == "fixed-point") {
            kind = PackedFieldKind::fixed_point;
        } else if (name == "mini-float") {
            kind = PackedFieldKind::mini_float;
        } else {
            fail(value->token.span,
                 "packed field kind must be 'unsigned', 'signed', 'enum', "
                 "'linear-quantized', 'fixed-point', or 'mini-float'");
        }
    }

    auto const* minimum{fields.optional("minimum")};
    auto const* maximum{fields.optional("maximum")};
    auto const& bits_form{fields.required("bits")};
    auto const bits_text{text(bits_form, "packed field bits")};
    auto const bits{bits_text == "auto"
                        ? std::optional<int>{}
                        : std::optional<int>{integer(bits_form, "packed field bits")}};
    std::vector<PackedNamedCodeSchema> named_codes;
    named_codes.reserve(fields.declarations().size());
    std::optional<SemanticRelationSchema> relationship;
    for (auto const* declaration : fields.declarations()) {
        if (declaration->head() == "relation") {
            if (relationship.has_value()) {
                fail(declaration->token.span, "packed field may have only one relationship");
            }
            Fields const relation{*declaration, "relation", 2};
            relation.validate({"unit"});
            auto const* unit{relation.optional("unit")};
            relationship = {
                .kind = parse_semantic_relation_kind(relation.positional(0)),
                .target = parse_type_ref(relation.positional(1)),
                .unit = unit == nullptr ? std::nullopt
                                        : std::optional{parse_semantic_relation_unit(*unit)},
            };
            continue;
        }
        Fields const code{*declaration, "code", 1};
        code.validate({"value", "sentinel"});
        named_codes.push_back(
            {.name = text(code.positional(0), "packed named code name"),
             .value = packed_integer(code.required("value"), "packed named code value"),
             .sentinel = boolean_or(code, "sentinel")});
    }
    return PackedFieldSchema{
        .name = text(fields.positional(0), "packed field name"),
        .type = parse_type_ref(fields.positional(1)),
        .bits = bits,
        .kind = kind,
        .range_helper = boolean_or(fields, "range-helper"),
        .minimum_value = minimum == nullptr ? std::nullopt
                                            : std::optional<PackedIntegerValue>{packed_integer(
                                                  *minimum, "packed field minimum")},
        .maximum_value = maximum == nullptr ? std::nullopt
                                            : std::optional<PackedIntegerValue>{packed_integer(
                                                  *maximum, "packed field maximum")},
        .named_codes = std::move(named_codes),
        .relationship = std::move(relationship),
        .default_value = fields.optional("default") == nullptr
                           ? std::nullopt
                           : std::optional{packed_integer(*fields.optional("default"),
                                                          "packed field default")},
    };
}

auto parse_packed_reserved_bits(Form const& form) -> PackedReservedBitsSchema {
    Fields const fields{form, "reserved", 1};
    fields.validate({"bits"});
    return {.name = text(fields.positional(0), "packed reserved-region name"),
            .bits = integer(fields.required("bits"), "packed reserved-region bits")};
}

auto parse_packed_value(Form const& form) -> PackedValueSchema {
    Fields const fields{form, "packed-value", 1};
    fields.validate(
        {"storage", "invalid-value", "export-specifier", "mutable", "byte-order", "bit-order"},
        {"field", "reserved"});
    auto const* invalid_value{fields.optional("invalid-value")};
    auto const* byte_order{fields.optional("byte-order")};
    auto const* bit_order{fields.optional("bit-order")};

    std::vector<PackedSegmentSchema> segments;
    segments.reserve(fields.declarations().size());
    for (auto const* declaration : fields.declarations()) {
        if (declaration->head() == "field") {
            segments.emplace_back(parse_packed_field(*declaration));
        } else {
            segments.emplace_back(parse_packed_reserved_bits(*declaration));
        }
    }

    return PackedValueSchema{
        .name = text(fields.positional(0), "packed value name"),
        .storage_type = parse_type_ref(fields.required("storage")),
        .segments = std::move(segments),
        .invalid_value = invalid_value == nullptr ? std::nullopt
                                                  : std::optional<std::uint64_t>{unsigned_integer(
                                                        *invalid_value, "packed invalid value")},
        .export_specifier = optional_text(fields, "export-specifier"),
        .mutable_value = boolean_or(fields, "mutable"),
        .byte_order = byte_order == nullptr
                        ? std::nullopt
                        : std::optional<PackedByteOrder>{parse_packed_byte_order(*byte_order)},
        .bit_order = bit_order == nullptr
                       ? std::nullopt
                       : std::optional<PackedBitOrder>{parse_packed_bit_order(*bit_order)},
    };
}

auto parse_enum(Form const& form) -> EnumSchema {
    auto const has_underlying_type{
        form.children.size() > 2 &&
        form.children[2].token.kind != codegen::sexpr::TokenKind::keyword &&
        (!form.children[2].is_list() || form.children[2].head() == "type-ref")};
    Fields const fields{form, "enum", has_underlying_type ? 2U : 1U};
    fields.validate({"reflection",
                     "enum-array",
                     "count",
                     "conversions",
                     "export-specifier",
                     "native-api",
                     "bit-width",
                     "signed"},
                    {"value", "unreal-projection"});
    std::vector<EnumeratorSchema> values;
    std::optional<EnumUnrealProjection> unreal_projection;
    for (auto const* declaration : fields.declarations()) {
        if (declaration->head() == "value") {
            Fields const value{*declaration, "value", 1};
            value.validate({"value", "display-name", "hidden", "serialized-name", "sentinel"});
            values.push_back(EnumeratorSchema{
                .name = text(value.positional(0), "enumerator name"),
                .initializer = optional_text(value, "value"),
                .display_name = optional_text(value, "display-name"),
                .hidden = boolean_or(value, "hidden"),
                .serialized_name = optional_text(value, "serialized-name"),
                .sentinel = boolean_or(value, "sentinel"),
            });
        } else {
            if (unreal_projection.has_value()) {
                fail(declaration->token.span, "enum may define only one Unreal projection");
            }
            unreal_projection = parse_enum_unreal_projection(*declaration);
        }
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
    std::optional<std::uint32_t> bit_width;
    if (auto const* value{fields.optional("bit-width")}) {
        auto const parsed{unsigned_integer(*value, "enum bit width")};
        if (parsed == 0 || parsed > 64) {
            fail(value->token.span, "enum bit width must be between 1 and 64 bits");
        }
        bit_width = static_cast<std::uint32_t>(parsed);
    }
    return EnumSchema{
        .name = text(fields.positional(0), "enum name"),
        .underlying_type = has_underlying_type ? std::optional{parse_type_ref(fields.positional(1))}
                                               : std::nullopt,
        .bit_width = bit_width,
        .signedness = optional_boolean(fields, "signed"),
        .reflection = reflection,
        .values = std::move(values),
        .enum_array = boolean_or(fields, "enum-array"),
        .count = optional_text(fields, "count"),
        .conversions = std::move(conversions),
        .export_specifier = optional_text(fields, "export-specifier"),
        .native_api = boolean_or(fields, "native-api"),
        .unreal_projection = std::move(unreal_projection),
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
        .validation_lines = cpp_lines_or(fields, "validation"),
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

auto parse_setting_device(Form const& form) -> SettingDevice {
    auto const value{text(form, "setting device")};
    if (value == "shared") {
        return SettingDevice::shared;
    }
    if (value == "keyboard-mouse") {
        return SettingDevice::keyboard_mouse;
    }
    if (value == "controller") {
        return SettingDevice::controller;
    }
    fail(form.token.span, "unknown setting device '" + value + "'");
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
    fields.validate({"tooltip", "category", "value-type", "backend", "apply", "device"},
                    {"control"});
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
        .device = fields.optional("device") != nullptr
                    ? parse_setting_device(*fields.optional("device"))
                    : SettingDevice::shared,
        .control = parse_control(*fields.declarations().front()),
    };
}

auto parse_record_member(Form const& form) -> RecordMemberSchema {
    Fields const fields{form, "member", 2};
    fields.validate({"count", "initializer"}, {"relation"});
    auto const* count{fields.optional("count")};
    std::optional<SemanticRelationSchema> relationship;
    for (auto const* declaration : fields.declarations()) {
        if (relationship.has_value()) {
            fail(declaration->token.span, "record member may have only one relationship");
        }
        Fields const relation{*declaration, "relation", 2};
        relation.validate({"unit"});
        auto const* unit{relation.optional("unit")};
        relationship = {
            .kind = parse_semantic_relation_kind(relation.positional(0)),
            .target = parse_type_ref(relation.positional(1)),
            .unit =
                unit == nullptr ? std::nullopt : std::optional{parse_semantic_relation_unit(*unit)},
        };
    }
    return RecordMemberSchema{
        .name = text(fields.positional(0), "record member name"),
        .type = parse_type_ref(fields.positional(1)),
        .count = count == nullptr
                   ? std::nullopt
                   : std::optional<std::uint64_t>{unsigned_integer(*count, "record member count")},
        .relationship = std::move(relationship),
        .initializer = optional_text(fields, "initializer"),
    };
}

auto parse_record(Form const& form) -> RecordSchema {
    Fields const fields{form, "record", 1};
    fields.validate({"export-specifier", "comparison", "comparison-noexcept"},
                    {"member", "function"});
    auto comparison{RecordComparison::none};
    if (auto const value{optional_text(fields, "comparison")}) {
        if (*value == "equality") {
            comparison = RecordComparison::equality;
        } else if (*value == "three-way") {
            comparison = RecordComparison::three_way;
        } else {
            fail(fields.required("comparison").token.span, "expected equality or three-way");
        }
    }
    std::vector<FunctionSchema> functions;
    std::vector<RecordMemberSchema> members;
    members.reserve(fields.declarations().size());
    for (auto const* declaration : fields.declarations()) {
        if (declaration->head() == "function") {
            functions.push_back(parse_function(*declaration));
        } else {
            members.push_back(parse_record_member(*declaration));
        }
    }
    return RecordSchema{
        .name = text(fields.positional(0), "record name"),
        .members = std::move(members),
        .export_specifier = optional_text(fields, "export-specifier"),
        .comparison = comparison,
        .comparison_noexcept = boolean_or(fields, "comparison-noexcept"),
        .functions = std::move(functions),
    };
}

auto parse_union_alternative(Form const& form) -> UnionAlternativeSchema {
    Fields const fields{form, "alternative", 2};
    fields.validate({"count"});
    auto const* count{fields.optional("count")};
    return UnionAlternativeSchema{
        .name = text(fields.positional(0), "union alternative name"),
        .type = parse_type_ref(fields.positional(1)),
        .count =
            count == nullptr
                ? std::nullopt
                : std::optional<std::uint64_t>{unsigned_integer(*count, "union alternative count")},
    };
}

auto parse_union(Form const& form) -> UnionSchema {
    Fields const fields{form, "union", 1};
    fields.validate({"export-specifier"}, {"alternative"});
    std::vector<UnionAlternativeSchema> alternatives;
    alternatives.reserve(fields.declarations().size());
    for (auto const* declaration : fields.declarations()) {
        alternatives.push_back(parse_union_alternative(*declaration));
    }
    return UnionSchema{
        .name = text(fields.positional(0), "union name"),
        .alternatives = std::move(alternatives),
        .export_specifier = optional_text(fields, "export-specifier"),
    };
}

auto parse_tagged_union_alternative(Form const& form) -> TaggedUnionAlternativeSchema {
    Fields const fields{form, "alternative", 2};
    fields.validate({"count", "tag"});
    auto const* count{fields.optional("count")};
    return TaggedUnionAlternativeSchema{
        .name = text(fields.positional(0), "tagged union alternative name"),
        .type = parse_type_ref(fields.positional(1)),
        .count = count == nullptr ? std::nullopt
                                  : std::optional<std::uint64_t>{unsigned_integer(
                                        *count, "tagged union alternative count")},
        .tag = text(fields.required("tag"), "tagged union alternative tag"),
    };
}

auto parse_tagged_union(Form const& form) -> TaggedUnionSchema {
    Fields const fields{form, "tagged-union", 1};
    fields.validate({"discriminant", "export-specifier"}, {"alternative"});
    std::vector<TaggedUnionAlternativeSchema> alternatives;
    alternatives.reserve(fields.declarations().size());
    for (auto const* declaration : fields.declarations()) {
        alternatives.push_back(parse_tagged_union_alternative(*declaration));
    }
    return TaggedUnionSchema{
        .name = text(fields.positional(0), "tagged union name"),
        .discriminant = parse_type_ref(fields.required("discriminant")),
        .alternatives = std::move(alternatives),
        .export_specifier = optional_text(fields, "export-specifier"),
    };
}

auto parse_integer_scalar(Form const& form) -> IntegerScalarSchema {
    Fields const fields{form, "integer-scalar", 1};
    fields.validate({"signed", "minimum", "maximum", "bit-width", "cpp-emission", "cpp-type"},
                    {"code", "relation"});

    std::optional<std::uint32_t> bit_width;
    if (auto const* width{fields.optional("bit-width")}) {
        auto const width_text{text(*width, "integer scalar bit width")};
        if (width_text != "auto") {
            auto const parsed{integer(*width, "integer scalar bit width")};
            if (parsed <= 0 || parsed > 64) {
                fail(width->token.span, "integer scalar bit width must be in the range 1..64");
            }
            bit_width = static_cast<std::uint32_t>(parsed);
        }
    }

    auto cpp_emission{IntegerScalarCppEmission::none};
    if (auto const* emission{fields.optional("cpp-emission")}) {
        auto const emission_name{text(*emission, "integer scalar C++ emission policy")};
        if (emission_name == "constants") {
            cpp_emission = IntegerScalarCppEmission::constants;
        } else if (emission_name == "constants-with-names") {
            cpp_emission = IntegerScalarCppEmission::constants_with_names;
        } else if (emission_name == "alias") {
            cpp_emission = IntegerScalarCppEmission::alias;
        } else if (emission_name != "none") {
            fail(emission->token.span,
                 "integer scalar C++ emission policy must be none, constants, "
                 "constants-with-names, or alias");
        }
    }
    auto const* cpp_type{fields.optional("cpp-type")};

    std::vector<PackedNamedCodeSchema> named_codes;
    named_codes.reserve(fields.declarations().size());
    std::optional<SemanticRelationSchema> relationship;
    for (auto const* declaration : fields.declarations()) {
        if (declaration->head() == "relation") {
            if (relationship.has_value()) {
                fail(declaration->token.span, "integer scalar may have only one relationship");
            }
            Fields const relation{*declaration, "relation", 2};
            relation.validate({"unit"});
            auto const* unit{relation.optional("unit")};
            relationship = {
                .kind = parse_semantic_relation_kind(relation.positional(0)),
                .target = parse_type_ref(relation.positional(1)),
                .unit = unit == nullptr ? std::nullopt
                                        : std::optional{parse_semantic_relation_unit(*unit)},
            };
            continue;
        }
        Fields const code{*declaration, "code", 1};
        code.validate({"value", "sentinel"});
        named_codes.push_back(
            {.name = text(code.positional(0), "integer scalar code name"),
             .value = packed_integer(code.required("value"), "integer scalar code value"),
             .sentinel = boolean_or(code, "sentinel")});
    }

    return IntegerScalarSchema{
        .name = text(fields.positional(0), "integer scalar name"),
        .signedness = boolean(fields.required("signed"), "integer scalar signedness"),
        .minimum_value = packed_integer(fields.required("minimum"), "integer scalar minimum"),
        .maximum_value = packed_integer(fields.required("maximum"), "integer scalar maximum"),
        .bit_width = bit_width,
        .named_codes = std::move(named_codes),
        .relationship = std::move(relationship),
        .cpp_emission = cpp_emission,
        .cpp_type = cpp_type == nullptr ? std::nullopt : std::optional{parse_type_ref(*cpp_type)},
    };
}

auto parse_linear_quantized(Form const& form) -> LinearQuantizedSchema {
    Fields const fields{form, "linear-quantized", 1};
    fields.validate({"source", "bits", "reserved-codes", "clipping"});

    auto clipping{QuantizationClipping::reject};
    if (auto const* value{fields.optional("clipping")}) {
        auto const name{text(*value, "linear quantization clipping policy")};
        if (name == "clamp") {
            clipping = QuantizationClipping::clamp;
        } else if (name != "reject") {
            fail(value->token.span, "linear quantization clipping must be reject or clamp");
        }
    }

    auto const bits{integer(fields.required("bits"), "linear quantization bit width")};
    if (bits <= 0 || bits > 64) {
        fail(fields.required("bits").token.span,
             "linear quantization bit width must be in the range 1..64");
    }
    auto const* reserved{fields.optional("reserved-codes")};
    return LinearQuantizedSchema{
        .name = text(fields.positional(0), "linear quantization name"),
        .source = parse_type_ref(fields.required("source")),
        .bit_width = static_cast<std::uint32_t>(bits),
        .reserved_codes = reserved != nullptr
                            ? unsigned_integer(*reserved, "linear quantization reserved code count")
                            : 0,
        .clipping = clipping,
    };
}

auto parse_integer_varint(Form const& form) -> IntegerVarintSchema {
    Fields const fields{form, "integer-varint", 1};
    fields.validate({"source", "encoding"});

    auto const encoding_name{text(fields.required("encoding"), "integer varint encoding")};
    IntegerVarintEncoding encoding;
    if (encoding_name == "unsigned") {
        encoding = IntegerVarintEncoding::unsigned_varint;
    } else if (encoding_name == "signed") {
        encoding = IntegerVarintEncoding::signed_varint;
    } else if (encoding_name == "zigzag") {
        encoding = IntegerVarintEncoding::zigzag_varint;
    } else {
        fail(fields.required("encoding").token.span,
             "integer varint encoding must be unsigned, signed, or zigzag");
    }

    return {.name = text(fields.positional(0), "integer varint name"),
            .source = parse_type_ref(fields.required("source")),
            .encoding = encoding};
}

auto parse_fixed_point(Form const& form) -> FixedPointSchema {
    Fields const fields{form, "fixed-point", 1};
    fields.validate({"signed", "total-bits", "fractional-bits", "rounding", "minimum", "maximum"});

    auto const total_bits{integer(fields.required("total-bits"), "fixed-point total width")};
    if (total_bits <= 0 || total_bits > 64) {
        fail(fields.required("total-bits").token.span,
             "fixed-point total width must be in the range 1..64");
    }
    auto const fractional_bits{
        integer(fields.required("fractional-bits"), "fixed-point fractional width")};
    if (fractional_bits < 0 || fractional_bits > 64) {
        fail(fields.required("fractional-bits").token.span,
             "fixed-point fractional width must be in the range 0..64");
    }

    auto rounding{FixedPointRounding::nearest_even};
    if (auto const* value{fields.optional("rounding")}) {
        auto const name{text(*value, "fixed-point rounding policy")};
        if (name == "toward-zero") {
            rounding = FixedPointRounding::toward_zero;
        } else if (name != "nearest-even") {
            fail(value->token.span, "fixed-point rounding must be nearest-even or toward-zero");
        }
    }

    auto const* minimum{fields.optional("minimum")};
    auto const* maximum{fields.optional("maximum")};
    return {
        .name = text(fields.positional(0), "fixed-point name"),
        .signedness = boolean(fields.required("signed"), "fixed-point signedness"),
        .total_bits = static_cast<std::uint32_t>(total_bits),
        .fractional_bits = static_cast<std::uint32_t>(fractional_bits),
        .rounding = rounding,
        .minimum_value = minimum == nullptr ? std::nullopt
                                            : std::optional{text(*minimum, "fixed-point minimum")},
        .maximum_value = maximum == nullptr ? std::nullopt
                                            : std::optional{text(*maximum, "fixed-point maximum")}};
}

auto parse_mini_float(Form const& form) -> MiniFloatSchema {
    Fields const fields{form, "mini-float", 1};
    fields.validate({"sign-bits", "exponent-bits", "significand-bits", "bias"});

    auto const sign_bits{integer(fields.required("sign-bits"), "mini-float sign width")};
    if (sign_bits < 0 || sign_bits > 1) {
        fail(fields.required("sign-bits").token.span, "mini-float sign width must be zero or one");
    }
    auto const exponent_bits{
        integer(fields.required("exponent-bits"), "mini-float exponent width")};
    if (exponent_bits < 2 || exponent_bits > 15) {
        fail(fields.required("exponent-bits").token.span,
             "mini-float exponent width must be in the range 2..15");
    }
    auto const significand_bits{
        integer(fields.required("significand-bits"), "mini-float significand width")};
    if (significand_bits < 0 || significand_bits > 62) {
        fail(fields.required("significand-bits").token.span,
             "mini-float significand width must be in the range 0..62");
    }
    if (sign_bits + exponent_bits + significand_bits > 64) {
        fail(form.token.span, "mini-float total width must not exceed 64 bits");
    }
    auto const exponent_bias{integer(fields.required("bias"), "mini-float exponent bias")};
    if (exponent_bias < -32'768 || exponent_bias > 32'767) {
        fail(fields.required("bias").token.span,
             "mini-float exponent bias must be in the range -32768..32767");
    }

    return {.name = text(fields.positional(0), "mini-float name"),
            .sign_bits = static_cast<std::uint32_t>(sign_bits),
            .exponent_bits = static_cast<std::uint32_t>(exponent_bits),
            .significand_bits = static_cast<std::uint32_t>(significand_bits),
            .exponent_bias = exponent_bias};
}

auto parse_optional_sentinel(Form const& form) -> OptionalSentinelSchema {
    Fields const fields{form, "optional-sentinel", 1};
    fields.validate({"source", "sentinel"});

    return {.name = text(fields.positional(0), "optional sentinel name"),
            .source = parse_type_ref(fields.required("source")),
            .sentinel = text(fields.required("sentinel"), "optional sentinel code")};
}

auto parse_optional_presence_bit(Form const& form) -> OptionalPresenceBitSchema {
    Fields const fields{form, "optional-presence-bit", 1};
    fields.validate({"source"});

    return {.name = text(fields.positional(0), "optional presence-bit name"),
            .source = parse_type_ref(fields.required("source"))};
}

auto parse_layout(Form const& form) -> HomogeneousLayoutSchema {
    Fields const layout{form, "layout", 1};
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
    return HomogeneousLayoutSchema{
        .name = text(layout.positional(0), "homogeneous layout name"),
        .components = text_list(layout.required("components"), "layout components"),
        .input_members = text_list_or(layout, "input-members"),
        .value_types = std::move(value_types),
        .export_specifier = optional_text(layout, "export-specifier"),
    };
}

auto parse_vector_soa(Form const& form) -> VectorSoaSchema {
    Fields const fields{form, "vector-soa", 1};
    fields.validate({"value-type",
                     "components",
                     "equivalent-members",
                     "equivalent-constructor",
                     "equivalent-type",
                     "export-specifier"},
                    {"fixed"});
    std::optional<FixedSoaSchema> fixed;
    if (!fields.declarations().empty()) {
        if (fields.declarations().size() != 1) {
            fail(form.token.span, "vector SOA accepts at most one fixed declaration");
        }
        fixed = parse_fixed(*fields.declarations().front());
    }
    return VectorSoaSchema{
        .name = text(fields.positional(0), "vector storage name"),
        .value_type = parse_type_ref(fields.required("value-type")),
        .components = text_list(fields.required("components"), "vector components"),
        .equivalent_members = text_list_or(fields, "equivalent-members"),
        .equivalent_constructor = optional_text(fields, "equivalent-constructor"),
        .equivalent_type = parse_type_ref(fields.required("equivalent-type")),
        .export_specifier = optional_text(fields, "export-specifier"),
        .fixed = std::move(fixed),
    };
}

auto parse_normal_declaration(Form const& form) -> DeclarationSchema {
    auto const head{form.head()};
    if (head == "enum") {
        return parse_enum(form);
    }
    if (head == "integer-scalar") {
        return parse_integer_scalar(form);
    }
    if (head == "linear-quantized") {
        return parse_linear_quantized(form);
    }
    if (head == "integer-varint") {
        return parse_integer_varint(form);
    }
    if (head == "fixed-point") {
        return parse_fixed_point(form);
    }
    if (head == "mini-float") {
        return parse_mini_float(form);
    }
    if (head == "optional-sentinel") {
        return parse_optional_sentinel(form);
    }
    if (head == "optional-presence-bit") {
        return parse_optional_presence_bit(form);
    }
    if (head == "packed-value") {
        return parse_packed_value(form);
    }
    if (head == "record") {
        return parse_record(form);
    }
    if (head == "union") {
        return parse_union(form);
    }
    if (head == "tagged-union") {
        return parse_tagged_union(form);
    }
    if (head == "struct") {
        return parse_soa(form);
    }
    if (head == "vector-soa") {
        return parse_vector_soa(form);
    }
    if (head == "layout") {
        return parse_layout(form);
    }
    if (head == "table") {
        return parse_table(form);
    }
    if (head == "facade") {
        return parse_facade(form);
    }
    fail(form.token.span, "unknown normal module declaration '" + std::string{head} + "'");
}

auto parse_module(Form const& form) -> ModuleSchema {
    auto const head{form.head()};
    if (head == "module") {
        Fields const fields{form, head, 1};
        fields.validate({"header",
                         "source",
                         "header-include",
                         "namespace",
                         "include-order",
                         "prelude",
                         "helper-namespace",
                         "backend"},
                        {"enum",
                         "integer-scalar",
                         "linear-quantized",
                         "integer-varint",
                         "fixed-point",
                         "mini-float",
                         "optional-sentinel",
                         "optional-presence-bit",
                         "packed-value",
                         "record",
                         "union",
                         "tagged-union",
                         "struct",
                         "vector-soa",
                         "layout",
                         "table",
                         "facade",
                         "array-allocator"});
        NormalModuleSchema module{.settings = parse_module_settings(fields),
                                  .enum_helper_namespace =
                                      optional_text(fields, "helper-namespace")};
        if (auto const backend{optional_text(fields, "backend")}) {
            if (*backend == "standard-library") {
                module.soa_backend = SoaBackend::standard_library;
            } else if (*backend != "unreal") {
                fail(fields.required("backend").token.span,
                     "SOA backend must be unreal or standard-library");
            }
        }
        for (auto const* declaration : fields.declarations()) {
            if (declaration->head() == "array-allocator") {
                Fields const allocator{*declaration, "array-allocator", 2};
                allocator.validate({});
                module.soa_array_allocators.push_back(
                    {text(allocator.positional(0), "allocator prefix"),
                     parse_type_ref(allocator.positional(1))});
            } else {
                module.declarations.push_back(parse_normal_declaration(*declaration));
            }
        }
        return module;
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
    fail(form.token.span, "unknown module declaration '" + std::string{head} + "'");
}

auto parse_external_integer(Form const& form) -> ExternalIntegerSchema {
    Fields const fields{form, "integer", 0};
    fields.validate({"signed", "bit-width", "minimum", "maximum"}, {"code"});
    auto const width{integer(fields.required("bit-width"), "external integer bit width")};
    if (width < 1 || width > 64) {
        fail(form.token.span, "external integer bit width must be in the range 1..64");
    }
    auto const signedness{boolean(fields.required("signed"), "external integer signedness")};
    auto const magnitude{std::uint64_t{1} << (width - 1)};
    auto const maximum{signedness    ? magnitude - 1
                       : width == 64 ? (std::numeric_limits<std::uint64_t>::max)()
                                     : (std::uint64_t{1} << width) - 1};
    ExternalIntegerSchema result{.signedness = signedness,
                                 .minimum_value =
                                     signedness ? PackedIntegerValue::from_parts(true, magnitude)
                                                : PackedIntegerValue{0},
                                 .maximum_value = PackedIntegerValue{maximum},
                                 .bit_width = static_cast<std::uint32_t>(width)};
    if (auto const* minimum{fields.optional("minimum")}) {
        result.minimum_value = packed_integer(*minimum, "external integer minimum");
    }
    if (auto const* maximum_form{fields.optional("maximum")}) {
        result.maximum_value = packed_integer(*maximum_form, "external integer maximum");
    }
    for (auto const* declaration : fields.declarations()) {
        Fields const code{*declaration, "code", 1};
        code.validate({"value", "sentinel"});
        result.named_codes.push_back({.name = text(code.positional(0), "code name"),
                                      .value = packed_integer(code.required("value"), "code value"),
                                      .sentinel = boolean_or(code, "sentinel")});
    }
    return result;
}

auto parse_type_definition(Form const& form) -> std::pair<std::string, RegisteredTypeSchema> {
    Fields const fields{form, "type", 1};
    fields.validate({"spelling", "header", "pass-by"}, {"operation", "integer", "floating-point"});
    ExternalScalarSchema semantics;
    auto type{CppType{text(fields.required("spelling"), "type spelling")}};
    if (auto const* pass_by{fields.optional("pass-by")}) {
        type.parameter_passing = parse_parameter_passing(*pass_by);
    }
    if (auto header{optional_text(fields, "header")}) {
        type.dependencies.push_back(TypeDependency{type.spelling, std::move(*header), {}});
    }
    for (auto const* declaration : fields.declarations()) {
        if (declaration->head() != "operation") {
            if (!std::holds_alternative<std::monostate>(semantics)) {
                fail(declaration->token.span, "type may have only one scalar description");
            }
            if (declaration->head() == "integer") {
                semantics = parse_external_integer(*declaration);
            } else {
                Fields const floating{*declaration, "floating-point", 0};
                floating.validate({"format"});
                auto const format{text(floating.required("format"), "floating-point format")};
                if (format == "ieee754-binary16") {
                    semantics = FloatingPointFormat::ieee754_binary16;
                } else if (format == "ieee754-binary32") {
                    semantics = FloatingPointFormat::ieee754_binary32;
                } else if (format == "ieee754-binary64") {
                    semantics = FloatingPointFormat::ieee754_binary64;
                } else {
                    fail(declaration->token.span,
                         "unsupported floating-point format '" + format + "'");
                }
            }
            continue;
        }
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
    return {text(fields.positional(0), "type name"),
            {.cpp_type = std::move(type), .semantics = std::move(semantics)}};
}

} // namespace

auto load_type_registry(std::filesystem::path const& root) -> LoadedTypeRegistry {
    LoadedTypeRegistry result;
    std::set<std::filesystem::path> loaded;
    std::set<std::filesystem::path> active;
    auto load = [&](auto&& self, std::filesystem::path const& source_path) -> void {
        auto const path{std::filesystem::weakly_canonical(source_path)};
        if (active.contains(path)) {
            throw ManifestError{"Cyclic type registry include: " + path.string()};
        }
        if (!loaded.insert(path).second) {
            return;
        }
        active.insert(path);
        auto const index{result.sources.size()};
        auto source{read_file(path)};
        auto const forms{sexpr::read_forms(path.string(), source)};
        result.sources.push_back({.path = path, .text = std::move(source)});
        for (auto const& form : forms) {
            RegistrySourceRange const range{index,
                                            form.token.span.offset,
                                            form.closing.span.offset + 1,
                                            form.token.span.line,
                                            form.token.span.column};
            if (form.head() == "include") {
                Fields const include{form, "include", 1};
                include.validate({});
                auto const target{std::filesystem::weakly_canonical(
                    path.parent_path() / text(include.positional(0), "include path"))};
                result.sources[index].includes.push_back({range, target});
                try {
                    self(self, target);
                } catch (std::exception const& error) {
                    fail(form.token.span, error.what());
                }
                continue;
            }
            auto [name, type]{parse_type_definition(form)};
            if (!result.types.emplace(name, std::move(type)).second) {
                fail(form.token.span, "duplicate type definition '" + name + "'");
            }
            result.declarations.emplace(std::move(name), range);
        }
        active.erase(path);
    };
    try {
        load(load, root);
    } catch (sexpr::SourceError const& error) {
        throw ManifestError{error.what()};
    }
    return result;
}

auto load_module_source(std::filesystem::path const& path) -> LoadedModuleSource {
    LoadedModuleSource result;
    result.text = read_file(path);
    if (detail::module_source_read_hook != nullptr) {
        detail::module_source_read_hook(path);
    }
    try {
        for (auto const& form : sexpr::read_forms(path.string(), result.text)) {
            result.modules.push_back(parse_module(form));
        }
    } catch (sexpr::SourceError const& error) {
        throw ManifestError{error.what()};
    }
    return result;
}

auto load_sources(std::filesystem::path const& types_path,
                  std::span<std::filesystem::path const> const module_paths) -> Manifest {
    return load_sources(load_type_registry(types_path), module_paths);
}

auto load_sources(LoadedTypeRegistry const& registry,
                  std::span<std::filesystem::path const> const module_paths) -> Manifest {
    std::vector<ModuleSchema> modules;
    for (auto const& module_path : module_paths) {
        auto source{load_module_source(module_path)};
        for (auto& module : source.modules) {
            modules.push_back(std::move(module));
        }
    }
    return Manifest{manifest_schema_version, registry.types, std::move(modules)};
}

} // namespace codegen
