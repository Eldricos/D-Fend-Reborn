cmake_minimum_required(VERSION 3.19)

project(DFendReborn VERSION 0.5.1 LANGUAGES CXX)

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

# All program code is compiled once into an object library, which is used by
# the program (DFend) and the package tool (DFRPackageTool).
qt_add_library(DFendCore OBJECT
    ${COMPAT_SOURCES}
    ${CORE_SOURCES}
    ${FORM_SOURCES}
    resources/dfend.qrc
)

# (the generated ui_*.h files are included by the form headers, so they are
# part of the interface of the library)
set(DFENDCORE_AUTOGEN ${CMAKE_CURRENT_BINARY_DIR}/DFendCore_autogen)
set_target_properties(DFendCore PROPERTIES AUTOGEN_BUILD_DIR ${DFENDCORE_AUTOGEN})
target_include_directories(DFendCore PUBLIC src src/compat src/core src/forms
    ${DFENDCORE_AUTOGEN}/include ${DFENDCORE_AUTOGEN}/include_$<CONFIG>)

target_link_libraries(DFendCore PUBLIC
    Qt6::Widgets Qt6::Network Qt6::Xml Qt6::Multimedia Qt6::MultimediaWidgets)

target_precompile_headers(DFendCore PRIVATE
    <QtWidgets> <QtXml> <QtNetwork> <functional>)

qt_add_executable(DFendReborn WIN32 MACOSX_BUNDLE
    src/main.cpp
    ${APP_ICON_RC}
)
target_link_libraries(DFendReborn PRIVATE DFendCore)

# Command line tool for building the game packages of the package manager
# (tools/PackageTool/README.md)
qt_add_executable(DFRPackageTool tools/PackageTool/main.cpp)
target_link_libraries(DFRPackageTool PRIVATE DFendCore)

# Linux/BSD desktops: D-Bus is used to inhibit the screensaver while games
# run and to set the KDE Plasma wallpaper (optional - without Qt D-Bus these
# features are left out)
if(UNIX AND NOT APPLE)
    find_package(Qt6 QUIET COMPONENTS DBus)
    if(Qt6DBus_FOUND)
        target_link_libraries(DFendCore PUBLIC Qt6::DBus)
        target_compile_definitions(DFendCore PUBLIC DFR_HAVE_DBUS)
    endif()
endif()

# Program version (shown in the program, compared by the update check with
# the latest GitHub release): the version of project() above.
# Compatibility version: the D-Fend Reloaded version whose data files
# (settings, profiles, package lists, language files) D-Fend Reborn uses;
# package lists and language files are checked against it.
set(DFR_COMPAT_VERSION 1.4.4)
target_compile_definitions(DFendCore PUBLIC DFR_VERSION="${PROJECT_VERSION}" DFR_COMPAT_VERSION="${DFR_COMPAT_VERSION}")

if(WIN32)
    target_link_libraries(DFendCore PUBLIC shell32 ole32 oleaut32 user32 advapi32 winmm uuid)
    target_compile_definitions(DFendCore PUBLIC UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
endif()

if(MSVC)
    target_compile_options(DFendCore PUBLIC /utf-8 /bigobj)
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

# Installation ("cmake --install <build folder> --prefix <folder>"): the
# program folder without user data, as the installer packages it
# (Install/README_HOW_TO_BUILD.txt). The user data is created from
# NewUserData by the installer / the program.
install(TARGETS DFendReborn RUNTIME DESTINATION . BUNDLE DESTINATION .)
install(DIRECTORY data/Lang data/Help data/IconSets data/NewUserData DESTINATION .)
if(WIN32)
    install(DIRECTORY data/Bin data/DOSBox data/DOSBoxLang DESTINATION .)
    # Qt libraries and plugins next to DFend.exe
    get_target_property(_QMAKE Qt6::qmake IMPORTED_LOCATION)
    get_filename_component(_QT_BIN "${_QMAKE}" DIRECTORY)
    find_program(WINDEPLOYQT windeployqt HINTS "${_QT_BIN}")
    if(WINDEPLOYQT)
        install(CODE "execute_process(COMMAND \"${WINDEPLOYQT}\" --no-translations --dir \"\${CMAKE_INSTALL_PREFIX}\" \"\${CMAKE_INSTALL_PREFIX}/DFend.exe\" COMMAND_ERROR_IS_FATAL ANY)")
    else()
        message(WARNING "windeployqt not found: \"cmake --install\" will not copy the Qt libraries")
    endif()
else()
    install(DIRECTORY data/Bin DESTINATION . PATTERN "*.exe" EXCLUDE PATTERN "*.dll" EXCLUDE)
endif()
