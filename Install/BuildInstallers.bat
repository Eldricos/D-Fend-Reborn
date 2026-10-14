@echo off
setlocal
rem Builds the D-Fend Reborn installers (see README_HOW_TO_BUILD.txt).
rem
rem Usage: BuildInstallers.bat ["CMake build folder of a release build"]
rem
rem Without a parameter the folder above this Install folder is used if it is
rem a CMake build folder (Install folder copied into the build folder), so the
rem script can simply be double-clicked there.

rem (all paths below are relative to this Install folder)
cd /d "%~dp0"

set "PAUSE_AT_END="
set "BUILD=%~1"
if not "%BUILD%"=="" goto HaveBuild
set "PAUSE_AT_END=1"
set "BUILD=%~dp0.."
:HaveBuild
for %%I in ("%BUILD%") do set "BUILD=%%~fI"
if exist "%BUILD%\CMakeCache.txt" goto BuildOK
echo "%BUILD%" is no CMake build folder (no CMakeCache.txt).
echo.
echo Usage: BuildInstallers.bat "CMake build folder of a release build"
echo (or copy this Install folder into the build folder and run it there)
goto Failed
:BuildOK

rem CMake: the one that configured the build folder (Qt Creator uses its own
rem CMake, which is usually not in PATH), otherwise CMake from PATH
set "CMAKE="
for /f "tokens=1,* delims==" %%A in ('findstr /b /c:"CMAKE_COMMAND:INTERNAL=" "%BUILD%\CMakeCache.txt"') do set "CMAKE=%%B"
if defined CMAKE set "CMAKE=%CMAKE:/=\%"
if defined CMAKE if not exist "%CMAKE%" set "CMAKE="
if not defined CMAKE for /f "delims=" %%A in ('where cmake 2^>nul') do if not defined CMAKE set "CMAKE=%%A"
if defined CMAKE goto CMakeOK
echo CMake not found.
goto Failed
:CMakeOK

rem NSIS (makensis.exe)
set "MAKENSIS="
if exist "%ProgramFiles(x86)%\NSIS\makensis.exe" set "MAKENSIS=%ProgramFiles(x86)%\NSIS\makensis.exe"
if not defined MAKENSIS if exist "%ProgramFiles%\NSIS\makensis.exe" set "MAKENSIS=%ProgramFiles%\NSIS\makensis.exe"
if not defined MAKENSIS for /f "delims=" %%A in ('where makensis 2^>nul') do if not defined MAKENSIS set "MAKENSIS=%%A"
if defined MAKENSIS goto NSISOK
echo NSIS (makensis.exe) not found. Install NSIS 3 from https://nsis.sourceforge.io/
goto Failed
:NSISOK

echo Build folder: %BUILD%
echo CMake:        %CMAKE%
echo NSIS:         %MAKENSIS%
echo.

rem Program folder for the installers: DFend.exe, Qt libraries and the program files
if exist Stage rd /s /q Stage
"%CMAKE%" --install "%BUILD%" --prefix Stage --config Release
if errorlevel 1 goto Failed
if exist Stage\DFend.exe goto StageOK
echo Stage\DFend.exe is missing: build the release configuration first.
goto Failed
:StageOK

"%MAKENSIS%" D-Fend-Reborn-UpdateSetup.nsi
if errorlevel 1 goto Failed
"%MAKENSIS%" D-Fend-Reborn-Setup.nsi
if errorlevel 1 goto Failed

echo.
echo Done: the installers are in %~dp0
if defined PAUSE_AT_END pause
exit /b 0

:Failed
echo.
echo Building the installers failed.
if defined PAUSE_AT_END pause
exit /b 1
