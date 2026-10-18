# DOSBox Staging, the DOSBox version included in the downloads of D-Fend
# Reborn (folder "DOSBox" next to the program): Windows and Linux (x86_64).
#
# The official release of DOSBox Staging for the platform is downloaded once
# per build folder (when CMake configures the build) and unpacked to
# <build folder>/DOSBoxStaging. DeployData.cmake copies it next to the
# program, "cmake --install" installs it and the packages include it.
#
# To include another DOSBox Staging version, run
# "python3 tools/update-dosbox-staging.py apply" (writes the version and the
# SHA-256 checksums of the Windows zip and the Linux tar.xz below). Without
# internet access configure the build with -DDFR_BUNDLE_DOSBOX=OFF (no DOSBox
# in the program folder: the DOSBox of the system is used) or put the archive
# into the build folder.
#
# DOSBox Staging is licensed under the GNU GPL (version 2 or later): its
# license files are kept and SOURCE.txt in the DOSBox folder names the
# source code of the included version (the release workflow also attaches
# the source archive to each D-Fend Reborn release).

set(DFR_DOSBOX_STAGING_VERSION 0.83.0)
set(DFR_DOSBOX_STAGING_SHA256 725b915e325a6d410ce30a10989fd492fdad07f6611fff40ff77f160478810f4)
set(DFR_DOSBOX_STAGING_SHA256_LINUX d3a94f7f1c3e68a47ec88d61145506c7904452adb0c9c5928cb8cfe2331d6c5c)

if(WIN32)
    set(_Name "dosbox-staging-windows-x64-v${DFR_DOSBOX_STAGING_VERSION}")
    set(_Ext "zip")
    set(_Sha ${DFR_DOSBOX_STAGING_SHA256})
    set(_Exe "dosbox.exe")
    set(_Platform "Windows")
    set(_NL "\r\n")
else()
    set(_Name "dosbox-staging-linux-x86_64-v${DFR_DOSBOX_STAGING_VERSION}")
    set(_Ext "tar.xz")
    set(_Sha ${DFR_DOSBOX_STAGING_SHA256_LINUX})
    set(_Exe "dosbox")
    set(_Platform "Linux (x86_64)")
    set(_NL "\n")
endif()
set(_Url "https://github.com/dosbox-staging/dosbox-staging/releases/download/v${DFR_DOSBOX_STAGING_VERSION}/${_Name}.${_Ext}")
set(_Zip "${CMAKE_BINARY_DIR}/${_Name}.${_Ext}")
set(DFR_DOSBOX_DIR "${CMAKE_BINARY_DIR}/DOSBoxStaging")
set(_Stamp "${CMAKE_BINARY_DIR}/DOSBoxStaging.version")

set(_Current "")
if(EXISTS "${_Stamp}")
    file(READ "${_Stamp}" _Current)
endif()

if(NOT _Current STREQUAL DFR_DOSBOX_STAGING_VERSION OR NOT EXISTS "${DFR_DOSBOX_DIR}/${_Exe}")
    if(NOT EXISTS "${_Zip}")
        message(STATUS "Downloading DOSBox Staging ${DFR_DOSBOX_STAGING_VERSION}")
        file(DOWNLOAD "${_Url}" "${_Zip}.part" SHOW_PROGRESS STATUS _Status
             EXPECTED_HASH SHA256=${_Sha})
        list(GET _Status 0 _Code)
        if(NOT _Code EQUAL 0)
            file(REMOVE "${_Zip}.part")
            message(FATAL_ERROR "Could not download DOSBox Staging (${_Url}): ${_Status}\n"
                    "Configure with -DDFR_BUNDLE_DOSBOX=OFF to build without the included DOSBox.")
        endif()
        file(RENAME "${_Zip}.part" "${_Zip}")
    else()
        file(SHA256 "${_Zip}" _Hash)
        if(NOT _Hash STREQUAL _Sha)
            message(FATAL_ERROR "${_Zip} has the wrong checksum (delete it to download it again)")
        endif()
    endif()

    set(_Tmp "${CMAKE_BINARY_DIR}/DOSBoxStaging.tmp")
    file(REMOVE_RECURSE "${_Tmp}" "${DFR_DOSBOX_DIR}")
    file(ARCHIVE_EXTRACT INPUT "${_Zip}" DESTINATION "${_Tmp}")
    # the archive contains one folder (the program files of DOSBox Staging)
    file(GLOB _Top LIST_DIRECTORIES true "${_Tmp}/*")
    list(LENGTH _Top _Count)
    if(NOT _Count EQUAL 1 OR NOT IS_DIRECTORY "${_Top}")
        message(FATAL_ERROR "Unexpected content of ${_Zip}")
    endif()
    file(RENAME "${_Top}" "${DFR_DOSBOX_DIR}")
    file(REMOVE_RECURSE "${_Tmp}")
    if(NOT EXISTS "${DFR_DOSBOX_DIR}/${_Exe}")
        message(FATAL_ERROR "${_Zip} contains no ${_Exe}")
    endif()
    # (the debugger build is not needed by D-Fend Reborn)
    file(REMOVE "${DFR_DOSBOX_DIR}/dosbox_with_debugger.exe")
    if(WIN32)
        set(_Licenses "LICENSE.txt and doc\\licenses")
    else()
        set(_Licenses "LICENSE and doc/licenses")
    endif()
    file(WRITE "${DFR_DOSBOX_DIR}/SOURCE.txt"
        "This folder contains DOSBox Staging ${DFR_DOSBOX_STAGING_VERSION} (https://www.dosbox-staging.org/),${_NL}"
        "the official ${_Platform} release, unchanged (without dosbox_with_debugger.exe).${_NL}"
        "${_NL}"
        "DOSBox Staging is free software, licensed under the GNU General Public License${_NL}"
        "version 2 or later (see ${_Licenses}).${_NL}"
        "${_NL}"
        "Source code of this version:${_NL}"
        "https://github.com/dosbox-staging/dosbox-staging/releases/tag/v${DFR_DOSBOX_STAGING_VERSION}${_NL}"
        "https://github.com/dosbox-staging/dosbox-staging/archive/refs/tags/v${DFR_DOSBOX_STAGING_VERSION}.tar.gz${_NL}"
        "The source archive is also attached to each D-Fend Reborn release:${_NL}"
        "https://github.com/Eldricos/D-Fend-Reborn/releases${_NL}")
    file(WRITE "${_Stamp}" "${DFR_DOSBOX_STAGING_VERSION}")
endif()
