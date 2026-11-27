from matrix import Compiler


CLANGCL = Compiler(
    name="clangcl",
    display_name="clang-cl",
    base_preset="windows-clang-cl",
    toolchain_file="${sourceDir}/cmake/toolchains/windows-clang-cl.cmake",
)
