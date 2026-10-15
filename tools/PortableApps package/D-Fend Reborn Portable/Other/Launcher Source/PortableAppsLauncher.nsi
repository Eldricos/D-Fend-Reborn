OutFile "..\..\D-Fend Reborn Portable.exe"
!if /FileExists "..\..\..\..\..\Install\Tools\upx.exe"
  !packhdr "$%TEMP%\exehead.tmp" '..\..\..\..\..\Install\Tools\upx.exe "$%TEMP%\exehead.tmp"'
!endif

Name "D-Fend Reborn PortableApps Launcher"
Caption "D-Fend Reborn PortableApps Launcher"
RequestExecutionLevel user
SetCompressor /solid lzma
Icon "..\..\App\AppInfo\appicon.ico"

!include FileFunc.nsh

Function .onInit
  SetSilent silent
FunctionEnd

Section "-Work"
  SectionIn RO
  
  ; start D-Fend Reborn from the folder of the launcher (not from the current
  ; folder) and pass on the parameters
  ${GetParameters} $0
  SetOutPath "$EXEDIR\App\D-Fend Reborn"
  Exec '"$EXEDIR\App\D-Fend Reborn\DFend.exe" $0'
SectionEnd