# Copies the run-time data files (data/ in the source tree) next to the
# executable, in the layout the original D-Fend Reloaded installer created
# for the "program folder" operation mode. Run as a post build step:
#
#   cmake -DSRC=<source>/data -DDST=<folder of the executable> -DWINDOWS=ON|OFF -P DeployData.cmake
#
# Program files (Bin, Lang, Help, IconSets, NewUserData, DOSBox) are refreshed on
# every build; file(COPY) only copies files that changed.
#
# The user data the installer created (templates, AutoSetup profiles, icon
# library, virtual hard disk with FreeDOS/Doszip, ...) is only copied once per
# build folder, so changes made in the program are not overwritten.

if(NOT SRC OR NOT DST)
    message(FATAL_ERROR "DeployData.cmake: SRC and DST have to be set")
endif()

# --- program files -----------------------------------------------------------
file(COPY "${SRC}/Lang" "${SRC}/Help" "${SRC}/IconSets" "${SRC}/NewUserData" DESTINATION "${DST}")
if(WINDOWS)
    file(COPY "${SRC}/Bin" DESTINATION "${DST}")
else()
    # The helper programs in Bin (mkdosfs, oggenc2, 7za, ...) are Windows
    # programs; Linux/macOS use the installed system programs instead.
    file(COPY "${SRC}/Bin" DESTINATION "${DST}"
         PATTERN "*.exe" EXCLUDE PATTERN "*.dll" EXCLUDE)
endif()

if(WINDOWS)
    # bundled DOSBox (Windows build) + DOSBox language files + FreeDOS
    # keyboard/code page files, as the installer did
    file(COPY "${SRC}/DOSBox" DESTINATION "${DST}")
    file(GLOB _Files "${SRC}/DOSBoxLang/*")
    file(COPY ${_Files} DESTINATION "${DST}/DOSBox")
    file(GLOB _Files "${SRC}/NewUserData/FREEDOS/CPI/*")
    if(_Files)
        file(COPY ${_Files} DESTINATION "${DST}/DOSBox")
    endif()
endif()

# --- user data (only once) ----------------------------------------------------
set(_Marker "${DST}/Settings/.DataDeployed")
if(NOT EXISTS "${_Marker}")
    set(_U "${SRC}/NewUserData")
    file(COPY "${_U}/Templates" "${_U}/AutoSetup" "${_U}/IconLibrary" DESTINATION "${DST}")
    file(COPY "${_U}/Capture/DOSBox DOS" DESTINATION "${DST}/Capture")
    file(COPY "${_U}/FREEDOS" "${_U}/DOSZIP" DESTINATION "${DST}/VirtualHD")
    foreach(_F "${_U}/Icons.ini" "${_U}/DataReader.xml" "${SRC}/Bin/Cheats.xml")
        get_filename_component(_N "${_F}" NAME)
        if(NOT EXISTS "${DST}/Settings/${_N}")
            file(COPY "${_F}" DESTINATION "${DST}/Settings")
        endif()
    endforeach()
    file(WRITE "${_Marker}" "Data files from NewUserData have been copied to this folder.\nDelete this file to copy them again on the next build.\n")
endif()
