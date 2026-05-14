cmake_minimum_required(VERSION 3.16)
project(WaveformSimulator LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Platform-specific optimization flags are applied per-target below.
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

target_compile_options(${PROJECT_NAME} PRIVATE
    $<$<AND:$<CONFIG:Release>,$<CXX_COMPILER_ID:MSVC>>:/O2>
    $<$<AND:$<CONFIG:Release>,$<CXX_COMPILER_ID:MSVC>>:/Oi>
    $<$<AND:$<CONFIG:Release>,$<CXX_COMPILER_ID:MSVC>>:/Ot>
    $<$<AND:$<CONFIG:Release>,$<CXX_COMPILER_ID:MSVC>>:/GL>
    $<$<AND:$<CONFIG:Release>,$<CXX_COMPILER_ID:MSVC>>:/fp:fast>
    $<$<AND:$<CONFIG:Release>,$<CXX_COMPILER_ID:GNU>>:-O3>
    $<$<AND:$<CONFIG:Release>,$<CXX_COMPILER_ID:GNU>>:-march=native>
    $<$<AND:$<CONFIG:Release>,$<CXX_COMPILER_ID:GNU>>:-ffast-math>
    $<$<AND:$<CONFIG:Release>,$<CXX_COMPILER_ID:GNU>>:-flto>
    $<$<AND:$<CONFIG:Release>,$<CXX_COMPILER_ID:Clang>>:-O3>
    $<$<AND:$<CONFIG:Release>,$<CXX_COMPILER_ID:Clang>>:-march=native>
    $<$<AND:$<CONFIG:Release>,$<CXX_COMPILER_ID:Clang>>:-ffast-math>
    $<$<AND:$<CONFIG:Release>,$<CXX_COMPILER_ID:Clang>>:-flto>
)

target_link_options(${PROJECT_NAME} PRIVATE
    $<$<AND:$<CONFIG:Release>,$<CXX_COMPILER_ID:MSVC>>:/LTCG>
    $<$<AND:$<CONFIG:Release>,$<CXX_COMPILER_ID:GNU>>:-flto>
    $<$<AND:$<CONFIG:Release>,$<CXX_COMPILER_ID:Clang>>:-flto>
)

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
