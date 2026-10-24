set(EXPECTED_DIAGNOSTIC "No S7 TypeReader specialization for this destination type")
include("${CMAKE_CURRENT_LIST_DIR}/../../../cmake/expect_compile_failure.cmake")
