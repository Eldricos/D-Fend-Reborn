; Stores the installer language that the D-Fend Reborn installers (setup,
; update and game package installers) use and that the first run wizard
; offers as program language. Started by D-Fend Reborn (Windows only).
Unicode true
OutFile "SetInstallerLanguage.exe"

Name "SetInstallerLanguage"
RequestExecutionLevel admin
SetCompressor /solid lzma
Icon "${NSISDIR}\Contrib\Graphics\Icons\arrow-install.ico"

Function .onInit
  SetSilent silent
FunctionEnd

Function GetParameters
 
   Push $R0
   Push $R1
   Push $R2
   Push $R3
   
   StrCpy $R2 1
   StrLen $R3 $CMDLINE
   
   ;Check for quote or space
   StrCpy $R0 $CMDLINE $R2
   StrCmp $R0 '"' 0 +3
     StrCpy $R1 '"'
     Goto loop
   StrCpy $R1 " "
   
   loop:
     IntOp $R2 $R2 + 1
     StrCpy $R0 $CMDLINE 1 $R2
     StrCmp $R0 $R1 get
     StrCmp $R2 $R3 get
     Goto loop
   
   get:
     IntOp $R2 $R2 + 1
     StrCpy $R0 $CMDLINE 1 $R2
     StrCmp $R0 " " get
     StrCpy $R0 $CMDLINE "" $R2
   
   Pop $R3
   Pop $R2
   Pop $R1
   Exch $R0
 
 FunctionEnd


Section "-Work"
  SectionIn RO
  
  Push $0
  Call GetParameters
  pop $0
  WriteRegStr HKLM "Software\D-Fend Reborn" "Installer Language" "$0"
SectionEnd
