# sillydoom extensions — id DOOM engine as isilly extension
#
# Build:
#   cmake -S ext -B ext/build -DISILLY_ROOT=/path/to/sillystate/isilly
#   cmake --build ext/build

cmake_minimum_required(VERSION 3.10)
project(sillydoom_ext C)

set(CMAKE_C_STANDARD 11)

if(NOT DEFINED ISILLY_ROOT)
    set(ISILLY_ROOT "$ENV{SILLYSTATE}/isilly")
endif()

set(SDK_INCLUDES
    ${ISILLY_ROOT}/sdk/include
    ${ISILLY_ROOT}/include
    ${ISILLY_ROOT}/../arm64/include
)

# ── doom_engine: id DOOM engine (linuxdoom-1.10) + MUS/FM synth ────
file(GLOB DOOM_ENGINE_SOURCES doom_engine/*.c)
list(APPEND DOOM_ENGINE_SOURCES
    doom_audio/mus_parser.c
    doom_audio/midi_synth.c
    doom_audio/synth_fm.c)
add_library(doom_engine SHARED ${DOOM_ENGINE_SOURCES})
target_include_directories(doom_engine PRIVATE
    ${SDK_INCLUDES}
    ${CMAKE_CURRENT_SOURCE_DIR}/doom_engine
    ${CMAKE_CURRENT_SOURCE_DIR}/doom_audio
    ${ISILLY_ROOT}/src/ext/audio)
target_compile_definitions(doom_engine PRIVATE NORMALUNIX LINUX)
set_target_properties(doom_engine PROPERTIES PREFIX "" OUTPUT_NAME "doom_engine")
target_compile_options(doom_engine PRIVATE
    -Wno-implicit-function-declaration
    -Wno-implicit-int
    -Wno-int-conversion
    -Wno-incompatible-pointer-types
    -Wno-pointer-sign
    -Wno-return-type
    -Wno-unused-result
    -Wno-parentheses
    -Wno-dangling-else
    -Wno-shift-negative-value
    -Wno-format)

# Platform-specific linking
if(APPLE)
    target_link_options(doom_engine PRIVATE "-undefined" "dynamic_lookup")
    set_target_properties(doom_engine PROPERTIES
        INSTALL_RPATH "@loader_path" BUILD_WITH_INSTALL_RPATH TRUE)
elseif(UNIX)
    target_link_options(doom_engine PRIVATE "-Wl,--no-as-needed")
endif()

target_link_libraries(doom_engine PRIVATE m)

# Copy to parent ext/ for require() resolution
add_custom_command(TARGET doom_engine POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:doom_engine>" "${CMAKE_CURRENT_SOURCE_DIR}/"
    COMMENT "doom_engine → ext/")
