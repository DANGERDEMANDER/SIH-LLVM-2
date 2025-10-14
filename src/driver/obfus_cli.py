#!/usr/bin/env python3
# src/driver/obfus_cli.py
import os, sys, subprocess, argparse

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
BUILD = os.path.join(ROOT, 'build')
DIST = os.path.join(ROOT, 'dist')
PLUGIN = os.path.join(BUILD, 'libObfPasses' + ('.dylib' if sys.platform == 'darwin' else '.so'))
RUNTIME_SRC = os.path.join(ROOT, 'src', 'runtime', 'decryptor.c')

def run(cmd, env=None):
    print("$ " + " ".join(cmd))
    subprocess.check_call(cmd, env=env)

def ensure_dirs():
    os.makedirs(BUILD, exist_ok=True)
    os.makedirs(DIST, exist_ok=True)

def build_plugin(llvm_dir=None):
    if not os.path.exists(PLUGIN):
        llvm_cmake = llvm_dir or os.environ.get('LLVM_DIR', '/usr/lib/llvm-14/lib/cmake/llvm')
        run(['cmake', '-S', ROOT, '-B', BUILD, f'-DLLVM_DIR={llvm_cmake}'])
        run(['cmake', '--build', BUILD, '-j'])
    else:
        print("Plugin exists:", PLUGIN)

def compile_to_bc(src, out):
    run(['clang', '-emit-llvm', '-c', src, '-o', out])

def run_passes(inbc, outbc, preset, seed):
    if preset == 'light':
        passes = 'string-obf'
    else:
        passes = 'string-obf,bogus-insert'
    env = os.environ.copy()
    if seed:
        env['LLVM_OBF_SEED'] = str(seed)
    env['OFILE'] = os.path.join(BUILD, 'counters.json')
    try:
        run(['opt', '-load-pass-plugin', PLUGIN, '-passes=' + passes, inbc, '-o', outbc], env=env)
    except Exception:
        run(['opt', '-load', PLUGIN, '-string-obf', inbc, '-o', outbc], env=env)

def codegen_and_link(bc, out):
    obj = os.path.join(BUILD, 'main_obf.o')
    run(['llc', '-filetype=obj', bc, '-o', obj])
    run(['clang', obj, RUNTIME_SRC, '-o', out])

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--src', default=os.path.join(ROOT, 'tests', 'hello.c'))
    parser.add_argument('--preset', default='balanced')
    parser.add_argument('--seed', type=int, default=None)
    parser.add_argument('--llvm-dir', default=None)
    args = parser.parse_args()
    ensure_dirs()
    build_plugin(args.llvm_dir)
    bc = os.path.join(BUILD, 'main.bc'); bc_out = os.path.join(BUILD, 'main_obf.bc')
    exe_orig = os.path.join(DIST, 'main_orig'); exe_obf = os.path.join(DIST, 'main_obf')
    compile_to_bc(args.src, bc)
    run(['clang', args.src, '-o', exe_orig])
    run_passes(bc, bc_out, args.preset, args.seed)
    codegen_and_link(bc_out, exe_obf)
    print("Built. You can run:", exe_obf)

if __name__ == '__main__':
    main()
