set(RFAUDS2_ROOT "" CACHE PATH "RFAuds2 checkout built with make")
find_path(RFAUDS2_INCLUDE_DIR rfauds2/rfauds2.h
  HINTS "${RFAUDS2_ROOT}/include" NO_DEFAULT_PATH NO_CMAKE_FIND_ROOT_PATH REQUIRED)
find_library(RFAUDS2_LIBRARY rfauds2
  HINTS "${RFAUDS2_ROOT}/build" "${RFAUDS2_ROOT}/build/package/lib"
  NO_DEFAULT_PATH NO_CMAKE_FIND_ROOT_PATH REQUIRED)
find_file(RFAUDS2_IRX rfauds2.irx
  HINTS "${RFAUDS2_ROOT}/build" "${RFAUDS2_ROOT}/build/package/iop"
  NO_DEFAULT_PATH NO_CMAKE_FIND_ROOT_PATH REQUIRED)
find_package(Python3 COMPONENTS Interpreter REQUIRED)
add_custom_command(OUTPUT "${PROJECT_BINARY_DIR}/generated/rfauds2_irx.c"
  COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/platform/ps2/tools/embed_irx.py"
    "${RFAUDS2_IRX}" "${PROJECT_BINARY_DIR}/generated/rfauds2_irx.c"
  DEPENDS "${RFAUDS2_IRX}" "${PROJECT_SOURCE_DIR}/platform/ps2/tools/embed_irx.py"
  VERBATIM)
target_sources(cdogs_ps2_audio PRIVATE "${PROJECT_BINARY_DIR}/generated/rfauds2_irx.c")
target_compile_definitions(cdogs_ps2_audio PUBLIC CDOGS_PS2_RFAUDS2)
target_include_directories(cdogs_ps2_audio PRIVATE "${RFAUDS2_INCLUDE_DIR}")
target_link_libraries(cdogs_ps2_audio PUBLIC "${RFAUDS2_LIBRARY}")
