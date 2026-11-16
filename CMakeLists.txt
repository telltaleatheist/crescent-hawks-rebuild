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
# Readers for the original's files, documented in docs/formats/. Standard C++ only.
add_library(chrformats STATIC
  src/formats/cells.cpp
  src/formats/cmp.cpp
  src/formats/exe_data.cpp
  src/formats/exepack.cpp
  src/formats/font_file.cpp
  src/formats/game_files.cpp
  src/formats/import.cpp
  src/formats/json.cpp
  src/formats/icn.cpp
  src/formats/mtp.cpp
  src/formats/palette.cpp
  src/formats/sha256.cpp
)
target_include_directories(chrformats PUBLIC src)

# (The game logic reads the map data through chrformats.)
add_library(chrgame STATIC
  src/game/asset_id.cpp
  src/game/map_view.cpp
  src/game/movement.cpp
  src/game/text.cpp
  src/game/text_provider.cpp
  src/game/town.cpp
)
target_include_directories(chrgame PUBLIC src)
target_link_libraries(chrgame PUBLIC chrformats)

enable_testing()

add_executable(chrtests
  tests/test_main.cpp
  tests/test_cmp.cpp
  tests/test_maps.cpp
  tests/test_text.cpp
  tests/test_walk.cpp
)
target_link_libraries(chrtests PRIVATE chrgame)
target_compile_definitions(chrtests PRIVATE CHR_DEFAULT_GAME_DIR="${CHR_GAME_DIR}"
  CHR_TEST_SCRATCH="${CMAKE_BINARY_DIR}/test-scratch")
add_test(NAME chrtests COMMAND chrtests)

# The platform layer: SDL3 window, rendering and the asset provider.
find_package(SDL3 CONFIG QUIET)
if(SDL3_FOUND)
  add_library(chrplatform STATIC
    src/platform/map_layer.cpp
    src/platform/original_assets.cpp
    src/platform/renderer.cpp
    src/platform/text_layer.cpp
  )
  target_include_directories(chrplatform PUBLIC src)
  target_link_libraries(chrplatform PUBLIC chrgame SDL3::SDL3)

  add_executable(chr src/main.cpp)
  target_link_libraries(chr PRIVATE chrplatform)
  target_compile_definitions(chr PRIVATE CHR_DEFAULT_GAME_DIR="${CHR_GAME_DIR}")
  # The program itself, walking known routes on map 1 with no window (SDL's
  # dummy video driver). They need the game files.
  if(EXISTS "${CHR_GAME_DIR}/MAP1.MTP")
    set(chr_headless "SDL_VIDEO_DRIVER=dummy;SDL_AUDIO_DRIVER=dummy;CHR_DATA_DIR=${CMAKE_BINARY_DIR}/chr-data")
    add_test(NAME walk_blocked_by_fence
      COMMAND chr --map 1 --script "down*3" --screenshot walk_blocked.bmp)
    set_tests_properties(walk_blocked_by_fence PROPERTIES ENVIRONMENT "${chr_headless}"
      PASS_REGULAR_EXPRESSION "blocked at 0c45,c01b")
    add_test(NAME walk_into_citadel
      COMMAND chr --map 1 --script "up*5 y" --screenshot walk_citadel.bmp)
    set_tests_properties(walk_into_citadel PROPERTIES ENVIRONMENT "${chr_headless}"
      PASS_REGULAR_EXPRESSION "entered place 1 \\(Citadel\\)")
    add_test(NAME walk_scrolls_west
      COMMAND chr --map 1 --script "left*10" --screenshot walk_west.bmp)
    set_tests_properties(walk_scrolls_west PROPERTIES ENVIRONMENT "${chr_headless}"
      PASS_REGULAR_EXPRESSION "moved at 0c3b,c019")
  endif()
  # The rebuild against the original, pixel for pixel (tools/reference,
  # docs/reference.md): runs the original in DOSBox-X headless. Skips without
  # DOSBox-X, nasm or the game.
  find_package(Python3 COMPONENTS Interpreter QUIET)
  if(Python3_FOUND)
    add_test(NAME reference_compare
      COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tools/reference/check.py
        --chr $<TARGET_FILE:chr> --game-dir ${CHR_GAME_DIR}
        --data-dir ${CMAKE_BINARY_DIR}/chr-data --work ${CMAKE_BINARY_DIR}/reference)
    set_tests_properties(reference_compare PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 300
      ENVIRONMENT "CHR_SCRATCH=${CMAKE_BINARY_DIR}/reference-scratch;SDL_VIDEO_DRIVER=dummy;SDL_AUDIO_DRIVER=dummy")
  endif()
  if(WIN32)
    # SDL3.dll beside chr.exe.
    add_custom_command(TARGET chr POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:SDL3::SDL3> $<TARGET_FILE_DIR:chr>)
  endif()
else()
  message(STATUS "SDL3 not found: chr is not built (macOS: brew install sdl3)")
endif()
