@echo off

rem Create main zip archive
"C:\Program Files\7-Zip\7z.exe" a -r D-Fend-Reborn-.zip "%USERPROFILE%\Desktop\D-Fend Reborn\"

rem Create mini zip archive
md "%USERPROFILE%\Desktop\D-Fend Reborn (mini)"
xcopy "%USERPROFILE%\Desktop\D-Fend Reborn" "%USERPROFILE%\Desktop\D-Fend Reborn (mini)" /E

del "%USERPROFILE%\Desktop\D-Fend Reborn (mini)\DOSBox\*.*" /S /Q
rd "%USERPROFILE%\Desktop\D-Fend Reborn (mini)\DOSBox" /S /Q

del "%USERPROFILE%\Desktop\D-Fend Reborn (mini)\VirtualHD\DOSZip\*.*" /S /Q
rd "%USERPROFILE%\Desktop\D-Fend Reborn (mini)\VirtualHD\DOSZip" /S /Q

del "%USERPROFILE%\Desktop\D-Fend Reborn (mini)\VirtualHD\FREEDOS\*.*" /S /Q
rd "%USERPROFILE%\Desktop\D-Fend Reborn (mini)\VirtualHD\FREEDOS" /S /Q

"C:\Program Files\7-Zip\7z.exe" a -r D-Fend-Reborn--Mini.zip "C:\Users\Alexander Herzog\Desktop\D-Fend Reborn (mini)\"

del "%USERPROFILE%\Desktop\D-Fend Reborn (mini)\*.*" /S /Q
rd "%USERPROFILE%\Desktop\D-Fend Reborn (mini)" /S /Q 

rem Create portable apps package
xcopy "%USERPROFILE%\Desktop\D-Fend Reborn" "..\Tools\PortableApps package\D-Fend Reborn Portable\App\D-Fend Reborn\" /E

"C:\Program Files (x86)\NSIS\makensis.exe" "..\Tools\PortableApps package\D-Fend Reborn Portable\Other\Launcher Source\PortableAppsLauncher.nsi"
"C:\Program Files (x86)\NSIS\makensis.exe" "..\Tools\PortableApps package\D-Fend Reborn Portable\Other\Installer Source\PortableApps.comInstaller.nsi"

copy "..\Tools\PortableApps package\*.exe" .
del "..\Tools\PortableApps package\*.exe"
del "..\Tools\PortableApps package\D-Fend Reborn Portable\D-Fend Reborn Portable.exe"

del "..\Tools\PortableApps package\D-Fend Reborn Portable\App\D-Fend Reborn\*.*" /S /Q
rd "..\Tools\PortableApps package\D-Fend Reborn Portable\App\D-Fend Reborn" /S /Q
md "..\Tools\PortableApps package\D-Fend Reborn Portable\App\D-Fend Reborn"

rem (Source archives: GitHub offers them on each release page)

rem Clean up
del "%USERPROFILE%\Desktop\D-Fend Reborn\*.*" /S /Q
rd "%USERPROFILE%\Desktop\D-Fend Reborn" /S /Q 