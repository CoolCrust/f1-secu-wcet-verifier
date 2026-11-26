cmake_minimum_required(VERSION 3.25)
project(f1_secu_wcet_verifier VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

list(APPEND CMAKE_PREFIX_PATH "/usr/lib/llvm-22")

add_link_options(-fuse-ld=lld-22)
set_property(GLOBAL PROPERTY JOB_POOLS compile=16 link=2)
set(CMAKE_JOB_POOL_COMPILE compile)
set(CMAKE_JOB_POOL_LINK link)

set(SECU_STRICT_WARNINGS
  -Wall
  -Wextra
  -Werror
  -pedantic
  -Wshadow
  -Wconversion
  -Wsign-conversion
  -Wdouble-promotion
  -Wnull-dereference
  -Wformat=2
)

# 1. Clean 2026 F1 SECU Control Firmware Library
add_library(secu_firmware STATIC
  firmware/src/mguk_bbw_arbiter.cpp
  firmware/src/active_aero_sync.cpp
  firmware/src/telemetry_framer.cpp
)
target_include_directories(secu_firmware PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/firmware/include
)
target_compile_options(secu_firmware PRIVATE
  ${SECU_STRICT_WARNINGS}
  -fno-exceptions
  -fno-rtti
)

# 2. Faulty firmware translation units registered in compile_commands.json
add_library(secu_faulty_ast_inputs OBJECT EXCLUDE_FROM_ALL
  firmware/faulty/faulty_unbounded_loop.cpp
  firmware/faulty/faulty_heap_isr.cpp
  firmware/faulty/faulty_narrowing_and_ub.cpp
)
target_include_directories(secu_faulty_ast_inputs PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/firmware/include
)

# 3. External Dependencies: LLVM 22, Clang 22, HiGHS MILP, nlohmann_json
find_package(LLVM REQUIRED CONFIG PATHS /usr/lib/llvm-22/lib/cmake/llvm NO_DEFAULT_PATH)
find_package(Clang REQUIRED CONFIG PATHS /usr/lib/llvm-22/lib/cmake/clang NO_DEFAULT_PATH)
find_path(HIGHS_INCLUDE_DIR NAMES Highs.h PATH_SUFFIXES highs REQUIRED)
find_library(HIGHS_LIBRARY NAMES highs REQUIRED)
find_package(nlohmann_json REQUIRED)

# 4. Phase 2: Clang LibTooling MISRA C++:2023 Safety Gatekeeper
add_executable(secu_misra_gatekeeper
  tools/misra_gatekeeper.cpp
)
target_include_directories(secu_misra_gatekeeper SYSTEM PRIVATE
  ${LLVM_INCLUDE_DIRS}
  ${CLANG_INCLUDE_DIRS}
)
target_link_libraries(secu_misra_gatekeeper PRIVATE
  clang-cpp
  LLVM
  nlohmann_json::nlohmann_json
)
target_compile_options(secu_misra_gatekeeper PRIVATE ${SECU_STRICT_WARNINGS})

# 5. Phase 3: LLVM 22 IR Static WCET & Cache Analysis Pass Plugin
add_library(WCETPass MODULE
  passes/wcet_pass.cpp
)
target_include_directories(WCETPass SYSTEM PRIVATE ${LLVM_INCLUDE_DIRS})
target_link_libraries(WCETPass PRIVATE LLVM nlohmann_json::nlohmann_json)
target_compile_options(WCETPass PRIVATE ${SECU_STRICT_WARNINGS})

# 6. Phase 4: HiGHS IPET ILP WCET Solver & Rate Monotonic RTA Certifier
add_library(secu_ipet_solver STATIC
  solver/ipet_solver.cpp
)
target_include_directories(secu_ipet_solver PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/solver
)
target_include_directories(secu_ipet_solver SYSTEM PUBLIC
  ${HIGHS_INCLUDE_DIR}
)
target_link_libraries(secu_ipet_solver PUBLIC
  ${HIGHS_LIBRARY}
  nlohmann_json::nlohmann_json
)
target_compile_options(secu_ipet_solver PRIVATE ${SECU_STRICT_WARNINGS})

add_executable(secu_verify_cli
  solver/secu_verify_cli.cpp
)
add_dependencies(secu_verify_cli secu_misra_gatekeeper WCETPass)
target_link_libraries(secu_verify_cli PRIVATE secu_ipet_solver)
target_compile_options(secu_verify_cli PRIVATE ${SECU_STRICT_WARNINGS})
target_compile_definitions(secu_verify_cli PRIVATE
  SECU_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
  SECU_BUILD_DIR="${CMAKE_CURRENT_BINARY_DIR}"
  SECU_GATEKEEPER_BIN="$<TARGET_FILE:secu_misra_gatekeeper>"
  SECU_WCET_PLUGIN="$<TARGET_FILE:WCETPass>"
)

# 7. GoogleTest Verification Suites
enable_testing()
find_package(GTest REQUIRED)

add_executable(secu_firmware_tests tests/firmware_test.cpp)
target_link_libraries(secu_firmware_tests PRIVATE secu_firmware GTest::gtest_main)
target_compile_options(secu_firmware_tests PRIVATE ${SECU_STRICT_WARNINGS})

add_executable(secu_misra_tests tests/misra_gatekeeper_test.cpp)
add_dependencies(secu_misra_tests secu_misra_gatekeeper)
target_link_libraries(secu_misra_tests PRIVATE nlohmann_json::nlohmann_json GTest::gtest_main)
target_compile_options(secu_misra_tests PRIVATE ${SECU_STRICT_WARNINGS})
target_compile_definitions(secu_misra_tests PRIVATE
  GATEKEEPER_BIN_PATH="$<TARGET_FILE:secu_misra_gatekeeper>"
  BUILD_DIR_PATH="${CMAKE_CURRENT_BINARY_DIR}"
  SOURCE_DIR_PATH="${CMAKE_CURRENT_SOURCE_DIR}"
)

add_executable(secu_wcet_pass_tests tests/wcet_pass_test.cpp)
add_dependencies(secu_wcet_pass_tests WCETPass)
target_link_libraries(secu_wcet_pass_tests PRIVATE nlohmann_json::nlohmann_json GTest::gtest_main)
target_compile_options(secu_wcet_pass_tests PRIVATE ${SECU_STRICT_WARNINGS})
target_compile_definitions(secu_wcet_pass_tests PRIVATE
  WCET_PASS_PLUGIN_PATH="$<TARGET_FILE:WCETPass>"
  SOURCE_DIR_PATH="${CMAKE_CURRENT_SOURCE_DIR}"
)

add_executable(secu_ipet_rta_tests tests/ipet_rta_test.cpp)
add_dependencies(secu_ipet_rta_tests secu_verify_cli)
target_link_libraries(secu_ipet_rta_tests PRIVATE secu_ipet_solver GTest::gtest_main)
target_compile_options(secu_ipet_rta_tests PRIVATE ${SECU_STRICT_WARNINGS})
target_compile_definitions(secu_ipet_rta_tests PRIVATE
  VERIFY_CLI_BIN_PATH="$<TARGET_FILE:secu_verify_cli>"
)

include(GoogleTest)
gtest_discover_tests(secu_firmware_tests)
gtest_discover_tests(secu_misra_tests)
gtest_discover_tests(secu_wcet_pass_tests)
gtest_discover_tests(secu_ipet_rta_tests)
