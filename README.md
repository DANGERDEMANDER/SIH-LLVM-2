<<<<<<< HEAD
# SIH-LLVM-2
llvm_obfuscation - base project

This repository contains several LLVM-based obfuscation passes built as a
PassPlugin and a small CLI and programmatic runner to exercise them.

Quick start
1. Ensure LLVM 14 (clang, opt, llc, llvm-config) is installed.
2. From repo root build and run tests:

```bash
cmake -S . -B build -DLLVM_DIR=$(llvm-config --cmakedir) -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
cd build && ctest --output-on-failure
```

Using the programmatic runner

The `run_cff` tool loads the plugin and runs the textual pipeline `cff` directly:

```bash
# Run using the built plugin in the build directory
RUN_CFF_PLUGIN=./libObfPasses.so ./tools/run_cff /absolute/path/to/tests/cff_test.bc
```

Using the CLI wrapper (`obfuscator`)

The `obfuscator` tool can accept bitcode or C/C++ source files. If a
source file is provided it will be compiled to LLVM bitcode using clang and
then the requested pipeline will be run. Example (bitcode input):

```bash
./tools/obfuscator path/to/input.bc ---o=build/hello.obf.bc ---pass=cff ---plugin=./libObfPasses.so ---v
```

Example (C source input):

```bash
./tools/obfuscator path/to/input.c ---o=build/hello.obf.bc ---pass=cff ---plugin=./libObfPasses.so ---v
```

Interactive mode (paste source):

```bash
./tools/obfuscator -i
# then follow the prompt to paste source and finish with EOF (Ctrl-D)
```

CI
This repository contains a simple GitHub Actions workflow at `.github/workflows/ci.yml`
that builds and runs the CTest suite on Ubuntu with LLVM 14. A Windows build
job was added to attempt building native Windows artifacts.

If CMake cannot find LLVM set `LLVM_DIR` to the output of `llvm-config --cmakedir`.

Quick start
1. Ensure LLVM 14 (clang, opt, llc, llvm-config) is installed.
2. From repo root build and run tests:

```bash
cmake -S . -B build -DLLVM_DIR=$(llvm-config --cmakedir) -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
cd build && ctest --output-on-failure
```

Using the programmatic runner

The `run_cff` tool loads the plugin and runs the textual pipeline `cff` directly:

```bash
# Run using the built plugin in the build directory
RUN_CFF_PLUGIN=./libObfPasses.so ./tools/run_cff /absolute/path/to/tests/cff_test.bc
```

Using the CLI wrapper (`obfuscator`)

The `obfuscator` tool shells out to `opt` and loads the plugin via `-load-pass-plugin`.

```bash
./tools/obfuscator -in tests/hello.bc -out build/hello.obf.bc -pass cff
```

CI
This repository contains a simple GitHub Actions workflow at `.github/workflows/ci.yml`
that builds and runs the CTest suite on Ubuntu with LLVM 14.

If CMake cannot find LLVM set `LLVM_DIR` to the output of `llvm-config --cmakedir`.
>>>>>>> 6f7124f (Add interactive CLI, source->bitcode compile and fix CFF ConstantInt types; add Windows CI job)
