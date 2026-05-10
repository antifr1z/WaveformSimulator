cmake_minimum_required(VERSION 3.16)
project(WaveformSimulator LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Platform-specific optimization flags
if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    add_compile_options(/O2 /Oi /Ot /GL /fp:fast)
    add_link_options(/LTCG)
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    add_compile_options(-O3 -march=native -ffast-math -flto)
    add_link_options(-flto)
endif()

# Find threading library (pthreads on Linux, native on Windows)
find_package(Threads REQUIRED)

# Find SFML (graphics, window, system modules for GUI rendering)
# On Windows: expects SFML-2.6.2/ folder alongside CMakeLists.txt
# On Linux:   expects SFML installed via package manager (libsfml-dev)
if(WIN32)
    set(SFML_DIR "${CMAKE_SOURCE_DIR}/SFML-2.6.2/lib/cmake/SFML")
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
