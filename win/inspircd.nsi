; *       +------------------------------------+
; *       | Inspire Internet Relay Chat Daemon |
; *       +------------------------------------+
; *
; *  InspIRCd: (C) 2002-2007 InspIRCd Development Team
; * See: http://www.inspircd.org/wiki/index.php/Credits
; *
; * This program is free but copyrighted software; see
; *            the file COPYING for details.
; *
; * ---------------------------------------------------

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

                       ;;;; SET THE BUILD TO BE PACKAGED HERE ;;;;

!define BUILD "release"

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

; HM NIS Edit Wizard helper defines
!define PRODUCT_NAME "InspIRCd"
!define PRODUCT_VERSION "1.1"
!define PRODUCT_PUBLISHER "InspIRCd Development Team"
!define PRODUCT_WEB_SITE "http://www.inspircd.org/"
!define PRODUCT_DIR_REGKEY "Software\Microsoft\Windows\CurrentVersion\App Paths\inspircd.exe"
!define PRODUCT_UNINST_KEY "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PRODUCT_NAME}"
!define PRODUCT_UNINST_ROOT_KEY "HKLM"
!define DOT_MAJOR "2"
!define DOT_MINOR "0"

SetCompressor bzip2

; MUI 1.67 compatible ------
!include "MUI.nsh"

; MUI Settings
!define MUI_ABORTWARNING
!define MUI_ICON "inspircd.ico"
!define MUI_UNICON "inspircd.ico"

; Welcome page
!insertmacro MUI_PAGE_WELCOME
; License page
!define MUI_LICENSEPAGE_CHECKBOX
!insertmacro MUI_PAGE_LICENSE "..\docs\COPYING"
; directory page
Page directory
; Components page
!insertmacro MUI_PAGE_COMPONENTS
; Instfiles page
!insertmacro MUI_PAGE_INSTFILES
; Finish page
!define MUI_FINISHPAGE_RUN "$INSTDIR\InspGUI.exe"
!insertmacro MUI_PAGE_FINISH

; Uninstaller pages
!insertmacro MUI_UNPAGE_INSTFILES

; Language files
!insertmacro MUI_LANGUAGE "English"

; Reserve files
!insertmacro MUI_RESERVEFILE_INSTALLOPTIONS

; MUI end ------

Name "${PRODUCT_NAME} ${PRODUCT_VERSION}"
OutFile "Setup.exe"
InstallDir "$PROGRAMFILES\InspIRCd"
InstallDirRegKey HKLM "${PRODUCT_DIR_REGKEY}" ""
ShowInstDetails show
ShowUnInstDetails show

Function IsDotNetInstalled
 
  StrCpy $0 "0"
  StrCpy $1 "SOFTWARE\Microsoft\.NETFramework" ;registry entry to look in.
  StrCpy $2 0
 
  StartEnum:
    ;Enumerate the versions installed.
    EnumRegKey $3 HKLM "$1\policy" $2
    
    ;If we don't find any versions installed, it's not here.
    StrCmp $3 "" noDotNet notEmpty
    
    ;We found something.
    notEmpty:
      ;Find out if the RegKey starts with 'v'.  
      ;If it doesn't, goto the next key.
      StrCpy $4 $3 1 0
      StrCmp $4 "v" +1 goNext
      StrCpy $4 $3 1 1
      
      ;It starts with 'v'.  Now check to see how the installed major version
      ;relates to our required major version.
      ;If it's equal check the minor version, if it's greater, 
      ;we found a good RegKey.
      IntCmp $4 ${DOT_MAJOR} +1 goNext yesDotNetReg
      ;Check the minor version.  If it's equal or greater to our requested 
      ;version then we're good.
      StrCpy $4 $3 1 3
      IntCmp $4 ${DOT_MINOR} yesDotNetReg goNext yesDotNetReg
 
    goNext:
      ;Go to the next RegKey.
      IntOp $2 $2 + 1
      goto StartEnum
 
  yesDotNetReg:
    ;Now that we've found a good RegKey, let's make sure it's actually
    ;installed by getting the install path and checking to see if the 
    ;mscorlib.dll exists.
    EnumRegValue $2 HKLM "$1\policy\$3" 0
    ;$2 should equal whatever comes after the major and minor versions 
    ;(ie, v1.1.4322)
    StrCmp $2 "" noDotNet
    ReadRegStr $4 HKLM $1 "InstallRoot"
    ;Hopefully the install root isn't empty.
    StrCmp $4 "" noDotNet
    ;build the actuall directory path to mscorlib.dll.
    StrCpy $4 "$4$3.$2\mscorlib.dll"
    IfFileExists $4 yesDotNet noDotNet
 
  noDotNet:
    MessageBox MB_OK "You do not have have v${DOT_MAJOR}.${DOT_MINOR} or greater of the .NET framework installed. This is required for the InspIRCd Monitor, however you can still launch the IRCd manually."
 
  yesDotNet:
    ;Everything checks out.  Go on with the rest of the installation.
    
FunctionEnd

Section "Binary Executable" SEC01
  Call IsDotNetInstalled
  SetOutPath "$INSTDIR"
  SetOverwrite ifnewer
  File "..\bin\${BUILD}\InspGUI.exe"
  CreateDirectory "$SMPROGRAMS\InspIRCd"
  CreateShortCut "$SMPROGRAMS\InspIRCd\InspIRCd.lnk" "$INSTDIR\InspGUI.exe"
  SetOutPath "$INSTDIR\bin"
  SetOverwrite ifnewer
  File "..\bin\${BUILD}\bin\inspircd.exe"
SectionEnd

Section "Config Files" SEC02
  SetOutPath "$INSTDIR\conf"
  File "..\conf\inspircd.motd.example"
  File "..\conf\inspircd.helpop-full.example"
  File "..\conf\inspircd.helpop.example"
  File "..\conf\inspircd.filter.example"
  File "..\docs\inspircd.conf.example"
  File "..\conf\inspircd.censor.example"
  File "..\conf\inspircd.rules.example"
  File "..\conf\inspircd.quotes.example"
SectionEnd

Section "Command Handlers" SEC03
  SetOutPath "$INSTDIR\lib"
  File "..\bin\${BUILD}\lib\cmd_*.so"
SectionEnd

Section "Modules" SEC04
  SetOutPath "$INSTDIR\modules"
  File "..\bin\${BUILD}\modules\m_*.so"
SectionEnd

Section  "SSL Modules" SEC05
  SetOutPath "$INSTDIR\bin"
  SetOverwrite ifnewer
  File "..\bin\${BUILD}\bin\libgcrypt-11.dll"
  File "..\bin\${BUILD}\bin\libgnutls-13.dll"
  File "..\bin\${BUILD}\bin\libgnutls-extra-13.dll"
  File "..\bin\${BUILD}\bin\libgnutls-openssl-13.dll"
  File "..\bin\${BUILD}\bin\libgpg-error-0.dll"
  File "..\bin\${BUILD}\bin\libopencdk-8.dll"
  File "..\bin\${BUILD}\bin\libtasn1-3.dll"
  SetOutPath "$INSTDIR\modules"
  File "d:\temp\m_ssl_gnutls.so"
  File "d:\temp\m_sslinfo.so"
  File "d:\temp\m_ssl_oper_cert.so"
  SetOutPath "$INSTDIR\conf"
  SetOverwrite off
  File "key.pem"
  File "cert.pem"
SectionEnd

Section  "Regexp Modules" SEC06
  SetOutPath "$INSTDIR\bin"
  SetOverwrite ifnewer
  File "..\bin\${BUILD}\bin\pcre.dll"
  SetOutPath "$INSTDIR\modules"
  File "d:\temp\m_filter_pcre.so"
SectionEnd

Section -AdditionalIcons
  SetOutPath $INSTDIR
  WriteIniStr "$INSTDIR\${PRODUCT_NAME}.url" "InternetShortcut" "URL" "${PRODUCT_WEB_SITE}"
  CreateShortCut "$SMPROGRAMS\InspIRCd\InspIRCd Website.lnk" "$INSTDIR\${PRODUCT_NAME}.url"
  CreateShortCut "$SMPROGRAMS\InspIRCd\Uninstall.lnk" "$INSTDIR\uninst.exe"
SectionEnd

Section -Post
  WriteUninstaller "$INSTDIR\uninst.exe"
  WriteRegStr HKLM "${PRODUCT_DIR_REGKEY}" "" "$INSTDIR\bin\inspircd.exe"
  WriteRegStr ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "DisplayName" "$(^Name)"
  WriteRegStr ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "UninstallString" "$INSTDIR\uninst.exe"
  WriteRegStr ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "DisplayIcon" "$INSTDIR\bin\inspircd.exe"
  WriteRegStr ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "DisplayVersion" "${PRODUCT_VERSION}"
  WriteRegStr ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "URLInfoAbout" "${PRODUCT_WEB_SITE}"
  WriteRegStr ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "Publisher" "${PRODUCT_PUBLISHER}"
  MessageBox MB_ICONINFORMATION|MB_OK "InspIRCd was successfully installed. Remember to edit your configuration file in $INSTDIR\conf!"
SectionEnd

; Section descriptions
!insertmacro MUI_FUNCTION_DESCRIPTION_BEGIN
  !insertmacro MUI_DESCRIPTION_TEXT ${SEC01} "Actual executable"
  !insertmacro MUI_DESCRIPTION_TEXT ${SEC03} "Command modules"
  !insertmacro MUI_DESCRIPTION_TEXT ${SEC02} "Default configuration files"
  !insertmacro MUI_DESCRIPTION_TEXT ${SEC04} "Optional non-SSL modules"
  !insertmacro MUI_DESCRIPTION_TEXT ${SEC05} "SSL modules and GnuTLS DLL libraries"
  !insertmacro MUI_DESCRIPTION_TEXT ${SEC06} "Regular expression module and PCRE DLL library"
!insertmacro MUI_FUNCTION_DESCRIPTION_END


Function un.onUninstSuccess
  HideWindow
  MessageBox MB_ICONINFORMATION|MB_OK "$(^Name) was successfully removed from your computer."
FunctionEnd

Function .onInit
  SectionSetFlags ${SEC01} 17
  SectionSetFlags ${SEC03} 17
  StrCpy $INSTDIR "$PROGRAMFILES\InspIRCd"
FunctionEnd

Function un.onInit
  MessageBox MB_ICONQUESTION|MB_YESNO|MB_DEFBUTTON2 "Are you sure you want to completely remove $(^Name) and all of its components?" IDYES +2
  Abort
FunctionEnd

Section Uninstall
  Delete "$INSTDIR\${PRODUCT_NAME}.url"
  Delete "$INSTDIR\uninst.exe"
  Delete "$INSTDIR\modules\m_*.so"
  Delete "$INSTDIR\lib\cmd_*.so"
  Delete "$INSTDIR\conf\inspircd.quotes.example"
  Delete "$INSTDIR\conf\inspircd.rules.example"
  Delete "$INSTDIR\conf\inspircd.censor.example"
  Delete "$INSTDIR\conf\inspircd.conf.example"
  Delete "$INSTDIR\conf\inspircd.filter.example"
  Delete "$INSTDIR\conf\inspircd.helpop.example"
  Delete "$INSTDIR\conf\inspircd.helpop-full.example"
  Delete "$INSTDIR\conf\inspircd.motd.example"
  Delete "$INSTDIR\bin\inspircd.exe"
  Delete "$INSTDIR\bin\*.dll"
  Delete "$INSTDIR\InspGUI.exe"
  Delete "$SMPROGRAMS\InspIRCd\Uninstall.lnk"
  Delete "$SMPROGRAMS\InspIRCd\InspIRCd Website.lnk"
  Delete "$SMPROGRAMS\InspIRCd\InspIRCd.lnk"

  RMDir "$SMPROGRAMS\InspIRCd"
  RMDir "$INSTDIR\modules"
  RMDir "$INSTDIR\lib"
  RMDir "$INSTDIR\conf"
  RMDir "$INSTDIR\bin"
  RMDir "$INSTDIR"

  DeleteRegKey ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}"
  DeleteRegKey HKLM "${PRODUCT_DIR_REGKEY}"
  SetAutoClose true
SectionEnd
