cmake_minimum_required(VERSION 3.20)
project(mySTLViewer VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find required packages
find_package(SDL3 CONFIG REQUIRED)
find_package(glad CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)
find_package(nfd CONFIG REQUIRED)
find_package(pugixml CONFIG REQUIRED)
find_package(LibArchive REQUIRED)
find_path(EARCUT_HPP_INCLUDE_DIRS "mapbox/earcut.hpp")
find_package(OpenMP)

# Source files
set(SOURCES
    src/main.cpp
    src/STLLoader.cpp
    src/XMLLoader.cpp
    src/Renderer.cpp
    src/progress/ConsoleProgress.cpp
)

set(HEADERS
    src/STLLoader.h
    src/XMLLoader.h
    src/Renderer.h
    src/Mesh.h
    src/progress/Progress_abstract.h
    src/progress/ConsoleProgress.h
)


# Create executable
add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS})

# Link libraries
target_link_libraries(${PROJECT_NAME} PRIVATE
    SDL3::SDL3
    glad::glad
    glm::glm
    nfd::nfd
    pugixml::pugixml
    LibArchive::LibArchive
)

# Include directories
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${EARCUT_HPP_INCLUDE_DIRS}
)

if (OpenMP_CXX_FOUND)
    target_link_libraries(${PROJECT_NAME} PRIVATE OpenMP::OpenMP_CXX)
    target_compile_definitions(${PROJECT_NAME} PRIVATE HAVE_OPENMP)
endif()

# Fallback for AppleClang with Homebrew libomp on macOS
if (APPLE AND NOT OpenMP_CXX_FOUND)
    set(HOMEBREW_LIBOMP_PREFIX "/opt/homebrew/opt/libomp" CACHE PATH "Homebrew libomp prefix")
    if (EXISTS "${HOMEBREW_LIBOMP_PREFIX}/include/omp.h")
        message(STATUS "Using Homebrew libomp at ${HOMEBREW_LIBOMP_PREFIX}")
        target_include_directories(${PROJECT_NAME} PRIVATE "${HOMEBREW_LIBOMP_PREFIX}/include")
        target_link_directories(${PROJECT_NAME} PRIVATE "${HOMEBREW_LIBOMP_PREFIX}/lib")
        target_link_libraries(${PROJECT_NAME} PRIVATE omp)
        target_compile_options(${PROJECT_NAME} PRIVATE -Xpreprocessor -fopenmp)
        target_link_options(${PROJECT_NAME} PRIVATE -Wl,-rpath,${HOMEBREW_LIBOMP_PREFIX}/lib)
        target_compile_definitions(${PROJECT_NAME} PRIVATE HAVE_OPENMP)
    else()
        message(STATUS "OpenMP not found; to enable on macOS, install libomp via Homebrew and set HOMEBREW_LIBOMP_PREFIX if non-standard")
    endif()
endif()

## No native menus; rely on cross-platform shortcuts

# Copy shaders to build directory
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/shaders
     DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
