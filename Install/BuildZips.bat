@echo off
setlocal
rem Builds the zip packages (full and mini) and the PortableApps.com package
rem from a portable installation in "Desktop\D-Fend Reborn" (see
rem README_HOW_TO_BUILD.txt, step 5).

cd /d "%~dp0"
set "SRC=%USERPROFILE%\Desktop\D-Fend Reborn"
set "MINI=%USERPROFILE%\Desktop\D-Fend Reborn (mini)"
set "PORTABLE=..\tools\PortableApps package"

if exist "%SRC%\DFend.exe" goto SourceOK
echo "%SRC%" does not exist: install a portable installation there first
echo (D-Fend-Reborn-...-Setup.exe, see README_HOW_TO_BUILD.txt).
goto Failed
:SourceOK

rem 7-Zip
set "SEVENZIP="
if exist "%ProgramFiles%\7-Zip\7z.exe" set "SEVENZIP=%ProgramFiles%\7-Zip\7z.exe"
if not defined SEVENZIP if exist "%ProgramFiles(x86)%\7-Zip\7z.exe" set "SEVENZIP=%ProgramFiles(x86)%\7-Zip\7z.exe"
if not defined SEVENZIP for /f "delims=" %%A in ('where 7z 2^>nul') do if not defined SEVENZIP set "SEVENZIP=%%A"
if defined SEVENZIP goto SevenZipOK
echo 7-Zip (7z.exe) not found.
goto Failed
:SevenZipOK

rem NSIS (makensis.exe)
set "MAKENSIS="
if exist "%ProgramFiles(x86)%\NSIS\makensis.exe" set "MAKENSIS=%ProgramFiles(x86)%\NSIS\makensis.exe"
if not defined MAKENSIS if exist "%ProgramFiles%\NSIS\makensis.exe" set "MAKENSIS=%ProgramFiles%\NSIS\makensis.exe"
if not defined MAKENSIS for /f "delims=" %%A in ('where makensis 2^>nul') do if not defined MAKENSIS set "MAKENSIS=%%A"
if defined MAKENSIS goto NSISOK
echo NSIS (makensis.exe) not found.
goto Failed
:NSISOK

rem Create main zip archive
"%SEVENZIP%" a -r D-Fend-Reborn-.zip "%SRC%\"
if errorlevel 1 goto Failed

rem Create mini zip archive (without DOSBox, DOSZip and FreeDOS)
if exist "%MINI%" rd /s /q "%MINI%"
xcopy "%SRC%" "%MINI%\" /E /I /Q
rd /s /q "%MINI%\DOSBox"
rd /s /q "%MINI%\VirtualHD\DOSZip"
rd /s /q "%MINI%\VirtualHD\FREEDOS"
"%SEVENZIP%" a -r D-Fend-Reborn--Mini.zip "%MINI%\"
if errorlevel 1 goto Failed
rd /s /q "%MINI%"

rem Create portable apps package
xcopy "%SRC%" "%PORTABLE%\D-Fend Reborn Portable\App\D-Fend Reborn\" /E /I /Q
"%MAKENSIS%" "%PORTABLE%\D-Fend Reborn Portable\Other\Launcher Source\PortableAppsLauncher.nsi"
if errorlevel 1 goto Failed
"%MAKENSIS%" "%PORTABLE%\D-Fend Reborn Portable\Other\Installer Source\PortableApps.comInstaller.nsi"
if errorlevel 1 goto Failed

copy "%PORTABLE%\*.exe" .
del "%PORTABLE%\*.exe"
del "%PORTABLE%\D-Fend Reborn Portable\D-Fend Reborn Portable.exe"
rd /s /q "%PORTABLE%\D-Fend Reborn Portable\App\D-Fend Reborn"
md "%PORTABLE%\D-Fend Reborn Portable\App\D-Fend Reborn"

rem (Source archives: GitHub offers them on each release page)

rem Clean up
rd /s /q "%SRC%"

echo.
echo Done: the packages are in %~dp0
pause
exit /b 0

:Failed
echo.
echo Building the packages failed.
pause
exit /b 1
