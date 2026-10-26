"""Check optimized x86 assembly produced from packed_performance_probes.cpp.

Compile with NDEBUG, optimization enabled, and LTO disabled. Accepts Clang/GCC
Intel syntax assembly and MSVC /FA listings; instruction spelling may differ.
"""

import argparse
import re
from pathlib import Path


def body(assembly: str, name: str) -> str:
    msvc = re.search(rf"^{name}\s+PROC\b(.*?)^{name}\s+ENDP\b", assembly, re.M | re.S)
    if msvc:
        return msvc.group(1)
    start = re.search(rf"^{name}:.*$", assembly, re.M)
    if start is None:
        raise AssertionError(f"Missing probe {name}")
    remainder = assembly[start.end():]
    end = re.search(r"^\s*(?:\.size\b|\.def\b|# -- End function)", remainder, re.M)
    if end is None:
        raise AssertionError(f"Missing end of probe {name}")
    return remainder[:end.start()]


def instructions(text: str) -> list[str]:
    result = []
    for line in text.splitlines():
        line = re.split(r"[;#]", line, maxsplit=1)[0].strip()
        if re.match(r"^[a-z][a-z0-9]+\s", line) or line in ("ret", "retq"):
            result.append(line)
    return result


def vectorized_loops(text: str) -> list[list[str]]:
    labels = {}
    result = []
    lines = text.splitlines()
    vector_operation = re.compile(r"^v?(?:pand\w*|andp[sd]|psrl\w*|padd\w*)\s", re.I)
    vector_register = re.compile(r"\b(?:xmm|ymm|zmm)\d+\b", re.I)
    for index, line in enumerate(lines):
        line = re.split(r"[;#]", line, maxsplit=1)[0].strip()
        label = re.match(r"^([.$\w@?]+):", line)
        if label:
            labels[label.group(1)] = index
        branch = re.match(r"^j\w+\s+(?:(?:SHORT|NEAR)\s+)?([.$\w@?]+)\b", line, re.I)
        if branch and branch.group(1) in labels:
            loop = instructions("\n".join(lines[labels[branch.group(1)]:index]))
            packed_operation = any(vector_operation.search(op) and vector_register.search(op)
                                   for op in loop)
            vector_memory = any("[" in op and vector_register.search(op) and
                                not re.search(r"\b(?:rsp|rbp|rip)\b", op, re.I)
                                for op in loop)
            if packed_operation and vector_memory:
                result.append(loop)
    return result


def vectorized_loop(text: str) -> bool:
    return bool(vectorized_loops(text))


def check_scalar(assembly: str, name: str) -> None:
    ops = instructions(body(assembly, name))
    assert ops, f"No instructions in {name}"
    assert not any(re.match(r"(?:call\w*|j\w+)\b", op) for op in ops), (
        f"Unexpected call or branch in {name}"
    )
    assert not any(re.search(r"\b(?:rsp|rbp|esp|ebp)\b", op) or
                   re.match(r"(?:push|pop)\b", op) for op in ops), (
        f"Unexpected stack access/spill in {name}"
    )
    assert not any("[" in op and not op.startswith("lea ") and not op.startswith("lea\t")
                   for op in ops), f"Unexpected memory access in {name}"
    print(f"{name}: {len(ops)} instructions; no calls, branches, memory accesses, or spills")


def check_arrays(assembly: str, generated: str, reference: str) -> None:
    generated_body = body(assembly, generated)
    reference_body = body(assembly, reference)
    assert not any(re.match(r"call\w*\b", op) for op in instructions(generated_body)), (
        "Array loop contains a call"
    )
    reference_vectorizes = vectorized_loop(reference_body)
    generated_vectorizes = vectorized_loop(generated_body)
    assert not reference_vectorizes or generated_vectorizes, "Packed array lost vectorization"
    print(f"array SIMD: packed={generated_vectorizes}, handwritten={reference_vectorizes}")
    for name, text in (("packed", generated_body), ("handwritten", reference_body)):
        for loop in vectorized_loops(text):
            memory_ops = sum("[" in op for op in loop)
            print(f"{name} SIMD body: {len(loop)} instructions, {memory_ops} memory operands")


def check(assembly: str) -> None:
    for suffix in ("nested_index", "set_nested_index", "kill_target_index", "make_kill"):
        for prefix in ("packed", "handwritten"):
            check_scalar(assembly, f"{prefix}_{suffix}")
    check_arrays(assembly, "packed_sum_nested_indices", "handwritten_sum_nested_indices")


def compare_entity_id(baseline: str, candidate: str) -> None:
    for title, assembly in (("baseline EntityId", baseline), ("candidate EntityId", candidate)):
        print(title)
        check_scalar(assembly, "generated_index")
        check_scalar(assembly, "handwritten_index")
        check_arrays(assembly, "generated_indices", "handwritten_indices")
    if vectorized_loop(body(baseline, "generated_indices")):
        assert vectorized_loop(body(candidate, "generated_indices")), "EntityId lost baseline SIMD"


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("assembly", type=Path, nargs="*")
    parser.add_argument("--entity-id-baseline", type=Path)
    parser.add_argument("--entity-id-candidate", type=Path)
    args = parser.parse_args()
    if bool(args.entity_id_baseline) != bool(args.entity_id_candidate):
        parser.error("EntityId comparison requires both baseline and candidate")
    if not args.assembly and not args.entity_id_baseline:
        parser.error("Provide probe assembly or an EntityId comparison")
    for path in args.assembly:
        print(path)
        check(path.read_text(encoding="utf-8"))
    if args.entity_id_baseline:
        compare_entity_id(args.entity_id_baseline.read_text(encoding="utf-8"),
                          args.entity_id_candidate.read_text(encoding="utf-8"))
