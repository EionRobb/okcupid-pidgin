; Script based on the Skype4Pidgin and Off-the-Record Messaging NSI files


SetCompress off

; todo: SetBrandingImage
; HM NIS Edit Wizard helper defines
!define PRODUCT_NAME "OkCupid for Pidgin"
!define PRODUCT_VERSION "1.00"
!define PRODUCT_PUBLISHER "Eion Robb and Kevin Ghadyani"
!define PRODUCT_WEB_SITE "http://okcupid-pidgin.googlecode.com/"
!define PRODUCT_UNINST_KEY "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PRODUCT_NAME}"
!define PRODUCT_UNINST_ROOT_KEY "HKLM"

; MUI 1.67 compatible ------
!include "MUI.nsh"

; MUI Settings
!define MUI_ABORTWARNING
!define MUI_ICON "${NSISDIR}\Contrib\Graphics\Icons\modern-install.ico"
!define MUI_UNICON "${NSISDIR}\Contrib\Graphics\Icons\modern-uninstall.ico"

; Welcome page
!insertmacro MUI_PAGE_WELCOME
; License page
!insertmacro MUI_PAGE_LICENSE "COPYING"
; Instfiles page
!insertmacro MUI_PAGE_INSTFILES
!define MUI_FINISHPAGE_RUN
!define MUI_FINISHPAGE_RUN_TEXT "Run Pidgin"
!define MUI_FINISHPAGE_RUN_FUNCTION "RunPidgin"
!insertmacro MUI_PAGE_FINISH

; Uninstaller pages
;!insertmacro MUI_UNPAGE_INSTFILES

; Language files
!insertmacro MUI_LANGUAGE "English"

; MUI end ------

Name "${PRODUCT_NAME} ${PRODUCT_VERSION}"
OutFile "okcupid-pidgin.exe"

Var "PidginDir"

ShowInstDetails show
ShowUnInstDetails show

Section "MainSection" SEC01
    ;Check for pidgin installation
    Call GetPidginInstPath
    
    SetOverwrite try
    
	SetOutPath "$PidginDir\pixmaps\pidgin"
	File "/oname=protocols\16\okcupid.png" "icons\16\okcupid.png"
	File "/oname=protocols\22\okcupid.png" "icons\22\okcupid.png"
	File "/oname=protocols\48\okcupid.png" "icons\48\okcupid.png"

    SetOverwrite try
	copy:
		ClearErrors
		Delete "$PidginDir\plugins\libokcupid.dll"
		IfErrors dllbusy
		SetOutPath "$PidginDir\plugins"
	    	File "libokcupid.dll"
		Goto after_copy
	dllbusy:
		MessageBox MB_RETRYCANCEL "libokcupid.dll is busy. Please close Pidgin (including tray icon) and try again" IDCANCEL cancel
		Goto copy
	cancel:
		Abort "Installation of ${PRODUCT_NAME} aborted"
	after_copy:
	
	SetOutPath "$PidginDir"
	File "libjson-glib-1.0.dll"
	
SectionEnd

Function GetPidginInstPath
  Push $0
  ReadRegStr $0 HKLM "Software\pidgin" ""
	IfFileExists "$0\pidgin.exe" cont
	ReadRegStr $0 HKCU "Software\pidgin" ""
	IfFileExists "$0\pidgin.exe" cont
		MessageBox MB_OK|MB_ICONINFORMATION "Failed to find Pidgin installation."
		Abort "Failed to find Pidgin installation. Please install Pidgin first."
  cont:
	StrCpy $PidginDir $0
FunctionEnd

Function RunPidgin
	ExecShell "" "$PidginDir\pidgin.exe"
FunctionEnd

