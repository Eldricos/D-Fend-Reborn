# DOSBox Staging, the DOSBox version included in the Windows downloads of
# D-Fend Reborn (folder "DOSBox" next to DFend.exe).
#
# The official Windows release of DOSBox Staging is downloaded once per build
# folder (when CMake configures the build) and unpacked to
# <build folder>/DOSBoxStaging. DeployData.cmake copies it next to the
# program, "cmake --install" installs it and the installers package it.
#
# To include another DOSBox Staging version, change the version and the
# SHA-256 checksum of the zip file below (sha256sum / certutil -hashfile
# <file> SHA256). Without internet access configure the build with
# -DDFR_BUNDLE_DOSBOX=OFF (no DOSBox in the program folder) or put the zip file
# into the build folder.
#
# DOSBox Staging is licensed under the GNU GPL (version 2 or later): its
# license files are kept and SOURCE.txt in the DOSBox folder names the
# source code of the included version (the release workflow also attaches
# the source archive to each D-Fend Reborn release).

set(DFR_DOSBOX_STAGING_VERSION 0.83.0)
set(DFR_DOSBOX_STAGING_SHA256 725b915e325a6d410ce30a10989fd492fdad07f6611fff40ff77f160478810f4)

set(_Name "dosbox-staging-windows-x64-v${DFR_DOSBOX_STAGING_VERSION}")
set(_Url "https://github.com/dosbox-staging/dosbox-staging/releases/download/v${DFR_DOSBOX_STAGING_VERSION}/${_Name}.zip")
set(_Zip "${CMAKE_BINARY_DIR}/${_Name}.zip")
set(DFR_DOSBOX_DIR "${CMAKE_BINARY_DIR}/DOSBoxStaging")
set(_Stamp "${CMAKE_BINARY_DIR}/DOSBoxStaging.version")

set(_Current "")
if(EXISTS "${_Stamp}")
    file(READ "${_Stamp}" _Current)
endif()

if(NOT _Current STREQUAL DFR_DOSBOX_STAGING_VERSION OR NOT EXISTS "${DFR_DOSBOX_DIR}/dosbox.exe")
    if(NOT EXISTS "${_Zip}")
        message(STATUS "Downloading DOSBox Staging ${DFR_DOSBOX_STAGING_VERSION}")
        file(DOWNLOAD "${_Url}" "${_Zip}.part" SHOW_PROGRESS STATUS _Status
             EXPECTED_HASH SHA256=${DFR_DOSBOX_STAGING_SHA256})
        list(GET _Status 0 _Code)
        if(NOT _Code EQUAL 0)
            file(REMOVE "${_Zip}.part")
            message(FATAL_ERROR "Could not download DOSBox Staging (${_Url}): ${_Status}\n"
                    "Configure with -DDFR_BUNDLE_DOSBOX=OFF to build without the included DOSBox.")
        endif()
        file(RENAME "${_Zip}.part" "${_Zip}")
    else()
        file(SHA256 "${_Zip}" _Hash)
        if(NOT _Hash STREQUAL DFR_DOSBOX_STAGING_SHA256)
            message(FATAL_ERROR "${_Zip} has the wrong checksum (delete it to download it again)")
        endif()
    endif()

    set(_Tmp "${CMAKE_BINARY_DIR}/DOSBoxStaging.tmp")
    file(REMOVE_RECURSE "${_Tmp}" "${DFR_DOSBOX_DIR}")
    file(ARCHIVE_EXTRACT INPUT "${_Zip}" DESTINATION "${_Tmp}")
    # the zip file contains one folder "dosbox-staging-v<version>"
    file(GLOB _Top LIST_DIRECTORIES true "${_Tmp}/*")
    list(LENGTH _Top _Count)
    if(NOT _Count EQUAL 1 OR NOT IS_DIRECTORY "${_Top}")
        message(FATAL_ERROR "Unexpected content of ${_Zip}")
    endif()
    file(RENAME "${_Top}" "${DFR_DOSBOX_DIR}")
    file(REMOVE_RECURSE "${_Tmp}")
    # (the debugger build is not needed by D-Fend Reborn)
    file(REMOVE "${DFR_DOSBOX_DIR}/dosbox_with_debugger.exe")
    file(WRITE "${DFR_DOSBOX_DIR}/SOURCE.txt"
        "This folder contains DOSBox Staging ${DFR_DOSBOX_STAGING_VERSION} (https://www.dosbox-staging.org/),\r\n"
        "the official Windows release, unchanged (without dosbox_with_debugger.exe).\r\n"
        "\r\n"
        "DOSBox Staging is free software, licensed under the GNU General Public License\r\n"
        "version 2 or later (see LICENSE.txt and doc\\licenses).\r\n"
        "\r\n"
        "Source code of this version:\r\n"
        "https://github.com/dosbox-staging/dosbox-staging/releases/tag/v${DFR_DOSBOX_STAGING_VERSION}\r\n"
        "https://github.com/dosbox-staging/dosbox-staging/archive/refs/tags/v${DFR_DOSBOX_STAGING_VERSION}.tar.gz\r\n"
        "The source archive is also attached to each D-Fend Reborn release:\r\n"
        "https://github.com/Eldricos/D-Fend-Reborn/releases\r\n")
    file(WRITE "${_Stamp}" "${DFR_DOSBOX_STAGING_VERSION}")
endif()
