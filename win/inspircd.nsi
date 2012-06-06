;
; InspIRCd -- Internet Relay Chat Daemon
;
;   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
;   Copyright (C) 2007 Craig Edwards <craigedwards@brainbox.cc>
;
; This file is part of InspIRCd.  InspIRCd is free software: you can
; redistribute it and/or modify it under the terms of the GNU General Public
; License as published by the Free Software Foundation, version 2.
;
; This program is distributed in the hope that it will be useful, but WITHOUT
; ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
; FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
; details.
;
; You should have received a copy of the GNU General Public License
; along with this program.  If not, see <http://www.gnu.org/licenses/>.
;

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

                       ;;;; SET THE BUILD TO BE PACKAGED HERE ;;;;

!define BUILD "release"

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

; HM NIS Edit Wizard helper defines
!define PRODUCT_NAME "InspIRCd"
!define PRODUCT_VERSION "2.0"
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

; Uninstaller pages
!insertmacro MUI_UNPAGE_INSTFILES

; Language files
!insertmacro MUI_LANGUAGE "English"

; Reserve files
!insertmacro MUI_RESERVEFILE_INSTALLOPTIONS

; MUI end ------

Name "${PRODUCT_NAME} ${PRODUCT_VERSION}"
OutFile "${PRODUCT_NAME}-${PRODUCT_VERSION}-Setup.exe"
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
  CreateDirectory "$SMPROGRAMS\InspIRCd"
  CreateDirectory "$INSTDIR\logs"
  CreateDirectory "$INSTDIR\data"
  CreateShortCut "$SMPROGRAMS\InspIRCd\InspIRCd.lnk" "$INSTDIR\inspircd.exe"
  SetOutPath "$INSTDIR"
  SetOverwrite ifnewer
  File "..\bin\${BUILD}\inspircd.exe"
  DetailPrint "Installing InspIRCd service..."
  nsExec::Exec /TIMEOUT=30000 '"$INSTDIR\inspircd.exe" --installservice'
SectionEnd

Section "Config Files" SEC02
  SetOutPath "$INSTDIR\conf"
  File "..\docs\*.example"
SectionEnd

Section "Command Handlers" SEC03
  SetOutPath "$INSTDIR\modules"
  File "..\bin\${BUILD}\modules\cmd_*.so"
SectionEnd

Section "Modules" SEC04
  SetOutPath "$INSTDIR\modules"
  File "..\bin\${BUILD}\modules\m_*.so"
SectionEnd

Section -AdditionalIcons
  SetOutPath $INSTDIR
  WriteIniStr "$INSTDIR\${PRODUCT_NAME}.url" "InternetShortcut" "URL" "${PRODUCT_WEB_SITE}"
  CreateShortCut "$SMPROGRAMS\InspIRCd\InspIRCd Website.lnk" "$INSTDIR\${PRODUCT_NAME}.url"
  CreateShortCut "$SMPROGRAMS\InspIRCd\Uninstall.lnk" "$INSTDIR\uninst.exe"
SectionEnd

Section -Post
  WriteUninstaller "$INSTDIR\uninst.exe"
  WriteRegStr HKLM "${PRODUCT_DIR_REGKEY}" "" "$INSTDIR\inspircd.exe"
  WriteRegStr ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "DisplayName" "$(^Name)"
  WriteRegStr ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "UninstallString" "$INSTDIR\uninst.exe"
  WriteRegStr ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "DisplayIcon" "$INSTDIR\inspircd.exe"
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
  DetailPrint "Uninstalling InspIRCd service..."
  nsExec::Exec /TIMEOUT=30000 '"$INSTDIR\inspircd.exe" --removeservice'
  Delete "$INSTDIR\${PRODUCT_NAME}.url"
  Delete "$INSTDIR\uninst.exe"
  Delete "$INSTDIR\modules\*.so"
  Delete "$INSTDIR\conf\*.example"
  Delete "$INSTDIR\*.log"
  Delete "$INSTDIR\logs\*"
  Delete "$INSTDIR\data\*"
  Delete "$INSTDIR\inspircd.exe"
  Delete "$SMPROGRAMS\InspIRCd\Uninstall.lnk"
  Delete "$SMPROGRAMS\InspIRCd\InspIRCd Website.lnk"
  Delete "$SMPROGRAMS\InspIRCd\InspIRCd.lnk"

  RMDir "$SMPROGRAMS\InspIRCd"
  RMDir "$INSTDIR\modules"
  RMDir "$INSTDIR\conf"
  RMDir "$INSTDIR\logs"
  RMDir "$INSTDIR\data"
  RMDir "$INSTDIR"

  DeleteRegKey ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}"
  DeleteRegKey HKLM "${PRODUCT_DIR_REGKEY}"
  SetAutoClose true
SectionEnd
