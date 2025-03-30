cmake_minimum_required(VERSION 3.10...3.31.6)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")
include(GitVersion)
message(STATUS "Building version ${FULL_VERSION}")

string(TIMESTAMP YEAR "%Y")
string(TIMESTAMP BUILD_DATE "%Y-%m-%d")

project(nyukomatic VERSION ${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH})

set(PROJECT_URL "https://github.com/alexanderk23/nyukomatic")
set(PROJECT_COPYRIGHT "Copyright © 2024-${YEAR} The Nyukomatic Contributors. All rights reserved.")

set(NM_EXE_NAME ${CMAKE_PROJECT_NAME})

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_C_FLAGS_RELEASE_INIT "-O3")
set(CMAKE_CXX_FLAGS_RELEASE_INIT "-O3")
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_INSTALL_PREFIX "${CMAKE_BINARY_DIR}")

set(USE_TLS ON)
set(USE_OPEN_SSL ON)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)

if(APPLE OR WIN32)
    ############################################################################
    # Windows
    if(WIN32)
        if(MSVC)
            add_compile_options("$<$<CONFIG:Release>:/MT>")
            # add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
        endif()
        set(PLATFORM_LIBS opengl32 glu32 winmm shlwapi)
    endif()

    ############################################################################
    # MacOS
    if(APPLE)
        if(CMAKE_OSX_ARCHITECTURES STREQUAL "")
            if(CMAKE_SYSTEM_PROCESSOR STREQUAL arm64)
                set(CMAKE_OSX_ARCHITECTURES arm64)
            else()
                set(CMAKE_OSX_ARCHITECTURES x86_64)
            endif()
        endif()
        if(CMAKE_OSX_ARCHITECTURES MATCHES x86_64)
            set(USE_OPEN_SSL OFF)
            set(CMAKE_OSX_DEPLOYMENT_TARGET "11.7")
        else()
            set(CMAKE_OSX_DEPLOYMENT_TARGET "15.0")
        endif()
        message(STATUS "Target macOS version is ${CMAKE_OSX_DEPLOYMENT_TARGET} ${CMAKE_OSX_ARCHITECTURES}")

        set(CMAKE_FIND_FRAMEWORK LAST)
        set(CMAKE_XCODE_ATTRIBUTE_CLANG_CXX_LANGUAGE_STANDARD "c++17")
        set(CMAKE_XCODE_ATTRIBUTE_CLANG_CXX_LIBRARY "libc++")
        set(CMAKE_XCODE_ATTRIBUTE_GCC_ENABLE_CPP_EXCEPTIONS "No")
        set(CMAKE_XCODE_ATTRIBUTE_GCC_ENABLE_CPP_RTTI "No")

        set(GUI_TYPE MACOSX_BUNDLE)
        set(NM_PROJECT_SRCS ${GUI_TYPE} ${NM_PROJECT_SRCS})

        find_library(COCOA_FRAMEWORK Cocoa)
        find_library(OPENGL_FRAMEWORK OpenGL)
        find_library(IOKIT_FRAMEWORK IOKit)
        find_library(COREVIDEO_FRAMEWORK CoreVideo)
        set(PLATFORM_LIBS ${COCOA_FRAMEWORK} ${OPENGL_FRAMEWORK} ${IOKIT_FRAMEWORK} ${COREVIDEO_FRAMEWORK})
    endif()
else()
    ##############################################################################
    # Linux / BSD / Unix
    set(CMAKE_INSTALL_RPATH "$ORIGIN/../lib")
    find_package(OpenGL COMPONENTS OpenGL REQUIRED)
    set(PLATFORM_LIBS ${OPENGL_LIBRARIES} fontconfig)
    if(NOT "${CMAKE_SYSTEM_NAME}" STREQUAL "OpenBSD")
        list(APPEND PLATFORM_LIBS asound)
    endif()
endif()

##############################################################################
# Vendor Libs
##############################################################################

##############################################################################
# xxhash_cpp
set(NM_PROJECT_INCLUDES ${NM_PROJECT_INCLUDES} vendor/xxhash_cpp/include)

##############################################################################
# sse2neon
if(APPLE)
    if(${CMAKE_SYSTEM_PROCESSOR} STREQUAL "arm64")
        add_compile_definitions(SSE2NEON_SUPPRESS_WARNINGS)
        set(NM_PROJECT_INCLUDES ${NM_PROJECT_INCLUDES} ${CMAKE_SOURCE_DIR}/vendor/sse2neon)
    endif()
endif()

##############################################################################
# GLFW
# GLFW settings and project inclusion
set(GLFW_DIR ${CMAKE_SOURCE_DIR}/vendor/glfw)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
set(GLFW_VULKAN_STATIC OFF CACHE BOOL "" FORCE)
mark_as_advanced(BUILD_SHARED_LIBS GLFW_BUILD_EXAMPLES GLFW_BUILD_TESTS GLFW_BUILD_DOCS GLFW_INSTALL GLFW_VULKAN_STATIC)
if(UNIX)
    set(GLFW_USE_OSMESA OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_WAYLAND OFF CACHE BOOL "" FORCE)
    mark_as_advanced(GLFW_USE_OSMESA GLFW_BUILD_WAYLAND)
endif()
if(WIN32)
    set(USE_MSVC_RUNTIME_LIBRARY_DLL OFF CACHE BOOL "" FORCE)
    mark_as_advanced(USE_MSVC_RUNTIME_LIBRARY_DLL)
endif()
add_subdirectory(${GLFW_DIR})
set(NM_PROJECT_LIBS ${NM_PROJECT_LIBS} glfw)

##############################################################################
# GLEW
set(GLEW_SRCS ${CMAKE_SOURCE_DIR}/vendor/glew/glew.c)
add_library(glew STATIC ${GLEW_SRCS})
target_include_directories(glew PUBLIC ${CMAKE_SOURCE_DIR}/vendor/glew)
target_compile_definitions(glew PUBLIC GLEW_STATIC)
if(MSVC)
    target_compile_options(glew PUBLIC "$<$<CONFIG:Release>:/MT>")
endif()
set(NM_PROJECT_LIBS ${NM_PROJECT_LIBS} glew)

##############################################################################
# Freetype
set(FREETYPE_DIR ${CMAKE_SOURCE_DIR}/vendor/freetype)
set(FT_DISABLE_PNG ON CACHE BOOL "" FORCE)
set(FT_DISABLE_BZIP2 ON CACHE BOOL "" FORCE)
set(FT_DISABLE_ZLIB ON CACHE BOOL "" FORCE)
set(FT_DISABLE_BROTLI ON CACHE BOOL "" FORCE)
set(FT_DISABLE_HARFBUZZ ON CACHE BOOL "" FORCE)
add_subdirectory(${FREETYPE_DIR} EXCLUDE_FROM_ALL)
if(MSVC)
    target_compile_options(freetype PUBLIC "$<$<CONFIG:Release>:/MT>")
endif()

##############################################################################
# IMGUI
set(IMGUI_DIR ${CMAKE_SOURCE_DIR}/vendor/imgui)
set(IMGUI_SRCS
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
    ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
    ${IMGUI_DIR}/misc/freetype/imgui_freetype.cpp
    ${IMGUI_DIR}/misc/cpp/imgui_stdlib.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp)
if(DEBUG)
    set(IMGUI_SRCS ${IMGUI_SRCS} ${IMGUI_DIR}/imgui_demo.cpp)
endif()
add_library(imgui STATIC ${IMGUI_SRCS})
target_include_directories(imgui PUBLIC
    ${GLFW_DIR}/include
    ${FREETYPE_DIR}/include
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
    ${IMGUI_DIR}/misc/freetype
    ${IMGUI_DIR}/misc/cpp)
if(MSVC)
    target_compile_options(imgui PUBLIC "$<$<CONFIG:Release>:/MT>")
endif()
target_link_libraries(imgui freetype)
target_compile_definitions(imgui PUBLIC IMGUI_ENABLE_FREETYPE)
set(NM_PROJECT_LIBS ${NM_PROJECT_LIBS} imgui)

##############################################################################
# Z80
set(Z80_DIR ${CMAKE_SOURCE_DIR}/vendor/z80)
set(NM_PROJECT_INCLUDES ${NM_PROJECT_INCLUDES} ${Z80_DIR})

##############################################################################
# SJASMPlus
set(SJASMPLUS_DIR ${CMAKE_SOURCE_DIR}/vendor/esjasmplus)
add_subdirectory(${SJASMPLUS_DIR} EXCLUDE_FROM_ALL)
if(MSVC)
    target_compile_options(esjasmplus PUBLIC "$<$<CONFIG:Release>:/MT>")
    target_compile_definitions(esjasmplus PUBLIC _CRT_SECURE_NO_WARNINGS)
elseif(APPLE)
    target_compile_options(esjasmplus PUBLIC -Wno-unused-command-line-argument -Wno-deprecated-declarations)
endif()
set(NM_PROJECT_INCLUDES ${NM_PROJECT_INCLUDES} ${SJASMPLUS_DIR})
set(NM_PROJECT_LIBS ${NM_PROJECT_LIBS} esjasmplus)

##############################################################################
# jart/json.cpp
set(JSONCPP_DIR ${CMAKE_SOURCE_DIR}/vendor/json.cpp)
add_subdirectory(${JSONCPP_DIR} EXCLUDE_FROM_ALL)
if (MSVC)
    target_compile_options(json PUBLIC "$<$<CONFIG:Release>:/MT>")
    target_compile_options(double-conversion PUBLIC "$<$<CONFIG:Release>:/MT>")
else ()
    target_compile_options(double-conversion PRIVATE -Wno-unknown-warning-option)
endif ()
set(NM_PROJECT_INCLUDES ${NM_PROJECT_INCLUDES} ${JSONCPP_DIR})
set(NM_PROJECT_LIBS ${NM_PROJECT_LIBS} json)

##############################################################################
# rang
set(RANG_DIR ${CMAKE_SOURCE_DIR}/vendor/rang)
set(NM_PROJECT_INCLUDES ${NM_PROJECT_INCLUDES} ${RANG_DIR}/include)

##############################################################################
# cxxopts
set(CXXOPTS_DIR ${CMAKE_SOURCE_DIR}/vendor/cxxopts)
set(NM_PROJECT_INCLUDES ${NM_PROJECT_INCLUDES} ${CXXOPTS_DIR}/include)

##############################################################################
# Boost Regex
set(BOOST_REGEX_DIR ${CMAKE_SOURCE_DIR}/vendor/regex)
set(BOOST_REGEX_STANDALONE ON CACHE BOOL "" FORCE)
set(BOOST_NO_EXCEPTIONS ON CACHE BOOL "" FORCE)
add_subdirectory(${BOOST_REGEX_DIR} EXCLUDE_FROM_ALL)
if(MSVC)
    target_compile_options(boost_regex INTERFACE "$<$<CONFIG:Release>:/MT>")
endif()

##############################################################################
# ZLIB
set(ZLIB_USE_STATIC_LIBS ON)
set(ZLIB_DIR ${CMAKE_SOURCE_DIR}/vendor/zlib)
add_subdirectory(${ZLIB_DIR} EXCLUDE_FROM_ALL)

##############################################################################
# IXWebSocket
set(USE_ZLIB OFF CACHE BOOL "" FORCE)
set(OPENSSL_USE_STATIC_LIBS TRUE)
set(OPENSSL_MSVC_STATIC_RT TRUE)
set(IXWEBSOCKET_INSTALL FALSE)
set(IXWS_DIR ${CMAKE_SOURCE_DIR}/vendor/IXWebSocket)
add_subdirectory(${IXWS_DIR} EXCLUDE_FROM_ALL)
target_compile_definitions(ixwebsocket PUBLIC IXWEBSOCKET_USE_ZLIB)
if(APPLE)
    target_compile_options(ixwebsocket PUBLIC -Wno-deprecated-declarations)
endif()
if(MSVC)
    target_compile_options(ixwebsocket INTERFACE "$<$<CONFIG:Release>:/MT>")
endif()
set(NM_PROJECT_LIBS ${NM_PROJECT_LIBS} ixwebsocket)
target_link_libraries(ixwebsocket PRIVATE zlibstatic)

##############################################################################
# ImGuiColorTextEdit
set(TEXT_EDITOR_SRCS
    ${CMAKE_SOURCE_DIR}/vendor/ImGuiColorTextEdit/TextEditor.cpp
    ${CMAKE_SOURCE_DIR}/vendor/ImGuiColorTextEdit/LanguageDefinitions.cpp)
add_library(texteditor STATIC ${TEXT_EDITOR_SRCS})
target_include_directories(texteditor PUBLIC
    ${IMGUI_DIR}
    ${BOOST_REGEX_DIR}/include
    ${CMAKE_SOURCE_DIR}/vendor/ImGuiColorTextEdit)
if(MSVC)
    target_compile_options(texteditor PUBLIC "$<$<CONFIG:Release>:/MT>")
endif()
# target_compile_definitions(texteditor PUBLIC IMGUI_EDITOR_NO_BOOST)
target_link_libraries(texteditor boost_regex)
set(NM_PROJECT_LIBS ${NM_PROJECT_LIBS} texteditor)

##############################################################################
# NYUKOMATIC
##############################################################################
configure_file(
    ${CMAKE_SOURCE_DIR}/cmake/version.h.in
    ${CMAKE_CURRENT_BINARY_DIR}/version.h)

set(NM_PROJECT_INCLUDES ${NM_PROJECT_INCLUDES} ${CMAKE_CURRENT_BINARY_DIR})

file(GLOB NM_PROJECT_SRCS
    ${CMAKE_SOURCE_DIR}/src/*.cpp
    ${CMAKE_SOURCE_DIR}/src/*.h)

source_group(${NM_EXE_NAME} FILES ${NM_PROJECT_SRCS})

if(APPLE)
    set(NM_RESOURCES_DATA
        ${CMAKE_SOURCE_DIR}/assets/macosx/icon.icns)
    set(NM_CONTENTS_DATA
        ${CMAKE_SOURCE_DIR}/assets/macosx/PkgInfo)
    set_source_files_properties(${NM_RESOURCES_DATA}
        PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
    set_source_files_properties(${NM_CONTENTS_DATA}
        PROPERTIES MACOSX_PACKAGE_LOCATION "")
    source_group("Data" FILES ${NM_RESOURCES_DATA} ${NM_CONTENTS_DATA})
elseif(UNIX)
elseif(WIN32)
    configure_file(
        ${CMAKE_SOURCE_DIR}/assets/windows/resources.rc.in
        ${CMAKE_CURRENT_BINARY_DIR}/resources.rc
        ESCAPE_QUOTES)
    set(NM_RESOURCES_DATA
        ${CMAKE_SOURCE_DIR}/assets/windows/icon.ico
        ${CMAKE_CURRENT_BINARY_DIR}/resources.rc)
    source_group("Data" FILES ${NM_RESOURCES_DATA})
    set(NM_PROJECT_INCLUDES ${CMAKE_SOURCE_DIR}/assets/windows ${NM_PROJECT_INCLUDES})
endif()

set(NM_PROJECT_SRCS ${NM_PROJECT_SRCS} ${NM_RESOURCES_DATA} ${NM_CONTENTS_DATA})
set(NM_PROJECT_INCLUDES ${CMAKE_SOURCE_DIR}/src ${NM_PROJECT_INCLUDES})

##############################################################################
# create the executable
add_executable(${NM_EXE_NAME} ${NM_PROJECT_SRCS})
target_include_directories(${NM_EXE_NAME} PUBLIC ${NM_PROJECT_INCLUDES})
target_link_libraries(${NM_EXE_NAME} ${NM_PROJECT_LIBS} ${PLATFORM_LIBS})

if(WIN32)
    ############################################################################
    # Windows
    if(MSVC)
        set_target_properties(${NM_EXE_NAME} PROPERTIES LINK_FLAGS "/SUBSYSTEM:WINDOWS /ENTRY:\"mainCRTStartup\"")
        target_compile_options(${NM_EXE_NAME} PUBLIC "$<$<CONFIG:Release>:/MT>")
        target_compile_definitions(${NM_EXE_NAME} PUBLIC NOMINMAX)
    endif()
    install(TARGETS ${NM_EXE_NAME} RUNTIME DESTINATION ".")
elseif(APPLE)
    ############################################################################
    # MacOS
    set(MACOSX_BUNDLE_BUNDLE_NAME ${NM_EXE_NAME})
    set(MACOSX_BUNDLE_GUI_IDENTIFIER "${NM_EXE_NAME}")
    set(MACOSX_BUNDLE_ICON_FILE icon.icns)
    set(MACOSX_BUNDLE_SHORT_VERSION_STRING "${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH}")
    set(MACOSX_BUNDLE_BUNDLE_VERSION "${GIT_COMMIT_SHORT_HASH}")
    set(MACOSX_BUNDLE_COPYRIGHT "${PROJECT_COPYRIGHT}")
    set_target_properties(${NM_EXE_NAME} PROPERTIES
        MACOSX_BUNDLE ON
        MACOSX_BUNDLE_BUNDLE_NAME "Nyukomatic"
        MACOSX_BUNDLE_INFO_PLIST ${CMAKE_SOURCE_DIR}/assets/macosx/MacOSXBundleInfo.plist.in)
    install(TARGETS ${NM_EXE_NAME}
        BUNDLE DESTINATION ".")
else()
    ##############################################################################
    # Linux / BSD / Unix
    configure_file(
        ${CMAKE_SOURCE_DIR}/assets/unix/desktop.in
        ${CMAKE_CURRENT_BINARY_DIR}/${NM_EXE_NAME}.desktop
        ESCAPE_QUOTES)
    install(TARGETS ${NM_EXE_NAME} RUNTIME DESTINATION "bin"
        PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE WORLD_READ WORLD_EXECUTE GROUP_READ GROUP_EXECUTE)
    install(FILES "${CMAKE_CURRENT_BINARY_DIR}/${NM_EXE_NAME}.desktop"
        DESTINATION "share/applications")
    install(FILES "${CMAKE_SOURCE_DIR}/assets/unix/icon.png"
        RENAME "${NM_EXE_NAME}.png"
        DESTINATION "share/icons/hicolor/256x256/apps")
endif()

##############################################################################
# setup CPack
set(CMAKE_INSTALL_LOCAL_ONLY ON CACHE BOOL "" FORCE)
set(CPACK_STRIP_FILES TRUE)
set(CPACK_PACKAGE_NAME ${NM_EXE_NAME})
set(CPACK_PACKAGE_VERSION ${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH})
set(CPACK_PACKAGE_DESCRIPTION "ZX Spectrum live coding tool")
set(CPACK_PACKAGE_VENDOR "Stardust")
set(CPACK_PACKAGE_CONTACT "alexanderk23@gmail.com")
set(CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION}-${CMAKE_SYSTEM_NAME}-${CMAKE_SYSTEM_PROCESSOR}")

if(${CMAKE_SYSTEM_NAME} MATCHES "Linux")
    set(CPACK_GENERATOR "ZIP")
elseif(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    set(CPACK_GENERATOR "DragNDrop")
    set(CPACK_DMG_FORMAT UDBZ)
    set(CPACK_DMG_VOLUME_NAME "${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION}")
    set(CPACK_DMG_BACKGROUND_IMAGE ${CMAKE_SOURCE_DIR}/assets/macosx/background.png)
    set(CPACK_DMG_DS_STORE_SETUP_SCRIPT ${CMAKE_SOURCE_DIR}/assets/macosx/dmgsetup.scpt)
elseif(${CMAKE_SYSTEM_NAME} MATCHES "Windows")
    set(CPACK_GENERATOR "ZIP")
else()
    message(ERROR "Unsupported system : ${CMAKE_SYSTEM_NAME}")
endif()

include(CPack)
