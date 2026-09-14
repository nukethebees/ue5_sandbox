from compilers import CLANGCL, MSVC
from matrix import Platform


WINDOWS = Platform(
    name="win",
    display_name="Windows",
    host_system_name="Windows",
    ue_platform="Win64",
    architecture="x64",
    compilers=(CLANGCL, MSVC),
)
