cmake_minimum_required(VERSION 3.16)
project(ift729_str VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Optimisation + warnings
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -O2")

# ── Sources ────────────────────────────────────────────────────────────────
set(SOURCES
    src/main.cpp
    src/acquisition/acquisition.cpp
    src/analyse/analyse.cpp
    src/alerte/alerte.cpp
    src/diagnostic/diagnostic.cpp
)

# ── Exécutable principal ───────────────────────────────────────────────────
add_executable(str_monitor ${SOURCES})

target_include_directories(str_monitor PRIVATE src)

# ── pthreads (Linux/WSL) ───────────────────────────────────────────────────
find_package(Threads REQUIRED)
target_link_libraries(str_monitor Threads::Threads)

# ── Dossier logs (créé à la compilation) ──────────────────────────────────
add_custom_command(TARGET str_monitor POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
            $<TARGET_FILE_DIR:str_monitor>/logs
    COMMENT "Création du dossier logs/"
)

message(STATUS "Projet IFT729-STR configuré — build avec: cmake --build build/")
