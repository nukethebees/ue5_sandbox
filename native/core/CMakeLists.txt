find_package(GTest CONFIG REQUIRED)

if(WIN32 AND
   NOT CMAKE_CXX_COMPILER_ID STREQUAL "MSVC" AND
   NOT CMAKE_CXX_SIMULATE_ID STREQUAL "MSVC")
  message(FATAL_ERROR "native-core requires an MSVC-compatible ABI on Windows.")
endif()

add_library(native-core STATIC)
target_sources(native-core PRIVATE
  src/generated/array_math_kernels.cpp
  src/array_math.cpp
  src/array_utils.cpp
  src/projectile_intercept.cpp
  src/rotation.cpp
  src/test_timeline.cpp
)
target_compile_features(native-core PUBLIC cxx_std_23)
target_include_directories(native-core PUBLIC include)
target_link_libraries(native-core PRIVATE sandbox::warnings_as_errors)

set(NATIVE_CORE_LIBRARY_DIRECTORY
  "${PROJECT_SOURCE_DIR}/Binaries/Native/Core/${UE_PLATFORM}/${UE_CONFIGURATION}"
)
set_target_properties(native-core PROPERTIES
  OUTPUT_NAME SandboxNativeCore
  ARCHIVE_OUTPUT_DIRECTORY "${NATIVE_CORE_LIBRARY_DIRECTORY}"
)

if(WIN32 AND DEFINED VCPKG_APPLOCAL_DEPS)
  set(NATIVE_CORE_SAVED_VCPKG_APPLOCAL_DEPS "${VCPKG_APPLOCAL_DEPS}")
  set(VCPKG_APPLOCAL_DEPS OFF)
endif()

add_executable(native-core-tests)

if(WIN32 AND DEFINED NATIVE_CORE_SAVED_VCPKG_APPLOCAL_DEPS)
  set(VCPKG_APPLOCAL_DEPS "${NATIVE_CORE_SAVED_VCPKG_APPLOCAL_DEPS}")
endif()

target_sources(native-core-tests PRIVATE
  tests/generated/array_math_kernels_tests.cpp
  tests/array_utils_tests.cpp
  tests/container_tests.cpp
  tests/interpolation_tests.cpp
  tests/invoke_tests.cpp
  tests/lock_free_mpsc_queue_tests.cpp
  tests/loop_bounds_tests.cpp
  tests/projectile_intercept_tests.cpp
  tests/quantized_decimal_tests.cpp
  tests/rotation_tests.cpp
  tests/static_string_view_tests.cpp
  tests/timing_tests.cpp
  tests/test_timeline_tests.cpp
  tests/vector_math_tests.cpp
)
target_compile_features(native-core-tests PRIVATE cxx_std_23)
target_link_libraries(native-core-tests PRIVATE native-core sandbox::warnings_as_errors)

if(WIN32)
  get_target_property(NATIVE_CORE_GTEST_INCLUDE_DIRECTORIES
    GTest::gtest INTERFACE_INCLUDE_DIRECTORIES)
  get_target_property(NATIVE_CORE_GTEST_LIBRARY GTest::gtest IMPORTED_IMPLIB_RELEASE)
  get_target_property(NATIVE_CORE_GTEST_MAIN_LIBRARY GTest::gtest_main IMPORTED_IMPLIB_RELEASE)
  get_target_property(NATIVE_CORE_GTEST_DLL GTest::gtest IMPORTED_LOCATION_RELEASE)
  get_target_property(NATIVE_CORE_GTEST_MAIN_DLL GTest::gtest_main IMPORTED_LOCATION_RELEASE)

  foreach(required_property IN ITEMS
      NATIVE_CORE_GTEST_INCLUDE_DIRECTORIES
      NATIVE_CORE_GTEST_LIBRARY
      NATIVE_CORE_GTEST_MAIN_LIBRARY
      NATIVE_CORE_GTEST_DLL
      NATIVE_CORE_GTEST_MAIN_DLL)
    if(NOT DEFINED ${required_property} OR "${${required_property}}" MATCHES "-NOTFOUND$")
      message(FATAL_ERROR "GTest does not provide ${required_property} for the release CRT.")
    endif()
  endforeach()

  target_include_directories(native-core-tests SYSTEM PRIVATE
    ${NATIVE_CORE_GTEST_INCLUDE_DIRECTORIES})
  target_link_libraries(native-core-tests PRIVATE
    "${NATIVE_CORE_GTEST_MAIN_LIBRARY}"
    "${NATIVE_CORE_GTEST_LIBRARY}"
  )
  add_custom_command(TARGET native-core-tests POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
      "${NATIVE_CORE_GTEST_DLL}"
      "${NATIVE_CORE_GTEST_MAIN_DLL}"
      "$<TARGET_FILE_DIR:native-core-tests>"
    VERBATIM
  )
else()
  target_link_libraries(native-core-tests PRIVATE GTest::gtest_main)
endif()

include(GoogleTest)
gtest_discover_tests(native-core-tests PROPERTIES LABELS "unit;all;native-core")

add_library(native-core-static-string-view-reject-automatic OBJECT EXCLUDE_FROM_ALL)
target_sources(native-core-static-string-view-reject-automatic PRIVATE
  tests/static_string_view_reject_automatic_storage.cpp
)
target_compile_features(native-core-static-string-view-reject-automatic PRIVATE cxx_std_23)
target_link_libraries(native-core-static-string-view-reject-automatic PRIVATE
  native-core
  sandbox::warnings_as_errors
)
add_test(NAME StaticStringViewRejectsAutomaticStorage
  COMMAND "${CMAKE_COMMAND}" --build "${CMAKE_BINARY_DIR}"
    --target native-core-static-string-view-reject-automatic
)
set_tests_properties(StaticStringViewRejectsAutomaticStorage PROPERTIES
  LABELS "unit;all;native-core"
  WILL_FAIL TRUE
)

set_property(TARGET native-core PROPERTY MSVC_RUNTIME_LIBRARY MultiThreadedDLL)
target_compile_definitions(native-core PRIVATE _ITERATOR_DEBUG_LEVEL=0)

if(WIN32)
  set_property(TARGET native-core-tests PROPERTY MSVC_RUNTIME_LIBRARY MultiThreadedDLL)
  target_compile_definitions(native-core-tests PRIVATE _ITERATOR_DEBUG_LEVEL=0)
endif()

if(SANDBOX_MSVC_FRONTEND)
  target_compile_options(native-core PRIVATE /permissive-)
  if(UE_CONFIGURATION MATCHES "^(Development|Shipping|Test)$")
    target_compile_options(native-core PRIVATE /O2)
  endif()
else()
  if(UE_CONFIGURATION MATCHES "^(Development|Shipping|Test)$")
    target_compile_options(native-core PRIVATE -O2)
  endif()
endif()

if(UE_CONFIGURATION MATCHES "^(Shipping|Test)$")
  target_compile_definitions(native-core PRIVATE NDEBUG)
endif()

if(SANDBOX_MSVC_FRONTEND)
  target_compile_options(native-core-tests PRIVATE /permissive-)
endif()
