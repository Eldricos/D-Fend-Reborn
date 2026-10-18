cmake_minimum_required(VERSION 3.19)

project(DFendReborn VERSION 0.5.7 LANGUAGES CXX)

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

# Automated tests (tests/CoreTests.cpp; run with ctest). Built if Qt Test
# is installed.
option(DFR_BUILD_TESTS "Build the automated tests" ON)
if(DFR_BUILD_TESTS)
    find_package(Qt6 QUIET COMPONENTS Test)
    if(Qt6Test_FOUND)
        enable_testing()
        qt_add_executable(DFRTests tests/CoreTests.cpp)
        target_link_libraries(DFRTests PRIVATE DFendCore Qt6::Test)
        target_compile_definitions(DFRTests PRIVATE DFR_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
        add_test(NAME DFRTests COMMAND DFRTests)
        # (no windows are shown; on systems without a display Qt needs the
        # "offscreen" platform)
        set_tests_properties(DFRTests PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    else()
        message(STATUS "Qt Test not found: the automated tests are not built")
    endif()
endif()

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

# Version of the DOSBox Staging manual pages the help menu links to (the
# included DOSBox Staging version, cmake/DOSBoxStaging.cmake; also used
# without the included DOSBox, e.g. on Linux).
file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/cmake/DOSBoxStaging.cmake" _StagingVersionLine REGEX "^set\\(DFR_DOSBOX_STAGING_VERSION ")
string(REGEX MATCH "([0-9]+)\\.([0-9]+)" _StagingDocsVersion "${_StagingVersionLine}")
target_compile_definitions(DFendCore PUBLIC DFR_STAGING_DOCS_VERSION="${_StagingDocsVersion}")

if(WIN32)
    target_link_libraries(DFendCore PUBLIC shell32 ole32 oleaut32 user32 advapi32 winmm uuid)
    target_compile_definitions(DFendCore PUBLIC UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
endif()

if(MSVC)
    target_compile_options(DFendCore PUBLIC /utf-8 /bigobj)
endif()

set_target_properties(DFendReborn PROPERTIES OUTPUT_NAME "DFend")

# Windows and Linux (x86_64): DOSBox Staging is included in the program
# folder (folder "DOSBox", downloaded when the build is configured, see
# cmake/DOSBoxStaging.cmake). Without it (-DDFR_BUNDLE_DOSBOX=OFF, other
# systems) the DOSBox of the system is used.
set(_BundleDefault OFF)
if(WIN32 OR (CMAKE_SYSTEM_NAME STREQUAL "Linux" AND CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|AMD64|amd64)$"))
    set(_BundleDefault ON)
endif()
option(DFR_BUNDLE_DOSBOX "Include DOSBox Staging in the program folder (Windows, Linux x86_64)" ${_BundleDefault})
set(DFR_DOSBOX_DIR "")
if(DFR_BUNDLE_DOSBOX)
    include(cmake/DOSBoxStaging.cmake)
endif()

# Copy the run-time data files (data/) next to the executable, so a build
# can be run directly (program folder operation mode, like the original
# installer set it up).
add_custom_target(DeployData ALL
    COMMAND ${CMAKE_COMMAND} -DSRC=${CMAKE_SOURCE_DIR}/data -DDST=$<TARGET_FILE_DIR:DFendReborn>
            -DWINDOWS=$<BOOL:${WIN32}> -DDOSBOX=${DFR_DOSBOX_DIR} -P ${CMAKE_SOURCE_DIR}/cmake/DeployData.cmake
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
    install(DIRECTORY data/Bin DESTINATION .)
    if(DFR_DOSBOX_DIR)
        install(DIRECTORY "${DFR_DOSBOX_DIR}/" DESTINATION DOSBox)
    endif()
    # Qt libraries next to DFend.exe (Windows loads them from the program
    # folder), all Qt plugins in the subfolder "Qt" (qt.conf). Not deployed:
    # translations (not used), opengl32sw.dll (software OpenGL, not used) and
    # d3dcompiler_47.dll (part of Windows 10/11).
    get_target_property(_QMAKE Qt6::qmake IMPORTED_LOCATION)
    get_filename_component(_QT_BIN "${_QMAKE}" DIRECTORY)
    find_program(WINDEPLOYQT windeployqt HINTS "${_QT_BIN}")
    if(WINDEPLOYQT)
        install(CODE "execute_process(COMMAND \"${WINDEPLOYQT}\" --no-translations --no-opengl-sw --no-system-d3d-compiler --plugindir \"\${CMAKE_INSTALL_PREFIX}/Qt\" --dir \"\${CMAKE_INSTALL_PREFIX}\" \"\${CMAKE_INSTALL_PREFIX}/DFend.exe\" COMMAND_ERROR_IS_FATAL ANY)")
        install(CODE "file(WRITE \"\${CMAKE_INSTALL_PREFIX}/qt.conf\" \"[Paths]\\nPlugins = Qt\\n\")")
    else()
        message(WARNING "windeployqt not found: \"cmake --install\" will not copy the Qt libraries")
    endif()
else()
    install(DIRECTORY data/Bin DESTINATION . PATTERN "*.exe" EXCLUDE PATTERN "*.dll" EXCLUDE)
    if(DFR_DOSBOX_DIR)
        # (USE_SOURCE_PERMISSIONS: the program files stay executable)
        install(DIRECTORY "${DFR_DOSBOX_DIR}/" DESTINATION DOSBox USE_SOURCE_PERMISSIONS)
    endif()
endif()
