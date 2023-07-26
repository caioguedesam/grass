# Typheus app standard build script
# TODO(caio): PROFILE builds

import sys
import subprocess
import time
import glob
import os

exe_name = 'app'

def build_engine(output_dir, build_type, full_build):
    os.chdir('./engine/')
    build_command = f'python build.py -{build_type}'
    if full_build:
        build_command += ' --full'
    subprocess.run(build_command)
    os.chdir('..')
    if os.path.exists(f'{output_dir}/ty.lib'):
        os.remove(f'{output_dir}/ty.lib')

    if build_type == 'd':
        os.rename(f'./engine/build/debug/ty.lib', f'{output_dir}/ty.lib')
    elif build_type == 'r':
        os.rename(f'./engine/build/release/ty.lib', f'{output_dir}/ty.lib')

def build_main(output_dir, build_type, cc_flags):
    build_command = f'clang {cc_flags}'
    if build_type == 'd':
        build_command += f' --debug -O0'
    elif build_type == 'r':
        build_command += f' -Ofast'
    build_command += f' ./app/main.cpp'
    build_command += f' -l{output_dir}/ty.lib'
    build_command += f' -fms-runtime-lib=dll -Wl,-nodefaultlib:libcmt'
    build_command += f' --output={output_dir}/{exe_name}.exe'

    print('Starting project build...')
    start = time.time()
    subprocess.run(build_command, shell=True)
    end = time.time()
    print(f'Finished project build in {end - start} seconds.')

build_type = None
output_dir = ''
if '-d' in sys.argv:
    build_type = 'd'    # Debug
    output_dir = './build/debug'
elif '-r' in sys.argv:
    build_type = 'r'    # Release
    output_dir = './build/release'

output_dir = output_dir.replace('\\', '/')
os.makedirs(output_dir, exist_ok=True)

# Compiler flags
f = open('compile_flags.txt')
cc_flags = ' '.join(f.read().splitlines())
f.close()

if '--engine-full' in sys.argv:
    build_engine(output_dir, build_type, True)
elif '--engine' in sys.argv:
    build_engine(output_dir, build_type, False)

build_main(output_dir, build_type, cc_flags)
