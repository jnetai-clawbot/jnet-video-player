cmake_minimum_required(VERSION 3.20)
project(jnet-video-player C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Find packages
find_package(PkgConfig REQUIRED)

pkg_check_modules(WAYLAND REQUIRED wayland-client wayland-egl wayland-cursor)
pkg_check_modules(EGL REQUIRED egl)
pkg_check_modules(XKB REQUIRED xkbcommon)
pkg_check_modules(FFMPEG REQUIRED libavcodec libavformat libavutil libswscale libswresample)
pkg_check_modules(PULSE REQUIRED libpulse)

# Wayland protocol XMLs
set(WL_PROTOCOLS_DIR "/usr/share/wayland-protocols")
set(WAYLAND_PROTOCOLS "${WAYLAND_PROTOCOLS_DIR}")

# Generate wayland protocol code
find_program(WAYLAND_SCANNER wayland-scanner REQUIRED)

set(PROTOCOL_FILES
    "${WL_PROTOCOLS_DIR}/stable/xdg-shell/xdg-shell.xml"
    "${WL_PROTOCOLS_DIR}/staging/xdg-activation/xdg-activation-v1.xml"
)

set(PROTOCOL_SOURCES)
foreach(XML ${PROTOCOL_FILES})
    get_filename_component(NAME ${XML} NAME_WLE)
    set(CODE_FILE "${CMAKE_CURRENT_BINARY_DIR}/${NAME}.c")
    set(HEADER_FILE "${CMAKE_CURRENT_BINARY_DIR}/${NAME}.h")
    if(EXISTS ${XML})
        add_custom_command(
            OUTPUT ${CODE_FILE} ${HEADER_FILE}
            COMMAND ${WAYLAND_SCANNER} client-header ${XML} ${HEADER_FILE}
            COMMAND ${WAYLAND_SCANNER} private-code ${XML} ${CODE_FILE}
            DEPENDS ${XML}
        )
        list(APPEND PROTOCOL_SOURCES ${CODE_FILE})
    endif()
endforeach()

# Source files
set(SOURCES
    main.c
    player.c
    renderer.c
    ui.c
    playlist.c
    ${PROTOCOL_SOURCES}
)

set(CFLAGS
    -O3 -march=armv8-a+crc+simd -mtune=cortex-a76
    -flto -ffast-math
    -Wall -Wextra -Wno-unused-parameter
)

add_executable(jnet-video ${SOURCES})

target_include_directories(jnet-video PRIVATE
    ${CMAKE_CURRENT_BINARY_DIR}
    ${WAYLAND_INCLUDE_DIRS}
    ${EGL_INCLUDE_DIRS}
    ${XKB_INCLUDE_DIRS}
    ${FFMPEG_INCLUDE_DIRS}
    ${PULSE_INCLUDE_DIRS}
)

target_link_libraries(jnet-video PRIVATE
    ${WAYLAND_LIBRARIES}
    ${EGL_LIBRARIES}
    ${XKB_LIBRARIES}
    ${FFMPEG_LIBRARIES}
    ${PULSE_LIBRARIES}
    m pthread dl
)

target_compile_options(jnet-video PRIVATE ${CFLAGS})

# Install
install(TARGETS jnet-video RUNTIME DESTINATION bin)
