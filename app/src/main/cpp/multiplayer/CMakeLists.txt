cmake_minimum_required(VERSION 3.12)

project(multiplayer LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(${ANDROID_ABI} STREQUAL "armeabi-v7a")
    add_definitions(-DVER_x32=true)
    set(GLOSSHOOK_ABI "ARM")
elseif(${ANDROID_ABI} STREQUAL "arm64-v8a")
    add_definitions(-DVER_x32=false)
    set(GLOSSHOOK_ABI "ARM64")
endif()

# Reordered multiplayer tree based on the provided cpp.zip SRC layout.
# Header rule from user: keep headers as .h only.
file(GLOB_RECURSE MULTIPLAYER_SOURCES CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_LIST_DIR}/*.c"
        "${CMAKE_CURRENT_LIST_DIR}/*.cc"
        "${CMAKE_CURRENT_LIST_DIR}/*.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/*.cxx"
)

# Safety filter for stale duplicate files left behind when users copy this
# multiplayer folder over an older folder instead of deleting/replacing it.
# These __dup_* files were produced by earlier reorder rounds and cause
# duplicate linker symbols such as CQuaternion::SetFromMatrix/GetMatrix/Normalize/Slerp.
list(FILTER MULTIPLAYER_SOURCES EXCLUDE REGEX ".*__dup_[0-9A-Fa-f]+\\.(c|cc|cpp|cxx)$")
list(FILTER MULTIPLAYER_SOURCES EXCLUDE REGEX ".*/samp/Multiplayer/(CFPSFix|Patches|Hooks)\\.cpp$")

# Keep target/output name as `samp` because Java/native loader expects libsamp.so.
add_library(samp SHARED ${MULTIPLAYER_SOURCES})

# Auto-include all source/header directories after path reordering.
# This keeps old basename includes working while the tree matches the reference layout.
file(GLOB_RECURSE MULTIPLAYER_HEADERS CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_LIST_DIR}/*.h"
        "${CMAKE_CURRENT_LIST_DIR}/*.hh"
        "${CMAKE_CURRENT_LIST_DIR}/*.inc"
)
list(FILTER MULTIPLAYER_HEADERS EXCLUDE REGEX ".*__dup_[0-9A-Fa-f]+\\.(h|hh|inc)$")
set(MULTIPLAYER_INCLUDE_DIRS
        "${CMAKE_CURRENT_LIST_DIR}"
        "${CMAKE_CURRENT_LIST_DIR}/../opus/include"
)
foreach(_hdr ${MULTIPLAYER_HEADERS})
    get_filename_component(_dir "${_hdr}" DIRECTORY)
    list(APPEND MULTIPLAYER_INCLUDE_DIRS "${_dir}")
endforeach()
list(REMOVE_DUPLICATES MULTIPLAYER_INCLUDE_DIRS)

target_include_directories(samp PRIVATE ${MULTIPLAYER_INCLUDE_DIRS})

target_link_libraries(samp
        log
        android
        EGL
        GLESv3
        opus
        shadowhook::shadowhook
        ${CMAKE_CURRENT_SOURCE_DIR}/vendor/bass/libs/${ANDROID_ABI}/libbass.so
        ${CMAKE_CURRENT_SOURCE_DIR}/vendor/bass/libs/${ANDROID_ABI}/libbass_ssl.so
        ${CMAKE_CURRENT_SOURCE_DIR}/vendor/patch/libs/${GLOSSHOOK_ABI}/libGlossHook.a
        ${CMAKE_CURRENT_SOURCE_DIR}/vendor/patch/libs/${GLOSSHOOK_ABI}/libGlossHook.so
)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -w -s -g -fvisibility=hidden -pthread -Wall -O3 -fexceptions -fstack-protector-strong")

target_compile_options(samp PRIVATE
        $<$<CONFIG:Debug>:-fvisibility=default>
        $<$<CONFIG:Release>:-fvisibility=hidden>
)

target_link_options(samp PRIVATE "-Wl,-z,max-page-size=16384")
