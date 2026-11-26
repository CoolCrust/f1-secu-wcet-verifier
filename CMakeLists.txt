cmake_minimum_required(VERSION 3.25)
project(f1_secu_wcet_verifier VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

list(APPEND CMAKE_PREFIX_PATH "/usr/lib/llvm-22")

# Use lld and cap concurrent link jobs to avoid OOM on 7.4 GiB WSL2 RAM
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

# 3. Phase 2: Clang LibTooling MISRA C++:2023 Safety Gatekeeper
find_package(LLVM REQUIRED CONFIG PATHS /usr/lib/llvm-22/lib/cmake/llvm NO_DEFAULT_PATH)
find_package(Clang REQUIRED CONFIG PATHS /usr/lib/llvm-22/lib/cmake/clang NO_DEFAULT_PATH)
find_package(nlohmann_json REQUIRED)

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
target_compile_options(secu_misra_gatekeeper PRIVATE
  ${SECU_STRICT_WARNINGS}
)

# 4. GoogleTest Verification Suites
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

include(GoogleTest)
gtest_discover_tests(secu_firmware_tests)
gtest_discover_tests(secu_misra_tests)
