from __future__ import annotations

from Codegen.cpp import (
    CppFile,
    IncludeDependencies,
    Module,
    Namespace,
    NewLines,
    TypeDependency,
)
from Codegen.soa import (
    FixedSoAConfig,
    FixedSoAContainer,
    SoAStruct,
    SoAStructNames,
    lower_soa_structs_with_source,
    soa_member,
    tarray_member,
)

from Codegen.manifests.common import (
    INCLUDE_ORDER,
    SANDBOX_CORE_TEST_PRIVATE_DIR,
    soa_source_file,
)


def fixed_soa_test_types_module() -> Module:
    string_type = TypeDependency("FString", "CoreMinimal.h")
    shared_ptr_type = TypeDependency("TSharedPtr<int32>", "Templates/SharedPointer.h")
    child = SoAStruct(
        SoAStructNames("FTestFixedChild"),
        (
            tarray_member("names", string_type),
            tarray_member("references", shared_ptr_type),
        ),
        fixed=FixedSoAConfig("TTestFixedChildStorage"),
    )
    rows = SoAStruct(
        SoAStructNames("FTestFixedRows"),
        (
            soa_member("children", "FTestFixedChild", fixed_schema=child),
            tarray_member("ids", "int32"),
        ),
        fixed=FixedSoAConfig(
            "TTestFixedRowsStorage",
            (
                FixedSoAContainer("TTestFixedRowsArray"),
                FixedSoAContainer("TTestFixedRowsArrayAlternate"),
            ),
        ),
    )
    lowered = lower_soa_structs_with_source((child, rows))
    header_path = SANDBOX_CORE_TEST_PRIVATE_DIR / "fixed_soa_test_types.h"
    return Module(
        name="fixed_soa_test_types",
        header=CppFile(
            path=header_path,
            clang_format_off=True,
            include_order=INCLUDE_ORDER,
            nodes=(
                IncludeDependencies(),
                NewLines(2),
                Namespace("ml::fixed_soa_tests", lowered.header_nodes),
            ),
        ),
        source=soa_source_file(
            header_path, lowered.source_nodes, "ml::fixed_soa_tests"
        ),
    )
