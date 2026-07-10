cmake_minimum_required(VERSION 3.16)

project(OS-Simulator VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(os_simulator
    src/main.cpp
    src/core/Process.cpp
    src/core/Simulation.cpp
    src/kernel/Kernel.cpp
    src/schedulers/FCFSScheduler.cpp
    src/shell/Shell.cpp
    src/system/SystemCallInterface.cpp
)

target_include_directories(os_simulator
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)
