# CMake generated Testfile for 
# Source directory: /home/aditya/llvm_obfuscation
# Build directory: /home/aditya/llvm_obfuscation/build
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test(run_cff_test "/home/aditya/llvm_obfuscation/build/tools/run_cff" "/home/aditya/llvm_obfuscation/tests/cff_test.bc")
set_tests_properties(run_cff_test PROPERTIES  ENVIRONMENT "RUN_CFF_PLUGIN=/home/aditya/llvm_obfuscation/build/libObfPasses.so" _BACKTRACE_TRIPLES "/home/aditya/llvm_obfuscation/CMakeLists.txt;77;add_test;/home/aditya/llvm_obfuscation/CMakeLists.txt;0;")
add_test(obfuscator_opt_test "/home/aditya/llvm_obfuscation/build/tools/obfuscator" "-in" "/home/aditya/llvm_obfuscation/tests/cff_test.bc" "-out" "/home/aditya/llvm_obfuscation/build/tests/cff_test.out.bc" "-pass" "cff" "-p" "/home/aditya/llvm_obfuscation/build/libObfPasses.so")
set_tests_properties(obfuscator_opt_test PROPERTIES  _BACKTRACE_TRIPLES "/home/aditya/llvm_obfuscation/CMakeLists.txt;82;add_test;/home/aditya/llvm_obfuscation/CMakeLists.txt;0;")
