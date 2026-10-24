#pragma once

#include <ioj/ascii.h>
#include <ioj/constrained_value.h>
#include <ioj/enum_traits.h>
#include <ioj/field_mask.h>
#include <ioj/s7/ast_result.h>
#include <ioj/try_numeric_cast.h>
#include <ioj/vector.h>

#include <algorithm>
#include <array>
#include <bitset>
#include <concepts>
#include <format>
#include <optional>
#include <type_traits>
#include <utility>

namespace ioj::s7 {
[[nodiscard]] inline auto read_error(AstErrorCode const code,
                                     std::string_view const path,
                                     std::string message) -> std::unexpected<AstDiagnostics> {
    return std::unexpected{AstDiagnostics{{code, std::string{path}, std::move(message)}}};
}

inline void append_errors(AstDiagnostics& destination, AstDiagnostics source) {
    destination.insert(destination.end(),
                       std::make_move_iterator(source.begin()),
                       std::make_move_iterator(source.end()));
}

class Reader {
  public:
    explicit Reader(Ast const& ast)
        : ast_{ast} {}

    [[nodiscard]] auto ast() const -> Ast const& { return ast_; }
    [[nodiscard]] auto list(NodeIndex node, std::string_view path) const
        -> ReadResult<std::span<NodeIndex const>>;
    [[nodiscard]] auto symbol(NodeIndex node, std::string_view path) const
        -> ReadResult<std::string>;
    [[nodiscard]] auto number(NodeIndex node, std::string_view path) const -> ReadResult<double>;
  private:
    Ast const& ast_;
};

template <typename T>
struct TypeReader;

enum class ReadMode : std::uint8_t { Create, Override };

template <typename T>
void validate_read(T const& value, std::string_view path, AstDiagnostics& errors);

template <typename T>
void decode_field(Reader const& reader,
                  NodeIndex node,
                  std::string_view path,
                  T& destination,
                  ReadMode mode,
                  AstDiagnostics& errors);

template <typename T>
concept Readable = requires(Reader const& reader, NodeIndex node, std::string_view path) {
    { TypeReader<T>::read(reader, node, path) } -> std::same_as<ReadResult<T>>;
};

template <typename T>
[[nodiscard]] auto read(Reader const& reader, NodeIndex const node, std::string_view const path)
    -> ReadResult<T> {
    static_assert(Readable<T>, "No S7 TypeReader specialization for this destination type");
    if constexpr (Readable<T>) {
        return TypeReader<T>::read(reader, node, path);
    }
}

template <std::integral T>
    requires (!std::same_as<T, bool>)
struct TypeReader<T> {
    static auto read(Reader const& reader, NodeIndex const node, std::string_view const path)
        -> ReadResult<T> {
        auto const& ast{reader.ast()};
        auto const kind{ast.node(node).kind};
        if (kind == NodeKind::Integer || kind == NodeKind::Ratio) {
            auto value{std::int64_t{}};
            if (kind == NodeKind::Ratio) {
                auto const ratio{ast.ratio(node)};
                if (ratio.denominator <= 0 || ratio.numerator % ratio.denominator != 0) {
                    return read_error(
                        AstErrorCode::InvalidNumber, path, "Expected an exact integer");
                }
                value = ratio.numerator / ratio.denominator;
            } else {
                value = ast.integer(node);
            }
            if (auto const converted{try_numeric_cast<T>(value)}) {
                return *converted;
            }
        } else if (kind == NodeKind::Real) {
            if (auto const converted{try_numeric_cast<T>(ast.real(node))}) {
                return *converted;
            }
        }
        return read_error(
            AstErrorCode::InvalidNumber, path, "Expected an integer within the destination range");
    }
};

template <std::floating_point T>
struct TypeReader<T> {
    static auto read(Reader const& reader, NodeIndex const node, std::string_view const path)
        -> ReadResult<T> {
        auto const value{reader.number(node, path)};
        if (!value) {
            return std::unexpected{value.error()};
        }
        if (auto const converted{try_numeric_cast<T>(*value)}) {
            return *converted;
        }
        return read_error(
            AstErrorCode::InvalidNumber, path, "Number is outside the destination range");
    }
};

template <>
struct TypeReader<bool> {
    static auto read(Reader const& reader, NodeIndex const node, std::string_view const path)
        -> ReadResult<bool> {
        if (reader.ast().node(node).kind != NodeKind::Boolean) {
            return read_error(AstErrorCode::InvalidType, path, "Expected a boolean");
        }
        return reader.ast().boolean(node);
    }
};

template <>
struct TypeReader<std::string> {
    static auto read(Reader const& reader, NodeIndex const node, std::string_view const path)
        -> ReadResult<std::string> {
        if (reader.ast().node(node).kind != NodeKind::String) {
            return read_error(AstErrorCode::InvalidType, path, "Expected a string");
        }
        return std::string{reader.ast().text(node)};
    }
};

template <typename T>
concept SerializedEnum = std::is_enum_v<T> && requires(T value) {
    EnumTraits<T>::values;
    { to_serialized_string(value) } -> std::convertible_to<std::string_view>;
};

template <SerializedEnum T>
struct TypeReader<T> {
    static auto read(Reader const& reader, NodeIndex const node, std::string_view const path)
        -> ReadResult<T> {
        auto const name{reader.symbol(node, path)};
        if (!name) {
            return std::unexpected{name.error()};
        }
        for (auto const value : EnumTraits<T>::values) {
            if (equals_ignore_case(to_serialized_string(value), *name)) {
                return value;
            }
        }
        return read_error(
            AstErrorCode::UnknownSymbol, path, std::format("Unknown enum value '{}'", *name));
    }
};

template <typename T, ConstrainedValuePolicy Policy>
struct TypeReader<ConstrainedValue<T, Policy>> {
    static auto read(Reader const& reader, NodeIndex const node, std::string_view const path)
        -> ReadResult<ConstrainedValue<T, Policy>> {
        auto value{s7::read<T>(reader, node, path)};
        if (!value) {
            return std::unexpected{std::move(value.error())};
        }
        auto constrained{ConstrainedValue<T, Policy>::try_make(std::move(*value))};
        if (!constrained) {
            return read_error(std::is_arithmetic_v<T> ? AstErrorCode::InvalidNumber
                                                      : AstErrorCode::InvalidType,
                              path,
                              Policy.message);
        }
        return std::move(*constrained);
    }
};

template <typename T>
struct TypeReader<std::vector<T>> {
    static auto read(Reader const& reader, NodeIndex const node, std::string_view const path)
        -> ReadResult<std::vector<T>> {
        auto const children{reader.list(node, path)};
        if (!children) {
            return std::unexpected{children.error()};
        }
        std::vector<T> result;
        AstDiagnostics errors;
        auto const count{children->size()};
        result.reserve(count);
        for (std::size_t i{}; i < count; ++i) {
            auto value{s7::read<T>(reader, (*children)[i], std::format("{}[{}]", path, i))};
            if (value) {
                result.push_back(std::move(*value));
            } else {
                append_errors(errors, std::move(value.error()));
            }
        }
        if (!errors.empty()) {
            return std::unexpected{std::move(errors)};
        }
        return result;
    }
};

template <typename T, std::size_t N>
struct TypeReader<std::array<T, N>> {
    static auto read(Reader const& reader, NodeIndex const node, std::string_view const path)
        -> ReadResult<std::array<T, N>> {
        auto const children{reader.list(node, path)};
        if (!children) {
            return std::unexpected{children.error()};
        }
        if (children->size() != N) {
            return read_error(
                AstErrorCode::InvalidType, path, std::format("Expected {} elements", N));
        }
        std::array<std::optional<T>, N> values;
        AstDiagnostics errors;
        for (std::size_t i{}; i < N; ++i) {
            auto value{s7::read<T>(reader, (*children)[i], std::format("{}[{}]", path, i))};
            if (value) {
                values[i] = std::move(*value);
            } else {
                append_errors(errors, std::move(value.error()));
            }
        }
        if (!errors.empty()) {
            return std::unexpected{std::move(errors)};
        }
        return [&]<std::size_t... I>(std::index_sequence<I...>) {
            return std::array<T, N>{std::move(*values[I])...};
        }(std::make_index_sequence<N>{});
    }
};

template <typename T>
struct TypeReader<std::optional<T>> {
    static void decode(Reader const& reader,
                       NodeIndex node,
                       std::string_view path,
                       std::optional<T>& destination,
                       ReadMode mode,
                       AstDiagnostics& errors) {
        if (mode == ReadMode::Override && destination &&
            !(reader.ast().node(node).kind == NodeKind::Keyword &&
              reader.ast().text(node) == "none")) {
            decode_field(reader, node, path, *destination, mode, errors);
            return;
        }
        auto value{read(reader, node, path)};
        if (value) {
            destination = std::move(*value);
        } else {
            append_errors(errors, std::move(value.error()));
        }
    }

    static void
        validate(std::optional<T> const& value, std::string_view path, AstDiagnostics& errors) {
        if (value) {
            validate_read(*value, path, errors);
        }
    }

    static auto read(Reader const& reader, NodeIndex const node, std::string_view const path)
        -> ReadResult<std::optional<T>> {
        if (reader.ast().node(node).kind == NodeKind::Keyword &&
            reader.ast().text(node) == "none") {
            return std::optional<T>{};
        }
        auto value{s7::read<T>(reader, node, path)};
        if (!value) {
            return std::unexpected{std::move(value.error())};
        }
        return std::optional<T>{std::move(*value)};
    }
};

template <typename T, std::size_t N>
struct TypeReader<Vector<T, N>> {
    static auto read(Reader const& reader, NodeIndex const node, std::string_view const path)
        -> ReadResult<Vector<T, N>> {
        return s7::read<std::array<T, N>>(reader, node, path).transform([](auto values) {
            return Vector<T, N>{values};
        });
    }
};

template <typename T>
void read_field(Reader const& reader,
                NodeIndex const node,
                std::string_view const path,
                std::optional<T>& destination,
                AstDiagnostics& errors) {
    auto value{read<T>(reader, node, path)};
    if (value) {
        destination.emplace(std::move(*value));
    } else {
        append_errors(errors, std::move(value.error()));
    }
}

template <typename T>
void validate_read(T const& value, std::string_view path, AstDiagnostics& errors) {
    if constexpr (requires { TypeReader<T>::validate(value, path, errors); }) {
        TypeReader<T>::validate(value, path, errors);
    }
}

template <typename T>
void validate_member(T const& value,
                     std::string_view path,
                     std::string_view name,
                     AstDiagnostics& errors) {
    if constexpr (requires { TypeReader<T>::validate(value, path, errors); }) {
        validate_read(value, std::format("{}.{}", path, name), errors);
    }
}

template <typename T>
void decode_field(Reader const& reader,
                  NodeIndex node,
                  std::string_view path,
                  T& destination,
                  ReadMode mode,
                  AstDiagnostics& errors) {
    if constexpr (requires {
                      TypeReader<T>::decode(reader, node, path, destination, mode, errors);
                  }) {
        if (mode == ReadMode::Override && reader.ast().node(node).kind == NodeKind::List) {
            auto const children{reader.ast().children(node)};
            if (!children.empty() &&
                reader.ast().node(children.front()).kind == NodeKind::Keyword &&
                reader.ast().text(children.front()) == "settings-layers") {
                for (auto const layer : children.subspan(1)) {
                    decode_field(reader, layer, path, destination, mode, errors);
                }
                return;
            }
        }
        TypeReader<T>::decode(reader, node, path, destination, mode, errors);
    } else {
        auto value{read<T>(reader, node, path)};
        if (value) {
            destination = std::move(*value);
        } else {
            append_errors(errors, std::move(value.error()));
        }
    }
}

template <typename T>
auto read_override(Reader const& reader, NodeIndex node, std::string_view path, T value)
    -> ReadResult<T> {
    AstDiagnostics errors;
    decode_field(reader, node, path, value, ReadMode::Override, errors);
    if (errors.empty()) {
        validate_read(value, path, errors);
    }
    if (!errors.empty()) {
        return std::unexpected{std::move(errors)};
    }
    return value;
}

template <typename Field>
class LargeFieldMask {
  public:
    constexpr void set(Field field) { bits_.set(static_cast<std::size_t>(field)); }
    [[nodiscard]] constexpr auto has(Field field) const -> bool {
        return bits_.test(static_cast<std::size_t>(field));
    }
    [[nodiscard]] constexpr auto is_empty() const -> bool { return bits_.none(); }
    [[nodiscard]] constexpr auto excluding(LargeFieldMask const& other) const -> LargeFieldMask {
        auto result{*this};
        result.bits_ &= ~other.bits_;
        return result;
    }
  private:
    std::bitset<static_cast<std::size_t>(Field::Count)> bits_{};
};

template <typename Field>
using RecordMask = std::conditional_t<(static_cast<std::size_t>(Field::Count) <= 64),
                                      FieldMask<Field>,
                                      LargeFieldMask<Field>>;

template <typename Field, std::size_t N>
constexpr auto required_fields(std::array<std::string_view, N> const& names) -> RecordMask<Field> {
    RecordMask<Field> required;
    for (std::size_t i{}; i < N; ++i) {
        if (!names[i].empty()) {
            required.set(static_cast<Field>(i));
        }
    }
    return required;
}

template <typename Field, std::size_t N>
void require_fields(RecordMask<Field> const& seen,
                    RecordMask<Field> const& required,
                    std::array<std::string_view, N> const& names,
                    std::string_view const path,
                    AstDiagnostics& errors) {
    auto const missing{[&] {
        if constexpr (N <= 64) {
            using Storage = typename RecordMask<Field>::storage_type;
            return RecordMask<Field>{static_cast<Storage>(required.value() & ~seen.value())};
        } else {
            return required.excluding(seen);
        }
    }()};
    if (missing.is_empty()) {
        return;
    }
    for (std::size_t i{}; i < N; ++i) {
        auto const name{names[i]};
        if (missing.has(static_cast<Field>(i))) {
            errors.push_back({AstErrorCode::MissingProperty,
                              std::format("{}.{}", path, name),
                              std::format("Missing required property ':{}'", name)});
        }
    }
}

// Stream fields once; leave domain-specific dispatch and presence requirements to the caller.
template <typename Field, typename Lookup, typename Visit>
auto read_record(Reader const& reader,
                 NodeIndex const node,
                 std::string_view const tag,
                 std::string_view const path,
                 Lookup lookup,
                 Visit visit) -> ReadResult<RecordMask<Field>> {
    auto const children{reader.list(node, path)};
    if (!children) {
        return std::unexpected{children.error()};
    }
    auto const& ast{reader.ast()};
    if (children->empty() || ast.node((*children)[0]).kind != NodeKind::Keyword ||
        ast.text((*children)[0]) != tag) {
        return read_error(
            AstErrorCode::ExpectedRecord, path, std::format("Expected a '{}' record", tag));
    }
    RecordMask<Field> seen;
    AstDiagnostics errors;
    auto const count{children->size()};
    for (std::size_t i{1}; i < count; i += 2) {
        auto const key{(*children)[i]};
        auto const location{std::format("{}[{}]", path, i)};
        if (i + 1 == count) {
            errors.push_back({AstErrorCode::MissingValue, location, "Property has no value"});
        }
        if (ast.node(key).kind != NodeKind::Keyword) {
            errors.push_back(
                {AstErrorCode::ExpectedKeyword, location, "Expected a property keyword"});
            continue;
        }
        auto const name{ast.text(key)};
        auto const field{lookup(name)};
        if (!field) {
            errors.push_back({AstErrorCode::UnknownProperty,
                              std::format("{}.{}", path, name),
                              std::format("Unknown property ':{}'", name)});
            continue;
        }
        assert(static_cast<std::size_t>(*field) < static_cast<std::size_t>(Field::Count));
        if (seen.has(*field)) {
            errors.push_back({AstErrorCode::DuplicateProperty,
                              location,
                              std::format("Duplicate property ':{}'", name)});
            continue;
        }
        seen.set(*field);
        if (i + 1 < count) {
            visit(*field, (*children)[i + 1], std::format("{}.{}", path, name));
        }
    }
    if (!errors.empty()) {
        return std::unexpected{std::move(errors)};
    }
    return seen;
}

inline auto positional_arguments(Reader const& reader,
                                 NodeIndex node,
                                 std::string_view tag,
                                 std::string_view path,
                                 std::size_t count) -> ReadResult<std::span<NodeIndex const>> {
    auto values{reader.list(node, path)};
    if (!values) {
        return values;
    }
    auto const& ast{reader.ast()};
    if (values->empty() || ast.node(values->front()).kind != NodeKind::Keyword ||
        ast.text(values->front()) != tag) {
        return read_error(AstErrorCode::ExpectedRecord, path, std::format("Expected '{}'", tag));
    }
    if (values->size() != count + 1) {
        return read_error(
            AstErrorCode::InvalidType, path, std::format("'{}' requires {} arguments", tag, count));
    }
    return values->subspan(1);
}

template <typename T>
auto read_generated(Reader const& reader, NodeIndex node, std::string_view path) -> ReadResult<T> {
    auto result{[&]() -> ReadResult<T> {
        if constexpr (std::default_initializable<T>) {
            T value{};
            AstDiagnostics errors;
            TypeReader<T>::decode(reader, node, path, value, ReadMode::Create, errors);
            if (!errors.empty()) {
                return std::unexpected{std::move(errors)};
            }
            return value;
        } else {
            return TypeReader<T>::construct(reader, node, path);
        }
    }()};
    if (result) {
        AstDiagnostics errors;
        validate_read(*result, path, errors);
        if (!errors.empty()) {
            return std::unexpected{std::move(errors)};
        }
    }
    return result;
}
}
