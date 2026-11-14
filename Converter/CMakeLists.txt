cmake_minimum_required(VERSION 3.22)
project(MCPE061To09Converter LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# The 0.9 LevelDB fork stores compression type 0x02 as zlib.  The converter
# uses the same zlib library as the UWP project for both chunk data and table
# blocks.  Define this imported target before adding LevelDB so its port
# layer can include zlib.h and link against the library.
set(MCPE_UWP_ROOT "${CMAKE_CURRENT_LIST_DIR}/..")
add_library(converter_zlib STATIC IMPORTED GLOBAL)
set_target_properties(converter_zlib PROPERTIES
  IMPORTED_LOCATION "${MCPE_UWP_ROOT}/handheld/lib/lib/x64/zlib.lib"
  INTERFACE_INCLUDE_DIRECTORIES "${MCPE_UWP_ROOT}/handheld/lib/include")

set(HAVE_ZLIB 1 CACHE INTERNAL "LevelDB zlib support" FORCE)
# Prevent a stale cache from enabling the previous, incompatible Zstd path.
set(HAVE_ZSTD 0 CACHE INTERNAL "LevelDB Zstd support" FORCE)
set(LEVELDB_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(LEVELDB_BUILD_BENCHMARKS OFF CACHE BOOL "" FORCE)
set(LEVELDB_INSTALL OFF CACHE BOOL "" FORCE)
add_subdirectory(third_party/leveldb)
target_link_libraries(leveldb converter_zlib)
add_executable(MCPE061To09Converter WIN32
  src/main.cpp src/converter.cpp src/converter.h
  src/legacy_biome.cpp src/legacy_biome.h)
set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT MCPE061To09Converter)
set_target_properties(MCPE061To09Converter PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/ReleaseFixed"
  RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/DebugFixed")
target_link_libraries(MCPE061To09Converter PRIVATE leveldb converter_zlib Comctl32 Shell32)
target_compile_definitions(MCPE061To09Converter PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN)
target_compile_options(MCPE061To09Converter PRIVATE /utf-8)
add_custom_command(TARGET MCPE061To09Converter POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${MCPE_UWP_ROOT}/handheld/lib/bin/x64/zlib1.dll"
    "$<TARGET_FILE_DIR:MCPE061To09Converter>/zlib1.dll")
