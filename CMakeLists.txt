cmake_minimum_required(VERSION 3.22)

project(gl4es LANGUAGES C)

# --- KONFIGURASI PURE ANDROID ---
message(STATUS "GL4ES 3 for Android")

# Set Hardcoded Variables untuk Android
set(ANDROID ON)
set(NOX11 ON)
set(NO_GBM ON)
set(DEFAULT_ES 3) # Kita kunci ke GLES 3.x backend
set(USE_ANDROID_LOG ON) # Wajib agar log muncul di Logcat/Launcher

# Output Paths
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/lib)

link_directories(${CMAKE_LIBRARY_OUTPUT_DIRECTORY})

# Definisi Preprocessor (Otak dari pembersihan)
# Kita HANYA mendefinisikan Android. Pandora, Pyra, dll tidak didefinisikan = MATI.
add_definitions(-DANDROID -DNOX11 -DNO_GBM -DDEFAULT_ES=3)
add_definitions(-DUSE_ANDROID_LOG)

# Setup Compiler Flags (Modern Clang 18 Ready)
# Kita gunakan C17 standard
set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Optimization Flags & Warning Supression
if(CMAKE_C_COMPILER_ID MATCHES "Clang")
    add_definitions(-Wno-deprecated-declarations)
    add_definitions(-Wno-unused-function -Wno-unused-variable)
    add_definitions(-Wno-implicit-const-int-float-conversion)
    add_definitions(-fvisibility=hidden) # Penting untuk performa!
endif()

# Temukan library Log Android
find_library(log-lib log)

# Include Source Code
include_directories(include)
add_subdirectory(src)