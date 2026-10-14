OutFile "..\..\D-Fend Reborn Portable.exe"
!if /FileExists "..\..\..\..\..\Install\Tools\upx.exe"
  !packhdr "$%TEMP%\exehead.tmp" '..\..\..\..\..\Install\Tools\upx.exe "$%TEMP%\exehead.tmp"'
!endif

Name "D-Fend Reborn PortableApps Launcher"
Caption "D-Fend Reborn PortableApps Launcher"
RequestExecutionLevel user
SetCompressor /solid lzma
Icon "..\..\App\AppInfo\appicon.ico"

Function .onInit
  SetSilent silent
FunctionEnd

Section "-Work"
  SectionIn RO
  
  Exec ".\App\D-Fend Reborn\DFend.exe"
SectionEnd