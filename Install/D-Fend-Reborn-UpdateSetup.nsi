; NSI SCRIPT FOR D-FEND REBORN (UPDATE)
; ============================================================

!include VersionSettings.nsi
!insertmacro VersionData
!define INST_FILENAME "UpdateSetup.exe"
!define Update
!include CommonTools.nsi



; Settings for the modern user interface (MUI)
; ============================================================

!include FileFunc.nsh
!insertmacro GetParameters
!insertmacro GetOptions

!define MUI_PAGE_CUSTOMFUNCTION_PRE SkipPageIfAutoUpdate
!insertmacro MUI_PAGE_WELCOME
!define MUI_PAGE_CUSTOMFUNCTION_PRE SkipPageIfAutoUpdate
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!define MUI_PAGE_CUSTOMFUNCTION_PRE SkipPageAndCloseIfAutoUpdate
!insertmacro MUI_PAGE_FINISH

!insertmacro UninstallerPages
!insertmacro LanguageSetup

Function SkipPageIfAutoUpdate
	${GetParameters} $R0
    ClearErrors
    ${GetOptions} $R0 /A $R0
    IfErrors +2 0
	Abort
FunctionEnd

Function SkipPageAndCloseIfAutoUpdate
	${GetParameters} $R0
    ClearErrors
    ${GetOptions} $R0 /A $R0
    IfErrors ShowFinishPage 0
	Call ExecAppFile
	Abort
	ShowFinishPage:
FunctionEnd



; Definition of install sections
; ============================================================

!insertmacro CommonSections

Section "$(LANGNAME_DFendReborn)" ID_DFend
  SectionIn RO
  
  SetDetailsPrint none
  
  ; Read installation type
  ; ($InstallDataType=0 <=> Prg dir mode, $InstallDataType=1 <=> User dir mode, $InstallDataType=2 <=> Portable mode; in user dir mode $DataInstDir will contain the data directory otherwise $INSTDIR)
  
  IfFileExists "$INSTDIR\DFend.dat" StartCheck
  MessageBox MB_OK "$(LANGNAME_NoInstallationFound)"
  Quit
  StartCheck:  
  StrCpy $DataInstDir $INSTDIR  
  ClearErrors
  FileOpen $0 $INSTDIR\DFend.dat r
  IfErrors ReadInstTypeEnd
  FileRead $0 $1
  FileClose $0
  StrCmp $1 "USERDIRMODE" InstUserMode
  StrCmp $1 "PORTABLEMODE" InstPortableMode
  IntOp $InstallDataType 0 + 0
  Goto ReadInstTypeEnd
  InstPortableMode:
  ; portable copy: files are updated in place, no uninstaller / registry entries
  IntOp $InstallDataType 0 + 2
  Goto ReadInstTypeEnd
  InstUserMode:
  IntOp $InstallDataType 0 + 1
  strcpy $DataInstDir "$PROFILE\D-Fend Reborn"
  ReadInstTypeEnd:
  
  ; Update main files
  
  SetDetailsPrint both
  DetailPrint "$(LANGNAME_UpdateDFendReborn)"

  SetDetailsPrint listonly
  !insertmacro InstallProgramFiles
  

  ; Update templates

  IntCmp $InstallDataType 1 WriteNewUserDir
    ; Prg dir mode -> Update files directly

	SetDetailsPrint listonly
    SetOutPath "$DataInstDir\AutoSetup"
	SetDetailsPrint none
    File "${STAGE}\NewUserData\AutoSetup\*.prof"	
	
    SetOutPath "$DataInstDir\Capture\DOSBox DOS"
    File "${STAGE}\NewUserData\Capture\DOSBox DOS\*.*"

    SetOutPath "$DataInstDir\Templates"
    File "${STAGE}\NewUserData\Templates\*.prof"

    SetOutPath "$DataInstDir\IconLibrary"
    File "${STAGE}\NewUserData\IconLibrary\*.*"
	
    SetOutPath "$DataInstDir\Settings"
    File "${STAGE}\NewUserData\Icons.ini"
	File "${STAGE}\NewUserData\DataReader.xml"

  Goto TemplateWritingFinish
  WriteNewUserDir:  
    ; User dir mode -> Update files in NewUserData folder

	SetDetailsPrint listonly
    SetOutPath "$INSTDIR\NewUserData\AutoSetup"
	SetDetailsPrint none
    File "${STAGE}\NewUserData\AutoSetup\*.prof"

    SetOutPath "$INSTDIR\NewUserData\Capture\DOSBox DOS"
    File "${STAGE}\NewUserData\Capture\DOSBox DOS\*.*"

    SetOutPath "$INSTDIR\NewUserData\Templates"
    File "${STAGE}\NewUserData\Templates\*.prof"

    SetOutPath "$INSTDIR\NewUserData\IconLibrary"
    File "${STAGE}\NewUserData\IconLibrary\*.*"
	
    SetOutPath "$INSTDIR\NewUserData"
    File "${STAGE}\NewUserData\Icons.ini"
	File "${STAGE}\NewUserData\DataReader.xml"
	
	; Copy FreeDOS files to NewUserData directory
	
	IfFileExists "$DataInstDir\VirtualHD\FREEDOS\*.*" 0 TemplateWritingFinish
	IfFileExists "$INSTDIR\NewUserData\FREEDOS\*.*" TemplateWritingFinish
	
	CreateDirectory "$INSTDIR\NewUserData\FREEDOS"
	CopyFiles /SILENT "$DataInstDir\VirtualHD\FREEDOS\*.*" "$INSTDIR\NewUserData\FREEDOS\"

  TemplateWritingFinish:
  
  ; Install DOSZip
  
  SetDetailsPrint both
  DetailPrint "$(LANGNAME_InstallDOSZip)"
  SetDetailsPrint listonly

  IntCmp $InstallDataType 1 DoszipToNewUserDir
    SetOutPath "$DataInstDir\VirtualHD\DOSZIP"
  Goto DoszipWritingStart
  DoszipToNewUserDir:
    SetOutPath "$INSTDIR\NewUserData\DOSZIP"
  DoszipWritingStart:
  
  SetDetailsPrint none
  File /r "${STAGE}\NewUserData\DOSZIP\*.*"
  
  IntCmp $InstallDataType 2 NoUninstallerUpdate
  
  ; Update uninstaller
  
  SetDetailsPrint both
  DetailPrint "$(LANGNAME_UpdateUninstaller)"
  SetDetailsPrint listonly
  SetOutPath "$INSTDIR"
  SetDetailsPrint none
  
  WriteUninstaller "Uninstall.exe"
  SetShellVarContext all
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\D-Fend Reborn" "DisplayVersion" "${VER_MAYOR}.${VER_MINOR1}.${VER_MINOR2}"
  
  
  NoUninstallerUpdate:
  
  ; Update DOSBox installation
  
  IfFileExists "$INSTDIR\DOSBox\*.*" 0 NoDOSBoxUpdate
  
  SetDetailsPrint both
  DetailPrint "$(LANGNAME_UpdateDOSBox)"
  SetDetailsPrint listonly
  SetOutPath "$INSTDIR\DOSBox"
  SetDetailsPrint none
  
  ; DOSBox Staging replaces the DOSBox 0.74 of older versions
  !insertmacro RemoveOldDOSBox
  SetOutPath "$INSTDIR\DOSBox"
  File /r "${STAGE}\DOSBox\*.*"

  IntCmp $InstallDataType 2 0 NoDOSBoxShortcutUpdate NoDOSBoxShortcutUpdate
  !insertmacro DOSBoxPortableMode
  Goto NoDOSBoxShortcutUpdate2
  NoDOSBoxShortcutUpdate:
  ; Start menu: shortcuts of DOSBox Staging instead of those of DOSBox 0.74
  SetShellVarContext all
  IfFileExists "$SMPROGRAMS\D-Fend Reborn\DOSBox\*.*" 0 NoDOSBoxShortcutUpdate2
  RmDir /r "$SMPROGRAMS\D-Fend Reborn\DOSBox"
  CreateDirectory "$SMPROGRAMS\D-Fend Reborn\DOSBox"
  CreateShortCut "$SMPROGRAMS\D-Fend Reborn\DOSBox\DOSBox Staging.lnk" "$INSTDIR\DOSBox\dosbox.exe" "" "$INSTDIR\DOSBox\dosbox.exe" 0
  CreateShortCut "$SMPROGRAMS\D-Fend Reborn\DOSBox\DOSBox Staging manual.lnk" "$INSTDIR\DOSBox\resources\docs\manual\index.html"
  NoDOSBoxShortcutUpdate2:
  
  NoDOSBoxUpdate:
SectionEnd



; Definition of NSIS functions
; ============================================================

!macro ExtractInstallOptionFiles
!macroend

!insertmacro CommonUACCode