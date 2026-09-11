"""Read-only checks for the authoritative simulation module boundary."""

from pathlib import Path
import re
import sys


SIMULATION = "SpaceGameSimulation"
ALLOWED_DEPENDENCIES = {
    "Core", "CoreUObject", "Engine", "TraceLog", "NativeMemory",
    "SandboxCore", "SandboxCoreEngine", "SandboxNative", "SGCollision",
}
FORBIDDEN = re.compile(
    r"presentation|SpaceGameRendering|Niagara|SandboxISMC|SandboxUI|"
    r"InstancedStaticMeshComponent|UserWidget|VisualLogger|DrawDebug|"
    r"\b(?:UMG|Slate|SlateCore|CommonUI)\b|ATestBatchOrchestrator|"
    r"[#]\s*include\s*[<\"](?:SpaceGame|SandboxGameShared)/",
    re.IGNORECASE,
)


def dependencies(text: str) -> set[str]:
    text = re.sub(r"//[^\n]*|/\*.*?\*/", "", text, flags=re.DOTALL)
    blocks = re.findall(
        r"(?:Public|Private)DependencyModuleNames\.Add(?:Range)?\s*\((.*?)\);",
        text, flags=re.DOTALL,
    )
    return {name for block in blocks for name in re.findall(r'"([\w]+)"', block)}


def check(root: Path) -> list[str]:
    errors: list[str] = []
    source = root / "Plugins/SpaceGame/Source"
    simulation = source / SIMULATION
    modules = {
        path.name.removesuffix(".Build.cs"): path
        for directory in (root / "Plugins", root / "Source")
        for path in directory.rglob("*.Build.cs")
    }
    if SIMULATION not in modules:
        errors.append("SpaceGameSimulation runtime module is missing")
    pending = [SIMULATION]
    visited: set[str] = set()
    while pending:
        module = pending.pop()
        if module in visited or module not in modules:
            continue
        visited.add(module)
        for dependency in dependencies(modules[module].read_text(encoding="utf-8-sig")):
            if dependency not in ALLOWED_DEPENDENCIES:
                errors.append(f"{module} has forbidden simulation dependency {dependency}")
            pending.append(dependency)
    for path in simulation.rglob("*"):
        if path.suffix not in {".h", ".cpp", ".inl"}:
            continue
        for line_number, line in enumerate(path.read_text(encoding="utf-8-sig").splitlines(), 1):
            if FORBIDDEN.search(line):
                errors.append(f"{path.relative_to(root)}:{line_number}: forbidden layer reference")
    for path in (source / "SpaceGamePresentation").rglob("*"):
        if path.suffix not in {".h", ".cpp", ".inl"}:
            continue
        text = path.read_text(encoding="utf-8-sig")
        if re.search(r"\b(?:FLevelSimulation|Simulation)\s*[*&]|\bconst_cast\s*<", text):
            errors.append(f"{path.relative_to(root)}: mutable simulation access")
    presentation = source / "SpaceGamePresentation/SpaceGamePresentation.Build.cs"
    presentation_dependencies = dependencies(presentation.read_text(encoding="utf-8-sig"))
    if "SpaceGame" in presentation_dependencies or SIMULATION not in presentation_dependencies:
        errors.append("Presentation must depend on Simulation, never on SpaceGame")
    composition = source / "SpaceGame/SpaceGame.Build.cs"
    if not {SIMULATION, "SpaceGamePresentation"} <= dependencies(composition.read_text(encoding="utf-8-sig")):
        errors.append("SpaceGame must compose both layers")
    return errors


if __name__ == "__main__":
    failures = check(Path(__file__).resolve().parents[1])
    print("\n".join(failures) if failures else "SpaceGame module boundaries are valid.")
    sys.exit(bool(failures))
