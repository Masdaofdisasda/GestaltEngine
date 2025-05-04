[![ci-linux](https://github.com/Masdaofdisasda/GestaltEngine/actions/workflows/ci-linux.yml/badge.svg?branch=main)](https://github.com/Masdaofdisasda/GestaltEngine/actions/workflows/ci-linux.yml)
[![ci-windows](https://github.com/Masdaofdisasda/GestaltEngine/actions/workflows/ci-windows.yml/badge.svg?branch=main)](https://github.com/Masdaofdisasda/GestaltEngine/actions/workflows/ci-windows.yml)

# Gestalt Engine

A modern game engine built with Vulkan, focused on high-performance graphics and rendering. The Gestalt Engine is currently tested on Windows 10 and 11, compiled using Visual Studio 2022 (MSVC).

## Requirements

- **Vulkan SDK**: Version 1.3.275 or higher must be installed.
- **CMake**: Required for building the project.
- **Python**: Used for the automated setup and build process.

## Installation

1. **Clone the Repository**:
    ```bash
    git clone https://github.com/Masdaofdisasda/GestaltEngine.git
    cd GestaltEngine
    ```

2. **Initialize Submodules**:  
   After cloning, initialize submodules to load the basic GLTF models needed to test the engine:
    ```bash
    git submodule update --init --recursive
    ```

3. **Run the Setup Script**:  
   The engine can be built automatically using the `setup.py` script:
    ```bash
    python setup.py
    ```
    This script will run CMake to configure and build the project using the default settings.

## Project Structure

The Gestalt Engine is organized into three layers:

- **Foundation Layer**: Contains utility classes and general definitions used throughout the engine.
- **Application Layer**: Handles application logic, UI, the Entity-Component System (ECS), and asset loading.
- **Graphics Layer**: Contains the main Vulkan graphics code, shaders, and implementations of rendering algorithms.

## Building Manually with CMake

If you prefer, you can build the project manually with CMake:

1. **Configure the Project**:
    ```bash
    cmake -B build -S .
    ```

2. **Build**:
    ```bash
    cmake --build build --config Release
    ```

## Running the Engine

After building, you can run the executable generated in the `build` directory to start the engine. The engine supports loading GLTF models directly from the GUI. Running the application for the first time will also generate a configuration file at `bin/config.json`.

## Additional Information

- **Config File**: The `config.json` file contains settings for the display window.
- **Supported Models**: Basic GLTF models are supported out of the box.
