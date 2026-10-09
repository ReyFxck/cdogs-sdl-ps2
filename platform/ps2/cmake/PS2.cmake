# Included only by the PS2 cross build. Reuse the game's source lists.
if(NOT PS2 OR NOT CMAKE_CROSSCOMPILING)
  message(FATAL_ERROR "Use -DCMAKE_TOOLCHAIN_FILE=$PS2DEV/share/ps2dev.cmake")
endif()
set(BUILD_EDITOR OFF CACHE BOOL "No editor on PS2" FORCE)
set(BUILD_TESTING OFF CACHE BOOL "Host tests are a separate build" FORCE)
set(USE_SHARED_ENET ON)
set(ENet_LIBRARIES "")
set(EXTRA_LIBRARIES m)
set(NANOPB_INCLUDE_DIRS "${PROJECT_SOURCE_DIR}/src/proto/nanopb")
set(CDOGS_DATA_DIR "./")
set(CDOGS_CFG_DIR "config/")
set(CDOGS_CONFIG_HEADER "${PROJECT_BINARY_DIR}/generated/cdogs/sys_config.h")
configure_file(src/cdogs/sys_config.h.cmake "${CDOGS_CONFIG_HEADER}")

set(CDOGS_PS2_AUDIO "OFF" CACHE STRING "PS2 audio: OFF or RFAUDS2")
set_property(CACHE CDOGS_PS2_AUDIO PROPERTY STRINGS OFF RFAUDS2)
if(NOT CDOGS_PS2_AUDIO STREQUAL "OFF" AND NOT CDOGS_PS2_AUDIO STREQUAL "RFAUDS2")
  message(FATAL_ERROR "CDOGS_PS2_AUDIO must be OFF or RFAUDS2")
endif()
find_package(SDL2 CONFIG REQUIRED)
if(NOT TARGET SDL2::SDL2 AND TARGET SDL2::SDL2-static)
  add_library(SDL2::SDL2 ALIAS SDL2::SDL2-static)
endif()
include_directories(BEFORE
  "${PROJECT_SOURCE_DIR}/platform/ps2/include"
  "${PROJECT_BINARY_DIR}/generated"
  "${PROJECT_BINARY_DIR}/generated/cdogs")
include_directories(src src/cdogs)
add_compile_definitions(CDOGS_PS2 NDEBUG MINIZ_NO_MMAP)
add_compile_options(-std=gnu99 -fsigned-char -freg-struct-return -ffunction-sections -fdata-sections
  -Wall -Wextra -Wno-unused-parameter -Wno-unused-function
  -Wno-incompatible-pointer-types)
add_link_options(-Wl,--gc-sections)

add_library(cdogs_ps2_audio STATIC platform/ps2/mixer.c)
target_link_libraries(cdogs_ps2_audio PUBLIC SDL2::SDL2)
if(CDOGS_PS2_AUDIO STREQUAL "RFAUDS2")
  include("${PROJECT_SOURCE_DIR}/platform/ps2/cmake/RFAuds2.cmake")
endif()

add_subdirectory(src)
target_sources(cdogs-sdl PRIVATE platform/ps2/main.c platform/ps2/platform.c
  platform/ps2/posix_paths.c platform/ps2/rwops.c platform/ps2/memory.c)
set_source_files_properties("${PROJECT_SOURCE_DIR}/src/cdogs.c"
  TARGET_DIRECTORY cdogs-sdl PROPERTIES COMPILE_DEFINITIONS "main=CDogsMain")
target_link_libraries(cdogs-sdl SDL2::SDL2main)
set_target_properties(cdogs-sdl PROPERTIES SUFFIX ".elf"
  RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}"
  RUNTIME_OUTPUT_DIRECTORY_RELEASE "${PROJECT_BINARY_DIR}"
  RUNTIME_OUTPUT_DIRECTORY_DEBUG "${PROJECT_BINARY_DIR}")
target_link_options(cdogs-sdl PRIVATE "-Wl,-Map,${PROJECT_BINARY_DIR}/cdogs-sdl.map")
target_link_options(cdogs-sdl PRIVATE "-Wl,--wrap=SDL_RWFromFile")
# tinydir and JSON loading use large local buffers, including recursive loads.
target_link_options(cdogs-sdl PRIVATE "-Wl,--defsym,_stack_size=1048576")
install(TARGETS cdogs-sdl RUNTIME DESTINATION .)
message(STATUS "PS2 game only; SDL2 video/pad; offline; audio=${CDOGS_PS2_AUDIO}")
