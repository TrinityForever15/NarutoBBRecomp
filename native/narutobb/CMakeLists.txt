# narutobb - ReXGlue Recompiled Project
#
# SDK boilerplate lives in generated/rexglue.cmake.

cmake_minimum_required(VERSION 3.25)
project(narutobb LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(NARUTOBB_PROFILE_GUEST_FUNCTIONS
       "Emit Tracy zones for every recompiled guest function" OFF)

if(NARUTOBB_PROFILE_GUEST_FUNCTIONS)
    # Release builds normally compile Tracy out of the runtime. Keep the
    # profiling build optimized while enabling the profiler lifecycle and
    # generated guest-function zones for this explicit diagnostic variant.
    add_compile_definitions(REXGLUE_ENABLE_PROFILING)
endif()

include(generated/rexglue.cmake)

# Sources
set(NARUTOBB_SOURCES
    src/main.cpp
    src/hooks.cpp
)

# The official v0.8.0 Windows package omitted xboxkrnl_usbcam.cpp. The source
# SDK includes it after our project patch, so only add the local copy when
# linking against the prebuilt package.
if(NOT REXSDK_DIR)
    list(APPEND NARUTOBB_SOURCES src/usbcam_stubs.cpp)
endif()

if(WIN32)
    add_executable(narutobb WIN32 ${NARUTOBB_SOURCES})
else()
    add_executable(narutobb ${NARUTOBB_SOURCES})
endif()

rexglue_setup_target(narutobb)

if(NARUTOBB_PROFILE_GUEST_FUNCTIONS)
    if(NOT REXGLUE_ENABLE_TRACY)
        message(FATAL_ERROR
            "NARUTOBB_PROFILE_GUEST_FUNCTIONS requires REXGLUE_ENABLE_TRACY")
    endif()
    target_compile_definitions(narutobb PRIVATE
        REXGLUE_PROFILE_GUEST_FUNCTIONS)
endif()

# Headless GPU trace replay utility used to turn F9 captures into PNGs. The
# SDK contains the replay implementation, but doesn't build a standalone tool.
if(WIN32 AND REXSDK_DIR)
    add_executable(narutobb_trace_dump
        tools/trace_dump_main.cpp
        "${REXSDK_DIR}/src/graphics/trace_dump.cpp"
        "${REXSDK_DIR}/src/graphics/trace_player.cpp"
        "${REXSDK_DIR}/src/graphics/trace_reader.cpp"
    )
    target_link_libraries(narutobb_trace_dump PRIVATE rex::runtime snappy)
    rexglue_apply_target_settings(narutobb_trace_dump)
endif()

# When consuming the SDK directly from its source tree, rex_app.cpp is added
# to this executable but the private ImGui include directory is not propagated.
if(REXSDK_DIR)
    target_include_directories(narutobb PRIVATE
        "${REXSDK_DIR}/thirdparty/imgui"
        "${CMAKE_CURRENT_BINARY_DIR}/rexglue-sdk/include"
    )
endif()
