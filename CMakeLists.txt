cmake_minimum_required(VERSION 3.16)
project(WaveformSimulator LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Default to Debug for single-config generators if not specified
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Choose the type of build." FORCE)
endif()

# Find threading library (pthreads on Linux, native on Windows)
find_package(Threads REQUIRED)

# Find SFML (graphics, window, system modules for GUI rendering)
# On Windows: prefers SFML-2.6.2 folder alongside CMakeLists.txt.
# On Linux: expects SFML installed via package manager (libsfml-dev).
if(WIN32 AND (NOT DEFINED SFML_DIR OR SFML_DIR STREQUAL "" OR SFML_DIR STREQUAL "SFML_DIR-NOTFOUND"))
    set(SFML_DIR "${CMAKE_SOURCE_DIR}/SFML-2.6.2/lib/cmake/SFML" CACHE PATH "Path to SFMLConfig.cmake" FORCE)
endif()
find_package(SFML 2.6 COMPONENTS graphics window system REQUIRED)

# Source files
set(SOURCES
    src/main.cpp
    src/Application.cpp
    src/WaveformEngine.cpp
    src/WaveformRenderer.cpp
    src/GuiRenderer.cpp
)

set(HEADERS
    src/Application.h
    src/GuiRenderer.h
    src/PulseShape.h
    src/Transition.h
    src/WaveformEngine.h
    src/WaveformRenderer.h
)

add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS})

target_include_directories(${PROJECT_NAME} PRIVATE src)

target_link_libraries(${PROJECT_NAME} PRIVATE
    Threads::Threads
    sfml-graphics
    sfml-window
    sfml-system
)

# On Linux, also link math library explicitly
if(UNIX AND NOT APPLE)
    target_link_libraries(${PROJECT_NAME} PRIVATE m)
endif()

# Copy SFML DLLs to output directory on Windows
if(WIN32)
    foreach(SFML_LIB graphics window system)
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${CMAKE_SOURCE_DIR}/SFML-2.6.2/bin/sfml-${SFML_LIB}-2.dll"
                $<TARGET_FILE_DIR:${PROJECT_NAME}>
        )
    endforeach()
endif()
