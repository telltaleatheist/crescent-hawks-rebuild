cmake_minimum_required(VERSION 3.20)
project(crescent_hawks_rebuild LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Where the player's own copy of the game is (docs/rules.md, rule 1: we never
# ship it). The CHR_GAME_DIR environment variable or `chr --game-dir` overrides
# this at run time. The tests that need real game files skip without it.
set(CHR_GAME_DIR "${CMAKE_SOURCE_DIR}/original/dos-1.03" CACHE PATH
    "The original game's folder (BTECH.EXE, *.CMP, ...) the program and tests read")

if(MSVC)
  add_compile_options(/W4 /permissive- /utf-8)
else()
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# The game logic. Plain C++: no SDL, no renderer types, never touches pixels.
# It speaks in game terms and asset IDs (the goal of a later visual makeover as a mod).
add_library(chrgame STATIC
  src/game/asset_id.cpp
)
target_include_directories(chrgame PUBLIC src)

# Readers for the original's files, documented in docs/formats/. Standard C++ only.
add_library(chrformats STATIC
  src/formats/cmp.cpp
  src/formats/game_files.cpp
  src/formats/palette.cpp
)
target_include_directories(chrformats PUBLIC src)

enable_testing()

add_executable(chrtests
  tests/test_main.cpp
  tests/test_cmp.cpp
)
target_link_libraries(chrtests PRIVATE chrformats chrgame)
target_compile_definitions(chrtests PRIVATE CHR_DEFAULT_GAME_DIR="${CHR_GAME_DIR}")
add_test(NAME chrtests COMMAND chrtests)

# The platform layer: SDL3 window, rendering and the asset provider.
find_package(SDL3 CONFIG QUIET)
if(SDL3_FOUND)
  add_library(chrplatform STATIC
    src/platform/original_assets.cpp
    src/platform/renderer.cpp
  )
  target_include_directories(chrplatform PUBLIC src)
  target_link_libraries(chrplatform PUBLIC chrgame chrformats SDL3::SDL3)

  add_executable(chr src/main.cpp)
  target_link_libraries(chr PRIVATE chrplatform chrformats chrgame SDL3::SDL3)
  target_compile_definitions(chr PRIVATE CHR_DEFAULT_GAME_DIR="${CHR_GAME_DIR}")
  if(WIN32)
    # SDL3.dll beside chr.exe.
    add_custom_command(TARGET chr POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:SDL3::SDL3> $<TARGET_FILE_DIR:chr>)
  endif()
else()
  message(STATUS "SDL3 not found: chr is not built (macOS: brew install sdl3)")
endif()
