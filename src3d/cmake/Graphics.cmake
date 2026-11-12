set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_GAMES OFF CACHE BOOL "" FORCE)
set(CUSTOMIZE_BUILD ON CACHE BOOL "" FORCE)
set(SUPPORT_FILEFORMAT_TTF ON CACHE BOOL "" FORCE)
set(SUPPORT_MODULE_RAUDIO OFF CACHE BOOL "" FORCE)
add_subdirectory("${RS_raylib_SOURCE_DIR}" raylib)
target_include_directories(raylib SYSTEM INTERFACE "${RS_raygui_SOURCE_DIR}/src")


# Pinned raylib 5.5 CUSTOMIZE_BUILD scans both conditional config.h definitions,
# emitting MAX_MESH_VERTEX_BUFFERS=9 and =7 together while GPU skinning is on.
# UploadMesh writes slots 7/8; retain one correctly sized definition.
get_target_property(rs_raylib_definitions raylib COMPILE_DEFINITIONS)
list(FILTER rs_raylib_definitions EXCLUDE REGEX "^MAX_MESH_VERTEX_BUFFERS=")
list(APPEND rs_raylib_definitions MAX_MESH_VERTEX_BUFFERS=9)
set_property(TARGET raylib PROPERTY COMPILE_DEFINITIONS "${rs_raylib_definitions}")
