cmake_minimum_required(VERSION 3.13)
project(llvm_obfuscation)

find_package(LLVM 14 REQUIRED CONFIG)
message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_BUILD_TYPE Release)

include_directories(${LLVM_INCLUDE_DIRS})
add_definitions(${LLVM_DEFINITIONS})

add_library(ObfPasses MODULE
    src/passes/StringObfPass.cpp
    src/passes/BogusInsertPass.cpp
    src/passes/ControlFlowFlatteningPass.cpp
)

if(WIN32)
  set(PLUG_SUFFIX ".dll")
elseif(APPLE)
  set(PLUG_SUFFIX ".dylib")
else()
  set(PLUG_SUFFIX ".so")
endif()

set_target_properties(ObfPasses PROPERTIES
    PREFIX ""
    OUTPUT_NAME "libObfPasses"
    SUFFIX ${PLUG_SUFFIX}
)

llvm_map_components_to_libnames(llvm_libs support core irreader passes analysis)
# Do not link LLVM libraries into the plugin shared object. When building a
# loadable plugin for opt we should rely on the host `opt` process to provide
# the LLVM symbols (avoids duplicate static initializers and CommandLine
# option registration errors). Leave the target with undefined references so
# the symbols are resolved at runtime by opt.
# target_link_libraries(ObfPasses PRIVATE ${llvm_libs})

# Small CLI wrapper that invokes 'opt' with the plugin. This keeps the
# executable small and easy to build for multiple platforms (it doesn't
# link LLVM directly — it shells out to the opt tool).
add_executable(obfuscator tools/obfus_cli.cpp)
set_target_properties(obfuscator PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tools
)
## Produce a single user-facing executable named LLVM_OBFSCALTION.exe (filename only)
set_target_properties(obfuscator PROPERTIES OUTPUT_NAME "LLVM_OBFSCALTION.exe")
set_target_properties(obfuscator PROPERTIES ENABLE_EXPORTS ON)
llvm_map_components_to_libnames(obf_libs support core irreader passes analysis)
target_link_libraries(obfuscator PRIVATE ${obf_libs})

add_executable(run_cff tools/run_cff.cpp)
set_target_properties(run_cff PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tools
)
# Ensure the run_cff executable exports its symbols so dlopen'd plugins can
# resolve references against the executable (useful when LLVM is linked
# statically into this runner). ENABLE_EXPORTS sets the proper linker flag
# on platforms that support it (e.g., -Wl,--export-dynamic on Linux).
set_target_properties(run_cff PROPERTIES ENABLE_EXPORTS ON)
llvm_map_components_to_libnames(run_cff_libs support core irreader passes analysis)
target_link_libraries(run_cff PRIVATE ${run_cff_libs})

enable_testing()

# Simple test that runs the programmatic runner against the sample bitcode
add_test(NAME run_cff_test
         COMMAND ${CMAKE_BINARY_DIR}/tools/run_cff ${CMAKE_SOURCE_DIR}/tests/cff_test.bc)
set_tests_properties(run_cff_test PROPERTIES ENVIRONMENT "RUN_CFF_PLUGIN=${CMAKE_BINARY_DIR}/libObfPasses.so")

# Test the opt-based wrapper if opt is present; obfuscator will return non-zero if opt fails
add_test(NAME obfuscator_opt_test
         COMMAND ${CMAKE_BINARY_DIR}/tools/obfuscator -in ${CMAKE_SOURCE_DIR}/tests/cff_test.bc -out ${CMAKE_BINARY_DIR}/tests/cff_test.out.bc -pass cff -p ${CMAKE_BINARY_DIR}/libObfPasses.so)

# Package target: copy the main exe and plugin into build/dist for easy distribution
add_custom_target(package_llvm_obfuscation ALL
  COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/dist
  COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_BINARY_DIR}/tools/LLVM_OBFSCALTION.exe ${CMAKE_BINARY_DIR}/dist/LLVM_OBFSCALTION.exe
  COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_BINARY_DIR}/libObfPasses.so ${CMAKE_BINARY_DIR}/dist/libObfPasses.so
  COMMENT "Packaging LLVM_OBFSCALTION.exe and plugin into build/dist"
)

