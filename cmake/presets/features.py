from matrix import Feature


ASAN = Feature(
    name="asan",
    display_name="ASan",
    cache_variables={"SANDBOX_WITH_ASAN": True},
    excluded_tests=(
        "Ast.RejectsInvalidFunctionQualifierCombinations",
        "Generator.PreservesDestinationWhenAtomicReplacementFails",
        "SourceLoader.ReportsSourceLocationForUnknownProperties",
        "SlateCompiler.RejectsDuplicateOwnersAcrossInputs",
        "SlateCompiler.ReportsMissingIncludesAndCycles",
        "SlateCompiler.RejectsInvalidMacroDeclarationsAndInvocations",
        "SlateCompiler.ReportsMacroDefinitionAndInvocationForSemanticErrors",
        "SlateCompiler.RestrictsIncludedFilesAndIsolatesMacrosBetweenInputs",
        "SlateCompiler.ExpansionCanInspectSemanticallyInvalidTrees",
        "SlateDsl.LibrariesRejectHostDependentForms",
        "SlateDsl.RejectsRepeatedSingleUseParameters",
        "SlateDsl.RejectsUndeclaredAndIncorrectParameterUses",
        "SlateDsl.RejectsUnusedAndDuplicateParameters",
        "SlateExpandCli",
        "KernelParser.RejectsUnknownExpressionOperatorWithLocation",
    ),
)

UNITY = Feature(
    name="unity",
    display_name="Unity",
    cache_variables={
        "CMAKE_UNITY_BUILD": True,
        "CMAKE_UNITY_BUILD_BATCH_SIZE": "32",
    },
)

# This order is also the canonical order of optional suffixes in preset names.
FEATURES = (ASAN, UNITY)


def environment_for(feature_names: set[str]) -> dict[str, str]:
    if {"asan", "unity"}.issubset(feature_names):
        return {"ASAN_OPTIONS": "detect_odr_violation=0"}
    return {}
