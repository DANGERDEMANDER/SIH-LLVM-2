# SIH LLVM Obfuscator

A C/C++ source code obfuscator built on LLVM 14. It applies multiple layers of code transformation to harden programs against reverse engineering and static analysis. The tool is managed through a user-friendly interactive command-line interface.

## ✨ Features

* **Multiple Obfuscation Passes**: Implements a suite of powerful obfuscation techniques:
    * **String Encryption**: Encrypts string literals and decrypts them at runtime.
    * **Bogus Control Flow**: Inserts complex and irrelevant conditional jumps to confuse decompilers.
    * **Fake Loops**: Adds computationally inexpensive loops to disrupt control flow analysis.
    * **Control Flow Flattening**: Transforms the program's control flow into a large state machine, obscuring the original logic.
* **Interactive TUI**: An easy-to-use menu for configuring and running the obfuscation process.
* **Obfuscation Presets**: Includes presets from `Light` to `Nightmare` for a quick and effective setup.
* **Detailed Reports**: Provides a "Before vs. After" summary, showing changes in instruction count, basic blocks, and code size to demonstrate the obfuscation's impact.
* **Readable IR Output**: Generates a final, human-readable LLVM IR file (`.ll`) for analysis and verification of the applied transformations.

## 🖥️ Demo

The tool operates through a simple interactive menu:

```text
=========================================================
                 SIH LLVM Obfuscator
    Professional Security & Anti-Analysis
=========================================================

>> Current Settings
---------------------------------------------------------
Input Source File         : ./tests/cff_test.c
Obfuscation Preset        : Balanced
Obfuscation Seed          : Random
---------------------------------------------------------

Main Menu:
  1. Change Input Source File
  2. Select Obfuscation Preset
  3. Set Obfuscation Seed (0 for random)
  4. Run Obfuscation Process
  5. Quit

Select option [1-5]:
🚀 Getting Started
Prerequisites
LLVM 14: You must have LLVM version 14 installed, including clang-14, opt-14, llc-14, and llvm-config-14.

CMake: Version 3.13 or higher.

C++ Compiler: A compiler that supports C++17 (like g++ or clang++).

Build Instructions
Clone the repository:

Bash

git clone [https://github.com/your-username/llvm_obfuscation.git](https://github.com/your-username/llvm_obfuscation.git)
cd llvm_obfuscation
Configure with CMake:

This command points to the LLVM 14 installation directory. Adjust LLVM_DIR if your installation path is different.

Bash

cmake -S . -B build -DLLVM_DIR=/usr/lib/llvm-14/lib/cmake/llvm
Build the project:

This will compile the passes and the main executable.

Bash

cmake --build build -j$(nproc)
🛠️ Usage
Run the main executable from the project's root directory, providing the path to the C/C++ source file you want to obfuscate.

Bash

./build/tools/LLVM_OBFSCALTION.exe ./tests/hello.c
This will launch the interactive menu where you can:

Confirm or change the input file.

Select an obfuscation preset (Light, Balanced, Heavy, Nightmare, or Custom).

Set a specific seed for reproducible builds or use a random one.

Run the obfuscation process.

Output Files
After a successful run, the following files will be generated in the project's root directory:

obfuscated_output: The final, compiled, and obfuscated executable.

final_readable_ir.ll: The human-readable LLVM IR of the fully obfuscated program, which you can inspect to see the transformations.

🔧 Continuous Integration
This repository includes a GitHub Actions workflow defined in .github/workflows/ci.yml. It automatically builds and tests the project on Ubuntu and Windows environments upon every push and pull request to ensure code integrity.
