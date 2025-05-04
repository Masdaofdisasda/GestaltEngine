import os
import subprocess
import argparse
import shutil
import platform

def create_build(build_dir, cmake_path='..', generator='Ninja', format_target='fix-format'):
    """
    Creates a build directory, generates project files, and formats the code.

    :param build_dir: The directory where the build files will be created.
    :param cmake_path: The relative path to the CMakeLists.txt.
    :param generator: The CMake generator to use.
    :param format_target: The CMake target for formatting the code.
    """
    os.makedirs(build_dir, exist_ok=True)

    # Construct the CMake command
    cmake_command = [
    'cmake',
    cmake_path,
    '-G', generator,
    '--graphviz=deps.dot'
    ]


    os.chdir(build_dir)
    subprocess.run(cmake_command, check=True)
    
    format_command = ['cmake', '--build', '.', '--target', format_target]
    subprocess.run(format_command, check=True)

def clean_build(build_dir):
    """
    Cleans the build directory by removing it.

    :param build_dir: The directory where the build files are located.
    """
    if os.path.exists(build_dir):
        shutil.rmtree(build_dir)
        print(f"Cleaned build directory: {build_dir}")
    else:
        print(f"Build directory does not exist: {build_dir}")

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description="Setup or clean the project build.")
    parser.add_argument('-clean', action='store_true', help="Clean the build directory before building.")

    args = parser.parse_args()
    build_directory = 'build'

    if args.clean:
        clean_build(build_directory)
    
    # Determine the appropriate generator based on the platform
    if platform.system() == 'Windows':
        generator = 'Visual Studio 17 2022'
    else:
        generator = 'Ninja'  # For Linux or other platforms, we use Ninja

    create_build(build_directory, generator=generator)
