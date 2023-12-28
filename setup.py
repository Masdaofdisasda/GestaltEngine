import os
import subprocess
import configparser

def create_vs_build(build_dir, cmake_path='..', vs_version='Visual Studio 17 2022', config_file='config.ini'):
    """
    Creates a build directory and generates Visual Studio project files.

    :param build_dir: The directory where the build files will be created.
    :param cmake_path: The relative path to the CMakeLists.txt.
    :param vs_version: The Visual Studio generator version.
    :param config_file: Path to the configuration file.
    """

    # Read the configuration file
    config = configparser.ConfigParser()
    config.read(config_file)
    vcpkg_toolchain_file = config.get('vcpkg', 'toolchain_file')

    # Create the build directory if it doesn't exist
    os.makedirs(build_dir, exist_ok=True)

    # Construct the CMake command
    cmake_command = [
        'cmake',
        cmake_path,
        '-G', vs_version,
        '-A', 'x64',  # Specify architecture
        '-DCMAKE_TOOLCHAIN_FILE=' + vcpkg_toolchain_file  # Add vcpkg toolchain file
    ]

    # Change to the build directory
    os.chdir(build_dir)

    # Execute the CMake command
    subprocess.run(cmake_command)

if __name__ == '__main__':
    build_directory = 'project'
    create_vs_build(build_directory)
