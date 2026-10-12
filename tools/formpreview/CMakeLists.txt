# Developer tool: renders every converted .ui form to a PNG for visual checks.
cmake_minimum_required(VERSION 3.19)
project(FormPreview LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
find_package(Qt6 REQUIRED COMPONENTS Widgets UiTools)
set(SRC ${CMAKE_CURRENT_SOURCE_DIR}/../../src)
add_executable(formpreview main.cpp
    ${SRC}/compat/SysUtils.cpp ${SRC}/compat/Classes.cpp ${SRC}/compat/Dialogs.cpp
    ${SRC}/compat/VclLayout.cpp ${SRC}/compat/VclWidgets.cpp ${SRC}/compat/VclForm.cpp
    ${SRC}/compat/VclForm.h ${SRC}/compat/VclWidgets.h ${SRC}/compat/ComCtrls.cpp ${SRC}/compat/ComCtrls.h ${SRC}/compat/Graphics.cpp ${SRC}/compat/IniFiles.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../resources/dfend.qrc)
target_include_directories(formpreview PRIVATE ${SRC}/compat)
target_link_libraries(formpreview PRIVATE Qt6::Widgets Qt6::UiTools)
