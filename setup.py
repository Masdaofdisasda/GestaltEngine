import os
import subprocess
import configparser

def create_vs_build(build_dir, cmake_path='..', vs_version='Visual Studio 17 2022',  format_target='fix-format'):
    """
    Creates a build directory, generates Visual Studio project files, and formats the code.

    :param build_dir: The directory where the build files will be created.
    :param cmake_path: The relative path to the CMakeLists.txt.
    :param vs_version: The Visual Studio generator version.
    :param config_file: Path to the configuration file.
    :param format_target: The CMake target for formatting the code.
    """

    os.makedirs(build_dir, exist_ok=True)

    # Construct the CMake command
    # cmake '..' -G 'Visual Studio 17 2022' -A 'x64'
    cmake_command = [
        'cmake',
        cmake_path,
        '-G', vs_version,
        '-A', 'x64',
    ]

    os.chdir(build_dir)

    subprocess.run(cmake_command)
    
    format_command = ['cmake', '--build', '.', '--target', format_target]
    subprocess.run(format_command)

if __name__ == '__main__':
    build_directory = 'project'
    create_vs_build(build_directory)
