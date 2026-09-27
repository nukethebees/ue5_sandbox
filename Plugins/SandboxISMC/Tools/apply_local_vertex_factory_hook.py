"""Install the small optional instance-input hook in an owned UE 5.8 checkout.

No engine shader is distributed here. Existing declarations and default decoding
stay in the engine file, unchanged. Unexpected engine revisions fail before write.
"""

import argparse
from pathlib import Path
import re


def patched(source: str) -> str:
    marker = "#define LOCALVF_INSTANCE_INPUT_HOOK_VERSION 1"
    if marker in source:
        return source
    declaration = re.compile(
        r"(?m)^\tfloat4 InstanceOrigin : ATTRIBUTE8;[^\n]*\n"
        r"\thalf4 InstanceTransform1 : ATTRIBUTE9;[^\n]*\n"
        r"\thalf4 InstanceTransform2 : ATTRIBUTE10;[^\n]*\n"
        r"\thalf4 InstanceTransform3 : ATTRIBUTE11;[^\n]*\n"
    )
    result, count = declaration.subn(
        lambda match: "#ifdef LOCALVF_INSTANCE_ATTRIBUTES\n"
        "\tLOCALVF_INSTANCE_ATTRIBUTES\n#else\n" + match[0] + "#endif\n",
        source,
    )
    if count != 3:
        raise ValueError(f"Expected three LocalVF attribute blocks, found {count}")
    start = "#define LOCALVF_GET_INSTANCE_INPUT(VFInput) MakeInstanceInput("
    end = "#endif // MANUAL_VERTEX_FETCH"
    if result.count(start) != 1:
        raise ValueError("Expected one default instance decoder")
    begin = result.index(start)
    finish = result.index(end, begin)
    result = (
        result[:begin]
        + '#if LOCALVF_CUSTOM_INSTANCE_INPUT\n'
        + '#include "/Engine/Generated/LocalVFCustomInstanceInput.ush"\n'
        + '#else\n'
        + result[begin:finish]
        + '#endif // LOCALVF_CUSTOM_INSTANCE_INPUT\n\n'
        + result[finish:]
    )
    return marker + "\n" + result


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("engine_root", type=Path)
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()
    path = args.engine_root / "Engine/Shaders/Private/LocalVertexFactory.ush"
    original = path.read_text(encoding="utf-8")
    updated = patched(original)
    if args.check:
        if original != updated:
            raise SystemExit("SandboxISMC LocalVF hook is missing; run this script without --check")
        print("SandboxISMC LocalVF hook is installed")
    elif original != updated:
        path.write_text(updated, encoding="utf-8", newline="\n")
        print(f"Installed SandboxISMC LocalVF hook: {path}")
    else:
        print("SandboxISMC LocalVF hook is already installed")
