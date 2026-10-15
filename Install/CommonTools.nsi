; D-Fend Reborn: Unicode installer (file names and texts of all languages)
Unicode true

; NSI SCRIPT TOOLS FOR D-FEND REBORN
; ============================================================

; Define VER_MAYOR, VER_MINOR1, VER_MINOR2 and INST_FILENAME before including this file !
; Define "Update" (without content) if update installer.



; Include used librarys
; ============================================================

!include "MUI.nsh"
!include "Sections.nsh"
!include "WinMessages.nsh"



; Define program name and version information
; ============================================================

!define PrgName "D-Fend Reborn ${VER_MAYOR}.${VER_MINOR1}.${VER_MINOR2}"
OutFile "D-Fend-Reborn-${VER_MAYOR}.${VER_MINOR1}.${VER_MINOR2}-${INST_FILENAME}"

VIAddVersionKey "ProductName" "D-Fend Reborn"
VIAddVersionKey "ProductVersion" "${VER_MAYOR}.${VER_MINOR1}.${VER_MINOR2}"
VIAddVersionKey "Comments" "${PrgName} is a Frontend for DOSBox"
VIAddVersionKey "CompanyName" "Written by Alexander Herzog"
VIAddVersionKey "LegalCopyright" "Licensed under the GPL v3"
!ifdef Update
  VIAddVersionKey "FileDescription" "Update installer for ${PrgName}"
!else
  VIAddVersionKey "FileDescription" "Installer for ${PrgName}"
!endif
VIAddVersionKey "FileVersion" "${VER_MAYOR}.${VER_MINOR1}.${VER_MINOR2}"
VIProductVersion "${VER_MAYOR}.${VER_MINOR1}.${VER_MINOR2}.0"



; Global variables
; ============================================================

Var DataInstDir
Var InstallDataType
Var AdminOK
Var PrgNameAddOn



; Initial settings
; ============================================================

Name "${PrgName}"
BrandingText "${PrgName} $PrgNameAddOn"

SetCompressor /solid lzma
!if /FileExists "Tools\upx.exe"
  !packhdr "$%TEMP%\exehead.tmp" 'Tools\upx.exe "$%TEMP%\exehead.tmp"'
!endif

RequestExecutionLevel highest
XPStyle on

; D-Fend Reborn is a 64 bit program: "C:\Program Files" (an existing
; installation is found via the registry and keeps its folder)
InstallDir "$PROGRAMFILES64\D-Fend Reborn\"
InstallDirRegKey HKLM "Software\D-Fend Reborn" "ProgramFolder"

!insertmacro MUI_RESERVEFILE_LANGDLL

ShowInstDetails show
ShowUninstDetails nevershow






; Settings for the modern user interface (MUI)
; ============================================================

!define MUI_ICON "${NSISDIR}\Contrib\Graphics\Icons\orange-install.ico"
!define MUI_UNICON "${NSISDIR}\Contrib\Graphics\Icons\orange-uninstall.ico"
!define MUI_HEADERIMAGE
!define MUI_HEADERIMAGE_BITMAP "${NSISDIR}\Contrib\Graphics\Header\win.bmp"
!define MUI_HEADERIMAGE_UNBITMAP "Images\1035.bmp"
!ifdef Update
  !define MUI_WELCOMEFINISHPAGE_BITMAP "Images\D-Fend Reborn - Update Installer.bmp"
!else
  !define MUI_WELCOMEFINISHPAGE_BITMAP "Images\D-Fend Reborn - Installer.bmp"
!endif
!define MUI_UNWELCOMEFINISHPAGE_BITMAP "Images\27.bmp"

!define MUI_ABORTWARNING
!define MUI_WELCOMEPAGE_TITLE_3LINES
!ifdef Update
  !define MUI_WELCOMEPAGE_TEXT "$(LANGNAME_WelcomeTextUpdate)"
!else
  !define MUI_WELCOMEPAGE_TEXT "$(LANGNAME_WelcomeText)"
  !define MUI_LICENSEPAGE_HEADER_TEXT "$(LANGNAME_LICENSE_TITLE)"
  !define MUI_LICENSEPAGE_HEADER_SUBTEXT "$(LANGNAME_LICENSE_SUBTITLE)"
  !define MUI_LICENSEPAGE_TEXT_TOP "$(LANGNAME_LICENSE_TOP)"
  !define MUI_LICENSEPAGE_TEXT_BOTTOM "$(LANGNAME_LICENSE_BOTTOM)"
  !define MUI_LICENSEPAGE_BUTTON "$(LANGNAME_Next) >"
  !define MUI_COMPONENTSPAGE_SMALLDESC
!endif

!define MUI_DIRECTORYPAGE_TEXT_TOP "$(LANGNAME_DIRECTORYPAGE_TEXT_TOP)"

!define MUI_FINISHPAGE_TITLE_3LINES
!define MUI_FINISHPAGE_RUN
!define MUI_FINISHPAGE_RUN_FUNCTION ExecAppFile
!define MUI_FINISHPAGE_RUN_TEXT "$(LANGNAME_RunDFend)"
!define MUI_UNABORTWARNING

Function ExecAppFile
  ; Execute DFend as normal user (unelevated), otherwise data package installers can't close DFend
  ; (programs started by Explorer do not get the admin rights of the installer)
  Exec '"$WINDIR\explorer.exe" "$INSTDIR\DFend.exe"'
FunctionEnd

!define MUI_LANGDLL_REGISTRY_ROOT "HKLM" 
!define MUI_LANGDLL_REGISTRY_KEY "Software\D-Fend Reborn" 
!define MUI_LANGDLL_REGISTRY_VALUENAME "Installer Language"

!macro UninstallerPages
  !define MUI_WELCOMEPAGE_TITLE_3LINES
  !insertmacro MUI_UNPAGE_WELCOME
  !insertmacro MUI_UNPAGE_CONFIRM
  !insertmacro MUI_UNPAGE_INSTFILES
  !define MUI_FINISHPAGE_TITLE_3LINES
  !insertmacro MUI_UNPAGE_FINISH
!macroend



; Main settings for different languages
; ============================================================

!macro LanguageSetup
  !insertmacro MUI_LANGUAGE "English"

  !insertmacro MUI_LANGUAGE "PortugueseBR"
  !insertmacro MUI_LANGUAGE "Czech"
  !insertmacro MUI_LANGUAGE "Danish"
  !insertmacro MUI_LANGUAGE "Dutch"
  !insertmacro MUI_LANGUAGE "French"
  !insertmacro MUI_LANGUAGE "German"
  !insertmacro MUI_LANGUAGE "Italian"
  !insertmacro MUI_LANGUAGE "Polish"
  !insertmacro MUI_LANGUAGE "Russian"
  !insertmacro MUI_LANGUAGE "SimpChinese"
  !insertmacro MUI_LANGUAGE "Spanish"
  !insertmacro MUI_LANGUAGE "TradChinese"
  !insertmacro MUI_LANGUAGE "Turkish"

  !include "Languages\D-Fend-Reborn-Setup-Lang-BrazilianPortuguese.nsi"
  !include "Languages\D-Fend-Reborn-Setup-Lang-Czech.nsi"
  !include "Languages\D-Fend-Reborn-Setup-Lang-Danish.nsi"
  !include "Languages\D-Fend-Reborn-Setup-Lang-Dutch.nsi"
  !include "Languages\D-Fend-Reborn-Setup-Lang-English.nsi"
  !include "Languages\D-Fend-Reborn-Setup-Lang-French.nsi"
  !include "Languages\D-Fend-Reborn-Setup-Lang-German.nsi"
  !include "Languages\D-Fend-Reborn-Setup-Lang-Italian.nsi"
  !include "Languages\D-Fend-Reborn-Setup-Lang-Polish.nsi"
  !include "Languages\D-Fend-Reborn-Setup-Lang-Russian.nsi"
  !include "Languages\D-Fend-Reborn-Setup-Lang-Simplified_Chinese.nsi"
  !include "Languages\D-Fend-Reborn-Setup-Lang-Spanish.nsi"
  !include "Languages\D-Fend-Reborn-Setup-Lang-Traditional_Chinese.nsi"
  !include "Languages\D-Fend-Reborn-Setup-Lang-Turkish.nsi"
!macroend



; Definition of install sections
; ============================================================



; DOSBox (folder "DOSBox": DOSBox Staging)
; ============================================================
; Files of the DOSBox 0.74 that older versions included (DOSBox Staging
; replaces it; its dosbox.exe replaces DOSBox.exe). User files in the folder
; (key mapper files, own configuration files) are kept.

!macro RemoveOldDOSBox
  Delete "$INSTDIR\DOSBox\SDL.dll"
  Delete "$INSTDIR\DOSBox\SDL_net.dll"
  Delete "$INSTDIR\DOSBox\dosbox.conf"
  Delete "$INSTDIR\DOSBox\DOSBox 0.74 Manual.txt"
  Delete "$INSTDIR\DOSBox\DOSBox 0.74 Options.bat"
  Delete "$INSTDIR\DOSBox\Reset KeyMapper.bat"
  Delete "$INSTDIR\DOSBox\Reset Options.bat"
  Delete "$INSTDIR\DOSBox\Screenshots & Recordings.bat"
  Delete "$INSTDIR\DOSBox\*.lng"
  Delete "$INSTDIR\DOSBox\*.dosbox.conf"
  Delete "$INSTDIR\DOSBox\Readme-0.*.txt"
  Delete "$INSTDIR\DOSBox\Readme-0.*.rtf"
  Delete "$INSTDIR\DOSBox\EGA*.CPX"
  RmDir /r "$INSTDIR\DOSBox\Documentation"
  RmDir /r "$INSTDIR\DOSBox\Video Codec"
  RmDir /r "$INSTDIR\DOSBox\zmbv"
!macroend

; DOSBox Staging uses its "portable layout" (settings, key mapper file and
; captures in its own folder) if dosbox-staging.conf is next to dosbox.exe:
; portable installations of D-Fend Reborn keep everything in their folder

!macro DOSBoxPortableMode
  IfFileExists "$INSTDIR\DOSBox\dosbox-staging.conf" DOSBoxPortableModeDone
  FileOpen $0 "$INSTDIR\DOSBox\dosbox-staging.conf" w
  FileWrite $0 "# DOSBox Staging settings (portable copy of D-Fend Reborn)$\r$\n"
  FileClose $0
  DOSBoxPortableModeDone:
!macroend


; Program files (D-Fend Reborn)
; ============================================================
; ${STAGE} is the program folder created by "cmake --install" (see
; README_HOW_TO_BUILD.txt): DFend.exe, the Qt libraries and plugins, Bin, Lang,
; Help, IconSets, NewUserData and DOSBox (DOSBox Staging).

!ifndef STAGE
  !define STAGE "Stage"
!endif

!macro InstallProgramFiles
  SetOutPath "$INSTDIR"
  File /r /x NewUserData /x DOSBox /x Thumbs.db "${STAGE}\*.*"

  SetOutPath "$DataInstDir\Settings"
  File "${STAGE}\Bin\Cheats.xml"

  ; Files of D-Fend Reborn that are not used any more
  Delete "$INSTDIR\Bin\7za.dll"
  Delete "$INSTDIR\Bin\DelZip179.dll"
  Delete "$INSTDIR\Bin\mediaplr.dll"
  Delete "$INSTDIR\Bin\DFendGameExplorerData.dll"
  Delete "$INSTDIR\Lang\*.chm"
  Delete "$INSTDIR\Bin\UpdateCheck.exe"   ; (updates are installed by D-Fend Reborn itself)
  ; Qt plugins of versions before 0.5.2 (now all in the folder "Qt", see qt.conf)
  RmDir /r "$INSTDIR\platforms"
  RmDir /r "$INSTDIR\styles"
  RmDir /r "$INSTDIR\imageformats"
  RmDir /r "$INSTDIR\iconengines"
  RmDir /r "$INSTDIR\multimedia"
  RmDir /r "$INSTDIR\networkinformation"
  RmDir /r "$INSTDIR\tls"
  RmDir /r "$INSTDIR\generic"
  RmDir /r "$INSTDIR\translations"
  Delete "$INSTDIR\opengl32sw.dll"
  Delete "$INSTDIR\d3dcompiler_47.dll"
!macroend

!macro CommonSections
  !define TO_MS 2000
  
  # Not working well
  #Section -AllowCancel
  #  SectionIn RO
  #  GetDlgItem $0 $HWNDPARENT 2
  #  EnableWindow $0 1
  #SectionEnd

  Section "-CloseDFend"
    SectionIn RO	

    Push $0
    nsExec::Exec 'taskkill /IM DFend.exe'
    Pop $0
    IntCmp $0 0 0 DoneCloseDFend DoneCloseDFend
    Sleep 2000
    DoneCloseDFend:
    Pop $0
  SectionEnd

  Section "Uninstall"
    SetDetailsPrint none
  
    Push $0
    nsExec::Exec 'taskkill /IM DFend.exe'
    Pop $0
    IntCmp $0 0 0 DoneCloseDFendUninstall DoneCloseDFendUninstall
    Sleep 2000
    DoneCloseDFendUninstall:
    Pop $0  
  
    ClearErrors
    FileOpen $0 $INSTDIR\DFend.dat r
    IfErrors UninstallUserDataEnd
    FileRead $0 $1
    FileClose $0
    StrCmp $1 "USERDIRMODE" AskForUserDataDel
    Goto UninstallUserDataEnd
  
    AskForUserDataDel:
    ReadRegStr $DataInstDir HKLM "Software\D-Fend Reborn" "DataFolder"
    StrCmp $DataInstDir "" UninstallUserDataEnd
	
	IfFileExists $DataInstDir 0 UninstallUserDataEnd
    MessageBox MB_YESNO "$(LANGNAME_ConfirmDelUserData)" IDYES DelUserData IDNO UninstallUserDataEnd
    DelUserData:  
    RmDir /r $DataInstDir  
    UninstallUserDataEnd:
	
    RmDir /r $INSTDIR\Bin
	RmDir /r $INSTDIR\DOSBox
	RmDir /r $INSTDIR\IconSets
	RmDir /r $INSTDIR\Lang
	RmDir /r $INSTDIR\Help
	; Qt libraries and plugins
	Delete $INSTDIR\*.dll
	Delete $INSTDIR\vc_redist*.exe
	Delete $INSTDIR\qt.conf
	RmDir /r $INSTDIR\Qt
	RmDir /r $INSTDIR\platforms
	RmDir /r $INSTDIR\styles
	RmDir /r $INSTDIR\imageformats
	RmDir /r $INSTDIR\iconengines
	RmDir /r $INSTDIR\multimedia
	RmDir /r $INSTDIR\networkinformation
	RmDir /r $INSTDIR\tls
	RmDir /r $INSTDIR\generic
	RmDir /r $INSTDIR\translations
	RmDir /r $INSTDIR\NewUserData
	Delete $INSTDIR\DFend.dat
	Delete $INSTDIR\DFend.exe
	Delete $INSTDIR\Readme_OperationMode.txt
	Delete $INSTDIR\Uninstall.exe
    Delete $INSTDIR\ConfOpt.dat
    Delete $INSTDIR\ScummVM.dat
    Delete $INSTDIR\History.dat
    Delete $INSTDIR\Icons.ini  
    Delete $INSTDIR\DFend.ini
    Delete $INSTDIR\Links.txt;
    Delete $INSTDIR\SearchLinks.txt
    Delete "$INSTDIR\D-Fend Reborn DataInstaller.nsi"
	
    Push "$INSTDIR"
    Call un.isEmptyDir
    Pop $0
    StrCmp $0 1 RemoveInstDir
	MessageBox MB_YESNO "$(LANGNAME_ConfirmDelPrgDir)" IDYES RemoveInstDir IDNO RemoveInstDirDone
	RemoveInstDir:
	RmDir /r $INSTDIR
	RemoveInstDirDone:
  
    StrCmp $1 "PORTABLEMODE" NoUninstallFromStartMenu
	

	SetShellVarContext all
    RmDir /r "$SMPROGRAMS\D-Fend Reborn"
    Delete "$DESKTOP\D-Fend Reborn.lnk"
    SetShellVarContext current
    RmDir /r "$SMPROGRAMS\D-Fend Reborn"
    Delete "$DESKTOP\D-Fend Reborn.lnk"
    DeleteRegKey HKLM "${MUI_LANGDLL_REGISTRY_KEY}"
    DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\D-Fend Reborn"
    
	NoUninstallFromStartMenu:
	
  SectionEnd
  
!macroend





; Definition of NSIS functions
; ============================================================

!macro CommonUACCode
  ; D-Fend Reborn: the UAC plugin of D-Fend Reloaded (ANSI only) is replaced by
  ; the functions of NSIS (RequestExecutionLevel highest, UserInfo), so the
  ; installer can be a Unicode installer. Administrators are asked for the
  ; elevation when the installer starts; without admin rights only the
  ; portable mode can be used (as before).
  Function .onInit
    !ifdef Update
      StrCpy $PrgNameAddOn "UPDATE"
    !else
      StrCpy $PrgNameAddOn ""
    !endif

    UserInfo::GetAccountType
    Pop $0
    StrCmp $0 "Admin" 0 NoAdmin
    IntOp $AdminOK 1 + 0
    Goto SelLang
    NoAdmin:
    IntOp $AdminOK 0 + 0

    SelLang:
    !define MUI_LANGDLL_ALLLANGUAGES
    !insertmacro MUI_LANGDLL_DISPLAY
	!insertmacro ExtractInstallOptionFiles

    IntCmp $AdminOK 1 InitReturn
	!ifdef Update
      MessageBox MB_YESNO "$(LANGNAME_NeedAdminRightsUpdate)" IDYES RunAsAdmin IDNO InitReturn
	!else
	  MessageBox MB_YESNO "$(LANGNAME_NeedAdminRights)" IDYES RunAsAdmin IDNO InitReturn
	!endif
    RunAsAdmin:
    ; start the installer again with admin rights
    ExecShell "runas" "$EXEPATH"
    Quit

    InitReturn:
	!insertmacro BetaWarning
  FunctionEnd

  Function un.onInit
    StrCpy $PrgNameAddOn ""
    !insertmacro MUI_UNGETLANGUAGE
  FunctionEnd
!macroend



; Check for not installing DFR directly into the program folder
; ============================================================

Function .onVerifyInstDir
  GetFullPathName $0 $INSTDIR
  
  GetFullPathName $1 $PROGRAMFILES
  StrCmp $0 $1 +1 DirCheck1
  Abort

  DirCheck1:
  GetFullPathName $1 $PROGRAMFILES64
  StrCmp $0 $1 +1 DirCheck2
  Abort
  
  DirCheck2:
  GetFullPathName $1 $WINDIR
  StrCmp $0 $1 +1 DirCheckOK
  Abort
  
  DirCheckOK:
FunctionEnd



; Check if directory is empty
; ============================================================
Function un.isEmptyDir
  # Stack ->                    # Stack: <directory>
  Exch $0                       # Stack: $0
  Push $1                       # Stack: $1, $0
  FindFirst $0 $1 "$0\*.*"
  strcmp $1 "." 0 _notempty
    FindNext $0 $1
    strcmp $1 ".." 0 _notempty
      ClearErrors
      FindNext $0 $1
      IfErrors 0 _notempty
        FindClose $0
        Pop $1                  # Stack: $0
        StrCpy $0 1
        Exch $0                 # Stack: 1 (true)
        goto _end
     _notempty:
       FindClose $0
       Pop $1                   # Stack: $0
       StrCpy $0 0
       Exch $0                  # Stack: 0 (false)
  _end:
FunctionEnd