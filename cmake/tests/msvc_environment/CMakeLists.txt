include("${PROJECT_SOURCE_DIR}/cmake/msvc_environment.cmake")

set(msvc_environment_text [=[noise
IOJ_MSVC_PATH=C:\VS Tools\bin;C:\Windows\System32
IOJ_MSVC_INCLUDE= C:\SDK Include\ucrt
]=])
string(REPLACE "\n" "\r\n" msvc_environment_text "${msvc_environment_text}")
sandbox_get_msvc_environment_value(msvc_path "${msvc_environment_text}" PATH)
sandbox_assert_equal("MSVC PATH retains semicolon-separated Windows paths"
  "${msvc_path}" [=[C:\VS Tools\bin;C:\Windows\System32]=])
sandbox_get_msvc_environment_value(msvc_include "${msvc_environment_text}" INCLUDE)
sandbox_assert_equal("MSVC environment values trim leading whitespace"
  "${msvc_include}" [=[C:\SDK Include\ucrt]=])
sandbox_make_msvc_library_linker_flags(msvc_linker_flags
  [=[C:\VS Tools\lib]=] [=[C:\Windows SDK\lib\x64]=])
sandbox_assert_equal("MSVC linker flags quote normalized library paths"
  "${msvc_linker_flags}"
  [=[ /LIBPATH:"C:/VS Tools/lib" /LIBPATH:"C:/Windows SDK/lib/x64"]=])

sandbox_get_clang_cl_asan_paths(asan_runtime asan_import asan_thunk
  [=[C:\LLVM Root\lib\clang\20]=])
sandbox_assert_equal("ASan runtime path" "${asan_runtime}"
  [=[C:/LLVM Root/lib/clang/20/lib/windows/clang_rt.asan_dynamic-x86_64.dll]=])
sandbox_assert_equal("ASan import library path" "${asan_import}"
  [=[C:/LLVM Root/lib/clang/20/lib/windows/clang_rt.asan_dynamic-x86_64.lib]=])
sandbox_assert_equal("ASan thunk path" "${asan_thunk}"
  [=[C:/LLVM Root/lib/clang/20/lib/windows/clang_rt.asan_dynamic_runtime_thunk-x86_64.lib]=])
