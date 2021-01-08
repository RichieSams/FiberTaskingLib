###
 # FiberTaskingLib - A tasking library that uses fibers for efficient task switching
 #
 # This library was created as a proof of concept of the ideas presented by
 # Christian Gyrling in his 2015 GDC Talk 'Parallelizing the Naughty Dog Engine Using Fibers'
 #
 # http://gdcvault.com/play/1022186/Parallelizing-the-Naughty-Dog-Engine
 #
 # FiberTaskingLib is the legal property of Adrian Astley
 # Copyright Adrian Astley 2015 - 2018
 #
 # Licensed under the Apache License, Version 2.0 (the "License");
 # you may not use this file except in compliance with the License.
 # You may obtain a copy of the License at
 # 
 # http://www.apache.org/licenses/LICENSE-2.0
 # 
 # Unless required by applicable law or agreed to in writing, software
 # distributed under the License is distributed on an "AS IS" BASIS,
 # WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 # See the License for the specific language governing permissions and
 # limitations under the License.
 ##

import subprocess
import glob
import sys
import os

def main():
    print('Re-create compile commands', flush=True)
    try:
        os.mkdir('build_lint')
    except FileExistsError:
        pass
    subprocess.check_call(['cmake', '-DCMAKE_EXPORT_COMPILE_COMMANDS=ON', '-Bbuild_lint', '.'])

    with open('tools/clang-tidy-checks.txt') as f:
        checks = f.readlines()
        checks = [x.strip() for x in checks]
    
    subprocess.check_call(['clang-tidy-8', '-list-checks', '-checks', f'{",".join(checks)}'])

    source_files = glob.glob('benchmarks/**/*.cpp', recursive=True)
    source_files += glob.glob('examples/**/*.cpp', recursive=True)
    source_files += glob.glob('include/**/*.cpp', recursive=True)
    source_files += glob.glob('source/**/*.cpp', recursive=True)
    source_files += glob.glob('tests/**/*.cpp', recursive=True)

    print('Files:')
    for file in source_files:
        print(f'    {file}')
    print('\n', flush=True)

    args = [
        'clang-tidy-8',
        '-quiet',
        '-p', 'build_lint',
        '-header-filter', 'include/ftl/.*',
        '-warnings-as-errors', '*',
        '-checks', f'{",".join(checks)}'
    ]

    result = subprocess.run(args + source_files)
    if result.returncode != 0:
        sys.exit(result.returncode)


if __name__ == '__main__':
    main()
