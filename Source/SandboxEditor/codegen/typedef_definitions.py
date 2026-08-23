"""
Strong typedef definitions for the Sandbox project.
Add new TypedefSpec entries to TYPEDEFS array to generate wrapper structs.
"""

from strong_typedefs import TypedefSpec, ConfigOptions

wrappers: dict[str, TypedefSpec] = {}
wrappers["FIntPoint"] = TypedefSpec(
    name="",
    underlying_type="FIntPoint",
    ops=[],
    members=[],
    config=ConfigOptions(),
)

def make_typedefs() -> list[TypedefSpec]:
    typedefs = [
        TypedefSpec(
            name="StackSize",
            underlying_type="int32",
            ops=["comparison", "arithmetic", "increment", "modulo"],
            members=[],
            config=ConfigOptions(),
        ),
        TypedefSpec(
            name="BulletTypeIndex",
            underlying_type="int32",
            ops=["comparison"],
            members=[],
            config=ConfigOptions(),
        ),
        TypedefSpec(
            name="NdcWriterIndex",
            underlying_type="int32",
            ops=["comparison"],
            members=[],
            config=ConfigOptions(),
        )
    ]

    return typedefs

TYPEDEFS = make_typedefs()
