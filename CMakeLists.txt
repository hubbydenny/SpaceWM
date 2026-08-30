cmake_minimum_required(VERSION 3.16)
project(spacewc CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fpermissive -Wno-invalid-offsetof")

add_definitions(-DWLR_USE_UNSTABLE -D_GLIBCXX_ASSERTIONS=1)

find_package(PkgConfig REQUIRED)
pkg_check_modules(PIXMAN REQUIRED pixman-1)
pkg_check_modules(XKB REQUIRED xkbcommon)
pkg_check_modules(EGL REQUIRED egl)
pkg_check_modules(GLES2 REQUIRED glesv2)

include_directories(
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/wlr
    ${PIXMAN_INCLUDE_DIRS}
    ${XKB_INCLUDE_DIRS}
    ${EGL_INCLUDE_DIRS}
    ${GLES2_INCLUDE_DIRS}
)

link_directories(/usr/lib/wlroots)

add_executable(spacewc-comp
    src/main_comp.cpp
    src/WaylandComp.cpp
    src/animations.cpp
)

target_link_libraries(spacewc-comp
    wlroots
    ${PIXMAN_LIBRARIES}
    ${XKB_LIBRARIES}
    ${EGL_LIBRARIES}
    ${GLES2_LIBRARIES}
    pthread dl
)

add_executable(spacewc
    src/main_client.cpp
    src/WaylandClient.cpp
)

target_link_libraries(spacewc
    wayland-client
    ${XKB_LIBRARIES}
    pthread dl
)
