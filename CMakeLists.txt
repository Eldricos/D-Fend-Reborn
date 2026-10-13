cmake_minimum_required(VERSION 3.19)

project(DFendReborn VERSION 1.4.4 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC_SEARCH_PATHS ${CMAKE_SOURCE_DIR}/src/forms)

find_package(Qt6 REQUIRED COMPONENTS Widgets Network Xml Multimedia MultimediaWidgets)

file(GLOB COMPAT_SOURCES CONFIGURE_DEPENDS src/compat/*.cpp src/compat/*.h)
file(GLOB CORE_SOURCES CONFIGURE_DEPENDS src/core/*.cpp src/core/*.h)
file(GLOB FORM_SOURCES CONFIGURE_DEPENDS src/forms/*.cpp src/forms/*.h src/forms/*.ui)

if(WIN32)
    set(APP_ICON_RC ${CMAKE_SOURCE_DIR}/resources/dfend.rc)
endif()

qt_add_executable(DFendReborn WIN32 MACOSX_BUNDLE
    src/main.cpp
    ${COMPAT_SOURCES}
    ${CORE_SOURCES}
    ${FORM_SOURCES}
    resources/dfend.qrc
    ${APP_ICON_RC}
)

target_include_directories(DFendReborn PRIVATE src src/compat src/core src/forms)

target_link_libraries(DFendReborn PRIVATE
    Qt6::Widgets Qt6::Network Qt6::Xml Qt6::Multimedia Qt6::MultimediaWidgets)

target_precompile_headers(DFendReborn PRIVATE
    <QtWidgets> <QtXml> <QtNetwork> <functional>)

# Linux/BSD desktops: D-Bus is used to inhibit the screensaver while games
# run and to set the KDE Plasma wallpaper (optional - without Qt D-Bus these
# features are left out)
if(UNIX AND NOT APPLE)
    find_package(Qt6 QUIET COMPONENTS DBus)
    if(Qt6DBus_FOUND)
        target_link_libraries(DFendReborn PRIVATE Qt6::DBus)
        target_compile_definitions(DFendReborn PRIVATE DFR_HAVE_DBUS)
    endif()
endif()

if(WIN32)
    target_link_libraries(DFendReborn PRIVATE shell32 ole32 oleaut32 user32 advapi32 winmm uuid)
    target_compile_definitions(DFendReborn PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
endif()

if(MSVC)
    target_compile_options(DFendReborn PRIVATE /utf-8 /bigobj)
endif()

set_target_properties(DFendReborn PROPERTIES OUTPUT_NAME "DFend")

# Copy the run-time data files (data/) next to the executable, so a build
# can be run directly (program folder operation mode, like the original
# installer set it up).
add_custom_target(DeployData ALL
    COMMAND ${CMAKE_COMMAND} -DSRC=${CMAKE_SOURCE_DIR}/data -DDST=$<TARGET_FILE_DIR:DFendReborn>
            -DWINDOWS=$<BOOL:${WIN32}> -P ${CMAKE_SOURCE_DIR}/cmake/DeployData.cmake
    COMMENT "Copying data files"
    VERBATIM)
add_dependencies(DeployData DFendReborn)
