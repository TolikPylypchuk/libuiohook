cmake_minimum_required(VERSION 4.2)

project(uiohook VERSION 2.0.0 LANGUAGES C)

set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS 1)

if (WIN32 OR WIN64)
    set(UIOHOOK_SOURCE_DIR "windows")
elseif (APPLE)
    set(UIOHOOK_SOURCE_DIR "macos")
else()
    set(UIOHOOK_SOURCE_DIR "linux/legacy")
endif()

if (WIN32 OR WIN64)
    add_library(uiohook SHARED
        "src/logger.c"
        "src/${UIOHOOK_SOURCE_DIR}/dispatch_event.c"
        "src/${UIOHOOK_SOURCE_DIR}/input_helper.c"
        "src/${UIOHOOK_SOURCE_DIR}/input_hook.c"
        "src/${UIOHOOK_SOURCE_DIR}/monitor_helper.c"
        "src/${UIOHOOK_SOURCE_DIR}/post_event.c"
        "src/${UIOHOOK_SOURCE_DIR}/system_properties.c"
        "src/${UIOHOOK_SOURCE_DIR}/unused_functions.c"
    )
elseif (APPLE)
    add_library(uiohook SHARED
        "src/logger.c"
        "src/${UIOHOOK_SOURCE_DIR}/dispatch_event.c"
        "src/${UIOHOOK_SOURCE_DIR}/input_helper.c"
        "src/${UIOHOOK_SOURCE_DIR}/input_hook.c"
        "src/${UIOHOOK_SOURCE_DIR}/post_event.c"
        "src/${UIOHOOK_SOURCE_DIR}/system_properties.c"
        "src/${UIOHOOK_SOURCE_DIR}/unused_functions.c"
    )
else()
    add_library(uiohook-x11 SHARED
        "src/logger.c"
        "src/linux/x11/dispatch_event.c"
        "src/linux/x11/input_helper.c"
        "src/linux/x11/input_hook.c"
        "src/linux/x11/post_event.c"
        "src/linux/x11/system_properties.c"
        "src/linux/x11/unused_functions.c"
    )

    set_target_properties(uiohook-x11 PROPERTIES
        C_STANDARD 23
        C_STANDARD_REQUIRED ON
        POSITION_INDEPENDENT_CODE 1
        OUTPUT_NAME "uiohook-x11"
        PUBLIC_HEADER ${CMAKE_CURRENT_SOURCE_DIR}/include/uiohook.h
    )

    add_library(uiohook-wayland SHARED
        "src/logger.c"
        "src/linux/wayland/dispatch_event.c"
        "src/linux/wayland/input_helper.c"
        "src/linux/wayland/input_hook.c"
        "src/linux/wayland/monitor_helper.c"
        "src/linux/wayland/post_event.c"
        "src/linux/wayland/system_properties.c"
        "src/linux/wayland/unused_functions.c"
    )

    set_target_properties(uiohook-wayland PROPERTIES
        C_STANDARD 23
        C_STANDARD_REQUIRED ON
        POSITION_INDEPENDENT_CODE 1
        OUTPUT_NAME "uiohook-wayland"
        PUBLIC_HEADER ${CMAKE_CURRENT_SOURCE_DIR}/include/uiohook.h
    )

    add_library(uiohook-legacy SHARED
        "src/logger.c"
        "src/linux/legacy/dispatch_event.c"
        "src/linux/legacy/input_helper.c"
        "src/linux/legacy/input_hook.c"
        "src/linux/legacy/post_event.c"
        "src/linux/legacy/system_properties.c"
        "src/linux/legacy/unused_functions.c"
    )

    set_target_properties(uiohook-legacy PROPERTIES
        C_STANDARD 23
        C_STANDARD_REQUIRED ON
        POSITION_INDEPENDENT_CODE 1
        OUTPUT_NAME "uiohook-legacy"
        PUBLIC_HEADER ${CMAKE_CURRENT_SOURCE_DIR}/include/uiohook.h
    )

    add_library(uiohook SHARED
        "src/linux/loader.c"
    )
endif()

set_target_properties(uiohook PROPERTIES
    C_STANDARD 23
    C_STANDARD_REQUIRED ON
    POSITION_INDEPENDENT_CODE 1
    OUTPUT_NAME "${PROJECT_NAME}"
    PUBLIC_HEADER ${CMAKE_CURRENT_SOURCE_DIR}/include/uiohook.h
)

include(GNUInstallDirs)

if (UNIX AND NOT APPLE)
    target_include_directories(uiohook-x11
        PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
            $<INSTALL_INTERFACE:include>

        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src
            ${CMAKE_CURRENT_SOURCE_DIR}/src/linux/x11
    )

    target_include_directories(uiohook-wayland
        PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
            $<INSTALL_INTERFACE:include>

        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src
            ${CMAKE_CURRENT_SOURCE_DIR}/src/linux/wayland
    )

    target_include_directories(uiohook-legacy
        PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
            $<INSTALL_INTERFACE:include>

        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src
            ${CMAKE_CURRENT_SOURCE_DIR}/src/linux/legacy
    )

    target_include_directories(uiohook
        PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
            $<INSTALL_INTERFACE:include>

        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src
            ${CMAKE_CURRENT_SOURCE_DIR}/src/linux
    )
else()
    target_include_directories(uiohook
        PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
            $<INSTALL_INTERFACE:include>

        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src
            ${CMAKE_CURRENT_SOURCE_DIR}/src/${UIOHOOK_SOURCE_DIR}
    )
endif()

if (UNIX AND NOT APPLE)
    install(TARGETS uiohook uiohook-x11 uiohook-wayland uiohook-legacy
        EXPORT ${PROJECT_NAME}-config
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
        RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
    )

    export(TARGETS uiohook uiohook-x11 uiohook-wayland uiohook-legacy FILE "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}-config.cmake")
    install(EXPORT ${PROJECT_NAME}-config DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME})
else()
    install(TARGETS uiohook
        EXPORT ${PROJECT_NAME}-config
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
        RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
    )

    export(TARGETS uiohook FILE "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}-config.cmake")
    install(EXPORT ${PROJECT_NAME}-config DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME})
endif()

if(UNIX AND NOT APPLE)
    find_package(PkgConfig REQUIRED)

    find_package(ECM REQUIRED NO_MODULE)
    set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${ECM_MODULE_PATH}")

    target_link_libraries(uiohook PRIVATE dl)

    pkg_check_modules(Libinput REQUIRED libinput)
    target_include_directories(uiohook-x11 PRIVATE ${LIBINPUT_INCLUDE_DIRS})
    target_link_libraries(uiohook-x11 ${LIBINPUT_LIBRARIES})
    target_include_directories(uiohook-wayland PRIVATE ${LIBINPUT_INCLUDE_DIRS})
    target_link_libraries(uiohook-wayland ${LIBINPUT_LIBRARIES})

    pkg_check_modules(LIBUDEV REQUIRED libudev)
    target_include_directories(uiohook-x11 PRIVATE ${LIBUDEV_INCLUDE_DIRS})
    target_link_libraries(uiohook-x11 ${LIBUDEV_LIBRARIES})
    target_include_directories(uiohook-wayland PRIVATE ${LIBUDEV_INCLUDE_DIRS})
    target_link_libraries(uiohook-wayland ${LIBUDEV_LIBRARIES})

    pkg_check_modules(X11 REQUIRED x11)
    target_include_directories(uiohook-x11 PRIVATE "${X11_INCLUDE_DIRS}")
    target_link_libraries(uiohook-x11 "${X11_LDFLAGS}")
    target_include_directories(uiohook-legacy PRIVATE "${X11_INCLUDE_DIRS}")
    target_link_libraries(uiohook-legacy "${X11_LDFLAGS}")

    pkg_check_modules(XTST REQUIRED xtst)
    target_include_directories(uiohook-x11 PRIVATE "${XTST_INCLUDE_DIRS}")
    target_link_libraries(uiohook-x11 "${XTST_LDFLAGS}")
    target_include_directories(uiohook-legacy PRIVATE "${XTST_INCLUDE_DIRS}")
    target_link_libraries(uiohook-legacy "${XTST_LDFLAGS}")

    include(CheckLibraryExists)
    check_library_exists(Xtst XRecordQueryVersion "" HAVE_XRECORD)

    include(CheckIncludeFile)
    check_include_file(X11/extensions/record.h HAVE_RECORD_H "-include X11/Xlib.h")

    pkg_check_modules(XKB_COMMON REQUIRED xkbcommon-x11)
    target_include_directories(uiohook-x11 PRIVATE "${XKB_COMMON_INCLUDE_DIRS}")
    target_link_libraries(uiohook-x11 "${XKB_COMMON_LDFLAGS}")
    target_include_directories(uiohook-legacy PRIVATE "${XKB_COMMON_INCLUDE_DIRS}")
    target_link_libraries(uiohook-legacy "${XKB_COMMON_LDFLAGS}")

    pkg_check_modules(X11_XCB REQUIRED x11-xcb)
    target_include_directories(uiohook-x11 PRIVATE "${X11_XCB_INCLUDE_DIRS}")
    target_link_libraries(uiohook-x11 "${X11_XCB_LDFLAGS}")
    target_include_directories(uiohook-legacy PRIVATE "${X11_XCB_INCLUDE_DIRS}")
    target_link_libraries(uiohook-legacy "${X11_XCB_LDFLAGS}")

    pkg_check_modules(XT REQUIRED xt)
    target_include_directories(uiohook-x11 PRIVATE "${XT_INCLUDE_DIRS}")
    target_link_libraries(uiohook-x11 "${XT_LDFLAGS}")
    target_include_directories(uiohook-legacy PRIVATE "${XT_INCLUDE_DIRS}")
    target_link_libraries(uiohook-legacy "${XT_LDFLAGS}")

    pkg_check_modules(XRANDR REQUIRED xrandr)
    target_include_directories(uiohook-x11 PRIVATE "${XRANDR_INCLUDE_DIRS}")
    target_link_libraries(uiohook-x11 "${XRANDR_LDFLAGS}")
    target_include_directories(uiohook-legacy PRIVATE "${XRANDR_INCLUDE_DIRS}")
    target_link_libraries(uiohook-legacy "${XRANDR_LDFLAGS}")

    pkg_check_modules(WAYLAND_CLIENT REQUIRED wayland-client)
    target_include_directories(uiohook-wayland PRIVATE "${WAYLAND_CLIENT_INCLUDE_DIRS}")
    target_link_libraries(uiohook-wayland "${WAYLAND_CLIENT_LIBRARIES}")

    pkg_check_modules(XKB_COMMON_WAYLAND REQUIRED xkbcommon)
    target_include_directories(uiohook-wayland PRIVATE "${XKB_COMMON_WAYLAND_INCLUDE_DIRS}")
    target_link_libraries(uiohook-wayland "${XKB_COMMON_WAYLAND_LIBRARIES}")

    find_package(WaylandProtocols REQUIRED)
    find_package(WaylandScanner REQUIRED)

    ecm_add_wayland_client_protocol(
        WAYLAND_SOURCES                              
        PROTOCOL ${WaylandProtocols_DATADIR}/unstable/xdg-output/xdg-output-unstable-v1.xml
        BASENAME xdg-output-unstable-v1
    )

    target_sources(uiohook-wayland PRIVATE ${WAYLAND_SOURCES})
    target_include_directories(uiohook-wayland PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
elseif(APPLE)
    set(CMAKE_MACOSX_RPATH 1)

    option(MAC_CATALYST "ApplicationServices framework (default: OFF)" OFF)
    if(MAC_CATALYST)
        # Mac Catalyst does not support the Application Services framework, so we need to link against other required libraries.

        find_library(CORE_FOUNDATION CoreFoundation REQUIRED)
        target_include_directories(uiohook PRIVATE "${CORE_FOUNDATION}")
        target_link_libraries(uiohook "${CORE_FOUNDATION}")

        find_library(CORE_GRAPHICS CoreGraphics REQUIRED)
        target_include_directories(uiohook PRIVATE "${CORE_GRAPHICS}")
        target_link_libraries(uiohook "${CORE_GRAPHICS}")

        add_compile_definitions(MAC_CATALYST)
    else()
        find_package(Threads REQUIRED)
        target_link_libraries(uiohook "${CMAKE_THREAD_LIBS_INIT}")

        find_library(CARBON Carbon REQUIRED)
        target_include_directories(uiohook PRIVATE "${CARBON}")
        target_link_libraries(uiohook "${CARBON}")

        find_library(APPLICATION_SERVICES ApplicationServices REQUIRED)
        target_include_directories(uiohook PRIVATE "${APPLICATION_SERVICES}")
        target_link_libraries(uiohook "${APPLICATION_SERVICES}")
    endif()

    find_library(IOKIT IOKit REQUIRED)
    target_include_directories(uiohook PRIVATE "${IOKIT}")
    target_link_libraries(uiohook "${IOKIT}")
elseif(WIN32)
    target_link_libraries(uiohook Advapi32)
endif()

option(BUILD_DEMO "Build demo applications (default: OFF)" OFF)
if (BUILD_DEMO)
    find_package(Threads REQUIRED)

    add_executable(demo_hook "./demo/demo_hook.c")
    add_dependencies(demo_hook uiohook)
    target_link_libraries(demo_hook uiohook "${CMAKE_THREAD_LIBS_INIT}")

    add_executable(demo_hook_async "./demo/demo_hook_async.c")
    add_dependencies(demo_hook_async uiohook)
    target_link_libraries(demo_hook_async uiohook "${CMAKE_THREAD_LIBS_INIT}")

    add_executable(demo_post "./demo/demo_post.c")
    add_dependencies(demo_post uiohook)
    target_link_libraries(demo_post uiohook "${CMAKE_THREAD_LIBS_INIT}")

    add_executable(demo_properties "./demo/demo_properties.c")
    add_dependencies(demo_properties uiohook)
    target_link_libraries(demo_properties uiohook "${CMAKE_THREAD_LIBS_INIT}")

    add_custom_target(all_demos DEPENDS
        demo_hook
        demo_hook_async
        demo_post
        demo_properties
    )

    set_target_properties(all_demos PROPERTIES
        C_STANDARD 99
        C_STANDARD_REQUIRED ON
    )

    if(MSVC)
        add_compile_definitions(all_demos PRIVATE inline=__inline)
        add_compile_definitions(all_demos PRIVATE _CRT_SECURE_NO_WARNINGS)
    endif()

    install(TARGETS demo_hook demo_hook_async demo_post demo_properties RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()

option(BUILD_TEST "Build tests (default: OFF)" OFF)
if(BUILD_TEST)
    add_executable(uiohook_tests
        "./test/input_helper_test.c"
        "./test/system_properties_test.c"
        "./test/minunit.h"
        "./test/uiohook_test.c"
    )

    set_target_properties(uiohook_tests PROPERTIES
        C_STANDARD 99
        C_STANDARD_REQUIRED ON
    )

    target_include_directories(uiohook_tests PRIVATE "./src/${UIOHOOK_SOURCE_DIR}")
    add_dependencies(uiohook_tests uiohook)
    target_link_libraries(uiohook_tests uiohook "${CMAKE_THREAD_LIBS_INIT}")

    if (UNIX AND NOT APPLE)
        add_dependencies(uiohook_tests uiohook-legacy)
        target_link_libraries(uiohook_tests uiohook-legacy)
    endif()
endif()

list(REMOVE_DUPLICATES INTERFACE_LINK_LIBRARIES)
string(REPLACE ";" " " COMPILE_LIBRARIES "${INTERFACE_LINK_LIBRARIES}")
configure_file("pc/uiohook.pc.in" "${PROJECT_BINARY_DIR}/uiohook.pc" @ONLY)
install(FILES "${PROJECT_BINARY_DIR}/uiohook.pc" DESTINATION "${CMAKE_INSTALL_LIBDIR}/pkgconfig")
