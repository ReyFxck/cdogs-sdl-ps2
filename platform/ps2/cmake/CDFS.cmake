find_package(Python3 COMPONENTS Interpreter REQUIRED)
set(PS2SDKSRC "$ENV{PS2SDKSRC}" CACHE PATH "PS2SDK sources for the game-local CDFS module")
if(NOT PS2SDKSRC)
  set(PS2SDKSRC "${PROJECT_SOURCE_DIR}/.ps2deps/ps2sdk")
endif()
if(NOT EXISTS "${PS2SDKSRC}/iop/cdvd/cdfs/src/cdfs_iop.c")
  message(FATAL_ERROR "CDFS requires PS2SDK sources. Run bootstrap.py or set PS2SDKSRC.")
endif()
find_program(CDOGS_IOP_MAKE make REQUIRED)
set(cdfs_dir "${PROJECT_BINARY_DIR}/generated/cdfs")
set(cdfs_irx "${cdfs_dir}/irx/cdogs_cdfs.irx")
add_custom_command(OUTPUT "${cdfs_irx}"
  COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/platform/ps2/tools/prepare_cdfs.py"
    "${PS2SDKSRC}" "${cdfs_dir}/src"
  COMMAND "${CMAKE_COMMAND}" -E copy
    "${PROJECT_SOURCE_DIR}/platform/ps2/cdfs/build.mk" "${cdfs_dir}/Makefile"
  COMMAND "${CMAKE_COMMAND}" -E env "PS2SDKSRC=${PS2SDKSRC}"
    "${CDOGS_IOP_MAKE}" -C "${cdfs_dir}"
  DEPENDS "${PROJECT_SOURCE_DIR}/platform/ps2/tools/prepare_cdfs.py"
    "${PROJECT_SOURCE_DIR}/platform/ps2/cdfs/build.mk"
    "${PS2SDKSRC}/iop/cdvd/cdfs/src/main.c"
    "${PS2SDKSRC}/iop/cdvd/cdfs/src/cdfs_iop.c"
    "${PS2SDKSRC}/iop/cdvd/cdfs/src/cdfs_iop.h"
    "${PS2SDKSRC}/iop/cdvd/cdfs/src/imports.lst"
    "${PS2SDKSRC}/iop/cdvd/cdfs/src/irx_imports.h"
  VERBATIM)
add_custom_command(OUTPUT "${PROJECT_BINARY_DIR}/generated/cdogs_cdfs_irx.c"
  COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/platform/ps2/tools/embed_irx.py"
    "${cdfs_irx}" "${PROJECT_BINARY_DIR}/generated/cdogs_cdfs_irx.c" cdogs_cdfs_irx
  DEPENDS "${cdfs_irx}" "${PROJECT_SOURCE_DIR}/platform/ps2/tools/embed_irx.py"
  VERBATIM)
target_sources(cdogs-sdl PRIVATE "${PROJECT_BINARY_DIR}/generated/cdogs_cdfs_irx.c")
set_source_files_properties("${PROJECT_BINARY_DIR}/generated/cdogs_cdfs_irx.c"
  TARGET_DIRECTORY cdogs-sdl PROPERTIES GENERATED TRUE)
add_custom_target(cdogs_cdfs_blob DEPENDS "${PROJECT_BINARY_DIR}/generated/cdogs_cdfs_irx.c")
add_dependencies(cdogs-sdl cdogs_cdfs_blob)
