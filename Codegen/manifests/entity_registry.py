from __future__ import annotations

from Codegen.cpp import (
    CppFile,
    FunctionParameter,
    IncludeDependencies,
    MemberFunctionSpec,
    Module,
    Namespace,
    NewLines,
    Raw,
    UsingDeclaration,
)
from Codegen.project_types import (
    E_TEST_DEATH_REASON,
    E_TEST_ENTITY_TYPE,
    E_TEST_TEAM,
    F_REGISTRY_ENTITY_HANDLE,
    F_REGISTRY_ENTITY_HANDLE_ARRAY,
    F_VECTORS_3F,
    TEST_ENTITY_UNIQUE_ID,
    nested_type,
    qualified_type,
)
from Codegen.soa import (
    ForEachSoAMemberCall,
    SoAStruct,
    SoAStructNames,
    lower_soa_structs_with_source,
    soa_member,
    tarray_member,
)

from Codegen.manifests.common import (
    ALL_STORAGE_OPERATIONS,
    ARRAY_FILL,
    INCLUDE_ORDER,
    SANDBOX_API,
    SOA_VECTOR_FILL,
    TEST_ENTITY_REGISTRY_DIR,
    soa_source_file,
)


def collision_damage_events_soa_module() -> Module:
    unresolved = SoAStruct(
        SoAStructNames("UnresolvedCollisionDamageEvents"),
        (
            tarray_member("damaged_actors", "AActor*"),
            tarray_member("damage_amounts", "int32"),
            tarray_member("actor_components", "UActorComponent*"),
            tarray_member("hit_items", "int32"),
            tarray_member("instigators", F_REGISTRY_ENTITY_HANDLE),
        ),
        storage_operations=ALL_STORAGE_OPERATIONS,
        storage_export_specifier=SANDBOX_API,
    )
    resolved = SoAStruct(
        SoAStructNames("CollisionDamageEvents"),
        (
            tarray_member("damage_amounts", "int32"),
            tarray_member("actor_components", "UActorComponent*"),
            tarray_member("hit_items", "int32"),
            tarray_member("instigators", F_REGISTRY_ENTITY_HANDLE),
        ),
        storage_operations=ALL_STORAGE_OPERATIONS,
        storage_export_specifier=SANDBOX_API,
    )
    lowered = lower_soa_structs_with_source((unresolved, resolved))
    header_path = TEST_ENTITY_REGISTRY_DIR / "CollisionDamageEventsSoA.h"
    return Module(
        name="collision_damage_events_soa",
        header=CppFile(
            path=header_path,
            clang_format_off=True,
            include_order=INCLUDE_ORDER,
            nodes=(
                IncludeDependencies(),
                NewLines(2),
                Raw("class AActor;\nclass UActorComponent;"),
                NewLines(2),
                *lowered.header_nodes,
            ),
        ),
        source=soa_source_file(header_path, lowered.source_nodes),
    )


def entity_death_info_module() -> Module:
    entity_death_info_members = (
        tarray_member("reasons", E_TEST_DEATH_REASON),
        tarray_member("victims", F_REGISTRY_ENTITY_HANDLE),
        tarray_member("killers", F_REGISTRY_ENTITY_HANDLE),
    )
    add_function = MemberFunctionSpec(
        "add",
        "void",
        (
            FunctionParameter(qualified_type(E_TEST_DEATH_REASON, " const"), "reason"),
            FunctionParameter(qualified_type(F_REGISTRY_ENTITY_HANDLE, " const"), "victim"),
            FunctionParameter(qualified_type(F_REGISTRY_ENTITY_HANDLE, " const"), "killer"),
        ),
        ForEachSoAMemberCall(entity_death_info_members, "Add"),
    )
    add_without_killer = MemberFunctionSpec(
        "add",
        "void",
        (
            FunctionParameter(qualified_type(E_TEST_DEATH_REASON, " const"), "reason"),
            FunctionParameter(qualified_type(F_REGISTRY_ENTITY_HANDLE, " const"), "victim"),
        ),
        Raw("add(reason, victim, FRegistryEntityHandle{});"),
        is_inline=True,
    )
    entity_death_info = SoAStruct(
        SoAStructNames("EntityDeathInfo"),
        entity_death_info_members,
        storage_operations=ALL_STORAGE_OPERATIONS,
        storage_export_specifier=SANDBOX_API,
        nodes=(
            add_function.declaration_node(),
            NewLines(1),
            add_without_killer.header_node(),
        ),
    )
    lowered = lower_soa_structs_with_source((entity_death_info,))
    header_path = TEST_ENTITY_REGISTRY_DIR / "EntityDeathInfo.h"
    return Module(
        name="entity_death_info",
        header=CppFile(
            path=header_path,
            clang_format_off=True,
            include_order=INCLUDE_ORDER,
            nodes=(
                IncludeDependencies(),
                NewLines(2),
                *lowered.header_nodes,
            ),
        ),
        source=soa_source_file(
            header_path,
            (
                *lowered.source_nodes,
                NewLines(2),
                add_function.definition_node("EntityDeathInfo"),
            ),
        ),
    )


def direct_damage_events_soa_module() -> Module:
    events = SoAStruct(
        SoAStructNames("DirectDamageEvents"),
        (
            tarray_member("damaged_entities", F_REGISTRY_ENTITY_HANDLE),
            tarray_member("damage_amounts", "int32"),
            tarray_member("instigators", F_REGISTRY_ENTITY_HANDLE),
        ),
        storage_operations=ALL_STORAGE_OPERATIONS,
        storage_export_specifier=SANDBOX_API,
    )
    lowered = lower_soa_structs_with_source((events,))
    header_path = TEST_ENTITY_REGISTRY_DIR / "DirectDamageEventsSoA.h"
    return Module(
        name="direct_damage_events_soa",
        header=CppFile(
            path=header_path,
            clang_format_off=True,
            include_order=INCLUDE_ORDER,
            nodes=(
                IncludeDependencies(),
                NewLines(2),
                *lowered.header_nodes,
            ),
        ),
        source=soa_source_file(header_path, lowered.source_nodes),
    )


def unique_entity_data_soa_module() -> Module:
    entity_data = SoAStruct(
        SoAStructNames("TestEntityUniqueEntityData"),
        (
            tarray_member("registry_indices", nested_type(F_REGISTRY_ENTITY_HANDLE, "index_type")),
            tarray_member(
                "registry_generations", nested_type(F_REGISTRY_ENTITY_HANDLE, "generation_type")
            ),
            tarray_member("entity_types", E_TEST_ENTITY_TYPE),
            tarray_member("teams", E_TEST_TEAM),
            tarray_member("kills", "uint32"),
            tarray_member("alive", "uint8"),
            tarray_member("killed_by", TEST_ENTITY_UNIQUE_ID),
            tarray_member("death_reason", E_TEST_DEATH_REASON),
        ),
        storage_operations=ALL_STORAGE_OPERATIONS,
        storage_export_specifier=SANDBOX_API,
        nodes=(UsingDeclaration("kills_type", "uint32"),),
    )
    lowered = lower_soa_structs_with_source((entity_data,))
    header_path = TEST_ENTITY_REGISTRY_DIR / "TestEntityUniqueEntityDataSoA.h"
    return Module(
        name="test_entity_unique_entity_data_soa",
        header=CppFile(
            path=header_path,
            clang_format_off=True,
            include_order=INCLUDE_ORDER,
            nodes=(
                IncludeDependencies(),
                NewLines(2),
                *lowered.header_nodes,
            ),
        ),
        source=soa_source_file(header_path, lowered.source_nodes),
    )


def entity_registry_data_soa_module() -> Module:
    add_disabled = MemberFunctionSpec(
        "add_disabled",
        "void",
        (FunctionParameter("int32 const", "count"),),
        Raw(
            "add_uninitialised(count);\n"
            "auto view{get_view()};\n"
            "auto slice{view.right(count)};\n\n"
            "ml::fill(slice.locations, 0.f);\n"
            "ml::fill(slice.velocities, 0.f);\n"
            "ml::fill(slice.radii, 0.f);\n"
            "ml::fill(slice.healths, 0);\n"
            "ml::fill(slice.teams, ETestTeam::White);\n"
            "ml::fill(slice.entity_types, ETestEntityType::COUNT);\n"
            "ml::fill(slice.alive, uint8{0u});",
            (ARRAY_FILL, SOA_VECTOR_FILL),
        ),
    )
    set_all_alive = MemberFunctionSpec(
        "set_all_alive",
        "void",
        (),
        Raw("ml::fill(alive, uint8{1});", (ARRAY_FILL,)),
    )
    set_all_velocities = MemberFunctionSpec(
        "set_all_velocities",
        "void",
        (FunctionParameter("float const", "value"),),
        Raw("ml::fill(velocities, value);", (SOA_VECTOR_FILL,)),
    )
    set_all_entity_types = MemberFunctionSpec(
        "set_all_entity_types",
        "void",
        (FunctionParameter(qualified_type(E_TEST_ENTITY_TYPE, " const"), "value"),),
        Raw("ml::fill(entity_types, value);", (ARRAY_FILL,)),
    )
    custom_functions = (
        add_disabled,
        set_all_alive,
        set_all_velocities,
        set_all_entity_types,
    )
    entity_data = SoAStruct(
        SoAStructNames("EntityData"),
        (
            soa_member("locations", F_VECTORS_3F),
            soa_member("velocities", F_VECTORS_3F),
            tarray_member("radii", "float"),
            tarray_member("healths", "int32"),
            tarray_member("teams", E_TEST_TEAM),
            tarray_member("entity_types", E_TEST_ENTITY_TYPE),
            tarray_member("alive", "uint8"),
        ),
        storage_operations=ALL_STORAGE_OPERATIONS,
        nodes=tuple(function.header_node() for function in custom_functions),
        source_nodes=tuple(
            function.definition_node("EntityData") for function in custom_functions
        ),
    )
    lowered = lower_soa_structs_with_source((entity_data,))
    header_path = TEST_ENTITY_REGISTRY_DIR / "TestEntityRegistryData.h"
    return Module(
        name="test_entity_registry_data_soa",
        header=CppFile(
            path=header_path,
            clang_format_off=True,
            include_order=INCLUDE_ORDER,
            nodes=(
                IncludeDependencies(),
                NewLines(2),
                Namespace("ml::entity_registry", lowered.header_nodes),
            ),
        ),
        source=soa_source_file(header_path, lowered.source_nodes, "ml::entity_registry"),
    )


def registry_entity_handles_soa_module() -> Module:
    add = MemberFunctionSpec(
        "add",
        "void",
        (
            FunctionParameter("int32 const", "index"),
            FunctionParameter("int32 const", "generation"),
        ),
        Raw("registry_indices.Add(index);\ngenerations.Add(generation);"),
    )
    append_to = MemberFunctionSpec(
        "append_to",
        "void",
        (FunctionParameter(qualified_type(F_REGISTRY_ENTITY_HANDLE_ARRAY, "&"), "out"),),
        Raw(
            "auto const count{num()};\n"
            "out.Reserve(out.Num() + count);\n"
            "for (int32 i{0}; i < count; ++i) {\n"
            "    out.Emplace(registry_indices[i], generations[i]);\n"
            "}"
        ),
        suffix=" const",
    )
    to_array = MemberFunctionSpec(
        "to_array",
        F_REGISTRY_ENTITY_HANDLE_ARRAY,
        (),
        Raw(
            "TArray<FRegistryEntityHandle> out;\n"
            "append_to(out);\n"
            "return out;"
        ),
        suffix=" const",
    )
    handles = SoAStruct(
        SoAStructNames("FRegistryEntityHandles"),
        (
            tarray_member("registry_indices", "int32"),
            tarray_member("generations", "int32"),
        ),
        storage_operations=ALL_STORAGE_OPERATIONS,
        storage_export_specifier=SANDBOX_API,
        nodes=(
            add.header_node(),
            NewLines(1),
            append_to.header_node(),
            NewLines(1),
            to_array.header_node(),
        ),
        source_nodes=(
            add.definition_node("FRegistryEntityHandles"),
            NewLines(2),
            append_to.definition_node("FRegistryEntityHandles"),
            NewLines(2),
            to_array.definition_node("FRegistryEntityHandles"),
        ),
        equivalent_type=F_REGISTRY_ENTITY_HANDLE,
    )
    lowered = lower_soa_structs_with_source((handles,))
    header_path = TEST_ENTITY_REGISTRY_DIR / "RegistryEntityHandles.h"
    return Module(
        name="registry_entity_handles_soa",
        header=CppFile(
            path=header_path,
            clang_format_off=True,
            include_order=INCLUDE_ORDER,
            nodes=(IncludeDependencies(), NewLines(2), *lowered.header_nodes),
        ),
        source=soa_source_file(header_path, lowered.source_nodes),
    )
