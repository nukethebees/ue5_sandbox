#include <lispb/schema/type_graph.h>

#include <codegen/validation.h>

#include <algorithm>
#include <limits>
#include <memory>
#include <set>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace lispb::schema {
namespace {

auto qualified_name(codegen::ModuleSettings const& settings, std::string const& name)
    -> std::string {
    return settings.namespace_name.has_value() ? *settings.namespace_name + "::" + name : name;
}

} // namespace

class TypeGraphBuilder {
  public:
    explicit TypeGraphBuilder(codegen::Manifest const& manifest)
        : manifest_{manifest} {}

    auto build() -> TypeGraph {
        declare_types();
        bind_registered_types();
        resolve_definitions();
        build_edges();
        return std::move(graph_);
    }
  private:
    struct Declaration {
        TypeId id;
        std::size_t module_index{};
        std::size_t declaration_index{};
    };

    auto add_type(TypeIdentity identity, std::string cpp_spelling, TypeDefinition definition)
        -> TypeId {
        if (graph_.types_.size() >= TypeId::invalid_value) {
            throw std::invalid_argument{"Semantic type graph contains too many types"};
        }
        if (graph_.identities_.contains(identity)) {
            throw std::invalid_argument{
                "Duplicate semantic type identity: " + identity.module_name + ":" +
                identity.namespace_name + ":" + identity.name};
        }
        auto const id{TypeId{static_cast<std::uint32_t>(graph_.types_.size())}};
        graph_.types_.push_back(TypeNode{.identity = identity,
                                         .cpp_spelling = std::move(cpp_spelling),
                                         .definition = std::move(definition)});
        graph_.identities_.emplace(std::move(identity), id);
        return id;
    }

    template <typename Definition>
    void declare(std::size_t const module_index,
                 std::size_t const declaration_index,
                 codegen::ModuleSettings const& settings,
                 std::string const& name,
                 Definition definition) {
        auto const namespace_name{settings.namespace_name.value_or("")};
        auto const id{add_type(TypeIdentity{.origin = TypeOrigin::declaration,
                                            .module_name = settings.name,
                                            .namespace_name = namespace_name,
                                            .name = name},
                               qualified_name(settings, name),
                               std::move(definition))};
        declarations_.push_back({id, module_index, declaration_index});
        declarations_by_module_name_.emplace(std::pair{settings.name, name}, id);
        declarations_by_spelling_[graph_.type(id).cpp_spelling].push_back(id);
        declarations_by_name_[name].push_back(id);
    }

    void declare_types() {
        for (std::size_t module_index{}; module_index < manifest_.modules.size(); ++module_index) {
            std::visit(
                [&](auto const& module) {
                    using Module = std::decay_t<decltype(module)>;
                    if constexpr (std::is_same_v<Module, codegen::EnumModuleSchema>) {
                        for (std::size_t index{}; index < module.enums.size(); ++index) {
                            declare(module_index,
                                    index,
                                    module.settings,
                                    module.enums[index].name,
                                    EnumType{});
                        }
                    } else if constexpr (std::is_same_v<Module, codegen::PackedValueModuleSchema>) {
                        for (std::size_t index{}; index < module.values.size(); ++index) {
                            declare(module_index,
                                    index,
                                    module.settings,
                                    module.values[index].name,
                                    PackedType{});
                        }
                    } else if constexpr (std::is_same_v<Module, codegen::RecordModuleSchema>) {
                        for (std::size_t index{}; index < module.records.size(); ++index) {
                            declare(module_index,
                                    index,
                                    module.settings,
                                    module.records[index].name,
                                    RecordType{});
                        }
                    } else if constexpr (std::is_same_v<Module, codegen::SoaModuleSchema>) {
                        for (std::size_t index{}; index < module.structs.size(); ++index) {
                            declare(module_index,
                                    index,
                                    module.settings,
                                    module.structs[index].name,
                                    SoaType{.backend = module.backend});
                        }
                    } else if constexpr (std::is_same_v<Module, codegen::VectorModuleSchema>) {
                        declare(module_index,
                                0,
                                module.settings,
                                module.storage_name,
                                SoaType{.backend = module.backend,
                                        .source_kind = SoaSourceKind::vector});
                    }
                },
                manifest_.modules[module_index]);
        }
    }

    void bind_registered_types() {
        for (auto const& [name, cpp_type] : manifest_.types) {
            auto const separator{cpp_type.spelling.rfind("::")};
            auto const declared_name{separator == std::string::npos
                                         ? cpp_type.spelling
                                         : cpp_type.spelling.substr(separator + 2)};
            if (auto const local{declarations_by_module_name_.find(std::pair{name, declared_name})};
                local != declarations_by_module_name_.end()) {
                graph_.registered_types_.emplace(name, local->second);
                continue;
            }

            auto declarations{declarations_by_spelling_.find(cpp_type.spelling)};
            auto const* matches{
                declarations == declarations_by_spelling_.end() ? nullptr : &declarations->second};
            if (matches == nullptr) {
                auto const by_name{declarations_by_name_.find(cpp_type.spelling)};
                if (by_name != declarations_by_name_.end()) {
                    matches = &by_name->second;
                }
            }
            if (matches != nullptr) {
                if (matches->size() != 1) {
                    throw std::invalid_argument{"Registered type '@" + name +
                                                "' ambiguously matches multiple declarations of '" +
                                                cpp_type.spelling + "'"};
                }
                graph_.registered_types_.emplace(name, matches->front());
                continue;
            }

            auto const id{
                add_type(TypeIdentity{.origin = TypeOrigin::registered_external, .name = name},
                         cpp_type.spelling,
                         ExternalType{.cpp_type = cpp_type, .registered_names = {name}})};
            graph_.registered_types_.emplace(name, id);
        }
    }

    auto raw_external(std::string const& spelling) -> TypeId {
        if (auto const found{raw_external_types_.find(spelling)};
            found != raw_external_types_.end()) {
            return found->second;
        }
        auto const id{add_type(TypeIdentity{.origin = TypeOrigin::cpp_spelling, .name = spelling},
                               spelling,
                               ExternalType{.cpp_type = codegen::CppType{spelling}})};
        raw_external_types_.emplace(spelling, id);
        return id;
    }

    auto resolve_ref(codegen::TypeRef const& reference, std::string const& module_name)
        -> ResolvedTypeRef {
        auto const cpp_type{codegen::resolve_type(reference, manifest_.types)};
        if (reference.name.starts_with('@')) {
            auto const name{reference.name.substr(1)};
            auto const found{graph_.registered_types_.find(name)};
            if (found == graph_.registered_types_.end()) {
                throw std::invalid_argument{"Unknown semantic type reference: " + reference.name};
            }
            return {.type = found->second, .cpp_type = cpp_type};
        }

        if (auto const local{
                declarations_by_module_name_.find(std::pair{module_name, reference.name})};
            local != declarations_by_module_name_.end()) {
            return {.type = local->second, .cpp_type = cpp_type};
        }

        if (auto const declarations{declarations_by_spelling_.find(reference.name)};
            declarations != declarations_by_spelling_.end() && declarations->second.size() == 1) {
            return {.type = declarations->second.front(), .cpp_type = cpp_type};
        }
        return {.type = raw_external(reference.name), .cpp_type = cpp_type};
    }

    auto local_declaration(std::string const& module_name, std::string const& name) const
        -> std::optional<TypeId> {
        auto const found{declarations_by_module_name_.find(std::pair{module_name, name})};
        return found == declarations_by_module_name_.end() ? std::nullopt
                                                           : std::optional{found->second};
    }

    void resolve_definitions() {
        for (auto const& declaration : declarations_) {
            auto const& module_schema{manifest_.modules[declaration.module_index]};
            std::visit(
                [&](auto const& module) {
                    using Module = std::decay_t<decltype(module)>;
                    if constexpr (std::is_same_v<Module, codegen::EnumModuleSchema>) {
                        auto const& source{module.enums[declaration.declaration_index]};
                        EnumType type{.underlying_type =
                                          resolve_ref(source.underlying_type, module.settings.name),
                                      .enumerators = {},
                                      .count = source.count};
                        type.enumerators.reserve(source.values.size());
                        for (auto const& value : source.values) {
                            type.enumerators.push_back(
                                {.name = value.name,
                                 .explicit_value = value.initializer,
                                 .display_name = value.display_name,
                                 .serialized_name = value.serialized_name,
                                 .hidden = value.hidden,
                                 .count_sentinel =
                                     source.count.has_value() && value.name == *source.count});
                        }
                        graph_.types_[declaration.id.value].definition = std::move(type);
                    } else if constexpr (std::is_same_v<Module, codegen::PackedValueModuleSchema>) {
                        auto const& source{module.values[declaration.declaration_index]};
                        PackedType type{
                            .storage_type = resolve_ref(source.storage_type, module.settings.name),
                            .fields = {},
                            .invalid_raw_value = source.invalid_value,
                        };
                        type.fields.reserve(source.fields.size());
                        for (auto const& field : source.fields) {
                            type.fields.push_back(
                                {.name = field.name,
                                 .semantic_type = resolve_ref(field.type, module.settings.name),
                                 .bit_width = static_cast<std::uint32_t>(field.bits),
                                 .kind = field.kind,
                                 .range_helper = field.range_helper});
                        }
                        graph_.types_[declaration.id.value].definition = std::move(type);
                    } else if constexpr (std::is_same_v<Module, codegen::RecordModuleSchema>) {
                        auto const& source{module.records[declaration.declaration_index]};
                        RecordType type;
                        type.members.reserve(source.members.size());
                        for (auto const& member : source.members) {
                            type.members.push_back(
                                {.name = member.name,
                                 .semantic_type = resolve_ref(member.type, module.settings.name),
                                 .count = member.count});
                        }
                        graph_.types_[declaration.id.value].definition = std::move(type);
                    } else if constexpr (std::is_same_v<Module, codegen::SoaModuleSchema>) {
                        auto const& source{module.structs[declaration.declaration_index]};
                        SoaType type{.backend = module.backend,
                                     .source_kind = SoaSourceKind::structure,
                                     .columns = {},
                                     .related_storage_name = source.single_allocation};
                        type.columns.reserve(source.members.size());
                        for (auto const& member : source.members) {
                            std::optional<TypeId> nested;
                            if (member.nested_schema.has_value()) {
                                nested =
                                    local_declaration(module.settings.name, *member.nested_schema);
                                if (!nested.has_value()) {
                                    throw std::invalid_argument{
                                        "Unknown nested semantic type '" + *member.nested_schema +
                                        "' in module '" + module.settings.name + "'"};
                                }
                            }
                            type.columns.push_back(
                                {.name = member.name,
                                 .semantic_type = resolve_ref(member.type, module.settings.name),
                                 .kind = member.kind,
                                 .nested_type = nested});
                        }
                        graph_.types_[declaration.id.value].definition = std::move(type);
                    } else if constexpr (std::is_same_v<Module, codegen::VectorModuleSchema>) {
                        SoaType type{.backend = module.backend,
                                     .source_kind = SoaSourceKind::vector,
                                     .columns = {},
                                     .related_storage_name = std::nullopt};
                        auto const value_type{resolve_ref(module.value_type, module.settings.name)};
                        type.columns.reserve(module.components.size());
                        for (auto const& component : module.components) {
                            type.columns.push_back(
                                {.name = component, .semantic_type = value_type});
                        }
                        graph_.types_[declaration.id.value].definition = std::move(type);
                    }
                },
                module_schema);
        }
    }

    void add_dependency(TypeNode& node, TypeId const dependency) {
        auto const own_id{static_cast<std::uint32_t>(std::addressof(node) - graph_.types_.data())};
        if (dependency.valid() && dependency.value != own_id &&
            std::ranges::find(node.dependencies, dependency) == node.dependencies.end()) {
            node.dependencies.push_back(dependency);
        }
    }

    void build_edges() {
        for (auto& node : graph_.types_) {
            std::visit(
                [&](auto const& definition) {
                    using Definition = std::decay_t<decltype(definition)>;
                    if constexpr (std::is_same_v<Definition, EnumType>) {
                        add_dependency(node, definition.underlying_type.type);
                    } else if constexpr (std::is_same_v<Definition, PackedType>) {
                        add_dependency(node, definition.storage_type.type);
                        for (auto const& field : definition.fields) {
                            add_dependency(node, field.semantic_type.type);
                        }
                    } else if constexpr (std::is_same_v<Definition, RecordType>) {
                        for (auto const& member : definition.members) {
                            add_dependency(node, member.semantic_type.type);
                        }
                    } else if constexpr (std::is_same_v<Definition, SoaType>) {
                        for (auto const& column : definition.columns) {
                            add_dependency(node, column.semantic_type.type);
                            if (column.nested_type.has_value()) {
                                add_dependency(node, *column.nested_type);
                            }
                        }
                    }
                },
                node.definition);
        }
        for (std::uint32_t index{}; index < graph_.types_.size(); ++index) {
            auto const user{TypeId{index}};
            for (auto const dependency : graph_.types_[index].dependencies) {
                graph_.types_[dependency.value].users.push_back(user);
            }
        }
    }

    codegen::Manifest const& manifest_;
    TypeGraph graph_;
    std::vector<Declaration> declarations_;
    std::map<std::pair<std::string, std::string>, TypeId> declarations_by_module_name_;
    std::map<std::string, std::vector<TypeId>, std::less<>> declarations_by_spelling_;
    std::map<std::string, std::vector<TypeId>, std::less<>> declarations_by_name_;
    std::map<std::string, TypeId, std::less<>> raw_external_types_;
};

auto TypeGraph::types() const -> std::span<TypeNode const> {
    return types_;
}

auto TypeGraph::type(TypeId const id) const -> TypeNode const& {
    if (!id.valid() || id.value >= types_.size()) {
        throw std::out_of_range{"Invalid semantic type id"};
    }
    return types_[id.value];
}

auto TypeGraph::find(TypeIdentity const& identity) const -> std::optional<TypeId> {
    auto const found{identities_.find(identity)};
    return found == identities_.end() ? std::nullopt : std::optional{found->second};
}

auto TypeGraph::find_declared(std::string const& module_name, std::string const& name) const
    -> std::optional<TypeId> {
    for (auto const& [identity, id] : identities_) {
        if (identity.origin == TypeOrigin::declaration && identity.module_name == module_name &&
            identity.name == name) {
            return id;
        }
    }
    return std::nullopt;
}

auto TypeGraph::find_registered(std::string const& name) const -> std::optional<TypeId> {
    auto const found{registered_types_.find(name)};
    return found == registered_types_.end() ? std::nullopt : std::optional{found->second};
}

auto TypeGraph::dependencies_of(TypeId const id) const -> std::span<TypeId const> {
    return type(id).dependencies;
}

auto TypeGraph::users_of(TypeId const id) const -> std::span<TypeId const> {
    return type(id).users;
}

auto resolve_type_graph(codegen::Manifest const& manifest) -> TypeGraph {
    codegen::validate_manifest(manifest);
    return TypeGraphBuilder{manifest}.build();
}

} // namespace lispb::schema
