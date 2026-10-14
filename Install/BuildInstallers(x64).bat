@echo off
rem Builds the D-Fend Reborn installers (see README_HOW_TO_BUILD.txt).
rem Usage: BuildInstallers(x64).bat "<CMake build folder of a release build>"

if "%~1"=="" (
  echo Usage: %~nx0 "<CMake build folder of a release build>"
  exit /b 1
)

rem Program folder for the installers: DFend.exe, Qt libraries and the program files
if exist Stage rd /s /q Stage
cmake --install "%~1" --prefix Stage || exit /b 1

"%ProgramFiles(x86)%\NSIS\makensis.exe" D-Fend-Reborn-UpdateSetup.nsi || exit /b 1
"%ProgramFiles(x86)%\NSIS\makensis.exe" D-Fend-Reborn-Setup.nsi || exit /b 1
