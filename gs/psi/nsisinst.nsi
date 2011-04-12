;  Copyright (C) 2001-2006 Artifex Software, Inc.
;  All Rights Reserved.
;
;  This software is provided AS-IS with no warranty, either express or
;  implied.
;
;  This software is distributed under license and may not be copied, modified
;  or distributed except as expressly authorized under the terms of that
;  license.  Refer to licensing information at http://www.artifex.com/
;  or contact Artifex Software, Inc.,  7 Mt. Lassen Drive - Suite A-134,
;  San Rafael, CA  94903, U.S.A., +1(415)492-9861, for further information.
;

; This script should be compiled with e.g.:
;     makensis -NOCD -DTARGET=gs900w32 -DVERSION=9.00 psi/nsisinst.nsi
 

; Significant differences with the older winzipse-based installer are:

; The Winzipse-based installer opens README on successful install.
; Besides the installation directory, the name of the short-cuts and 
; whether it applies to "All Users" are optional, and so is whether
; to generate a CJK cidfmap, (default to NO for silent installation).

; The NSIS-based installer does not open README on successful install; 
; only whether to generate a CJK cidfmap (default to YES for silent
; installation) is optional; short-cuts are for "All Users". On the other
; hand, it removes the short-cuts on Uninstall (which the Winzipse-based
; installer doesn't do) and also does not leave behind empty directories.

; the following is from: http://nsis.sourceforge.net/StrRep
!define StrRep "!insertmacro StrRep"
!macro StrRep output string old new
    Push "${string}"
    Push "${old}"
    Push "${new}"
    !ifdef __UNINSTALL__
        Call un.StrRep
    !else
        Call StrRep
    !endif
    Pop ${output}
!macroend
 
!macro Func_StrRep un
    Function ${un}StrRep
        Exch $R2 ;new
        Exch 1
        Exch $R1 ;old
        Exch 2
        Exch $R0 ;string
        Push $R3
        Push $R4
        Push $R5
        Push $R6
        Push $R7
        Push $R8
        Push $R9
 
        StrCpy $R3 0
        StrLen $R4 $R1
        StrLen $R6 $R0
        StrLen $R9 $R2
        loop:
            StrCpy $R5 $R0 $R4 $R3
            StrCmp $R5 $R1 found
            StrCmp $R3 $R6 done
            IntOp $R3 $R3 + 1 ;move offset by 1 to check the next character
            Goto loop
        found:
            StrCpy $R5 $R0 $R3
            IntOp $R8 $R3 + $R4
            StrCpy $R7 $R0 "" $R8
            StrCpy $R0 $R5$R2$R7
            StrLen $R6 $R0
            IntOp $R3 $R3 + $R9 ;move offset by length of the replacement string
            Goto loop
        done:
 
        Pop $R9
        Pop $R8
        Pop $R7
        Pop $R6
        Pop $R5
        Pop $R4
        Pop $R3
        Push $R0
        Push $R1
        Pop $R0
        Pop $R1
        Pop $R0
        Pop $R2
        Exch $R1
    FunctionEnd
!macroend
!insertmacro Func_StrRep ""

!ifndef TARGET
!define TARGET gs899w32
!endif

!ifndef VERSION
!define VERSION 8.99
!endif

SetCompressor /SOLID /FINAL lzma
XPStyle on
CRCCheck on

!include "MUI2.nsh"
; for detecting if running on x64 machine.
!include "x64.nsh"

!define MUI_FINISHPAGE_RUN
!define MUI_FINISHPAGE_RUN_TEXT "Generate cidfmap for Windows CJK TrueType fonts"
!define MUI_FINISHPAGE_RUN_FUNCTION CJKGen
; !define MUI_FINISHPAGE_RUN_NOTCHECKED
!define MUI_FINISHPAGE_SHOWREADME "$INSTDIR\doc\Readme.htm"
; MUI_FINISHPAGE_SHOWREADME_NOTCHECKED
!define MUI_FINISHPAGE_LINK          "Visit the Ghostscript web site"
!define MUI_FINISHPAGE_LINK_LOCATION http://www.ghostscript.com/

!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_LICENSE "LICENSE"
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_LANGUAGE "English"

!searchparse /ignorecase /noerrors "${TARGET}" w WINTYPE
!echo "Building ${WINTYPE}-bit installer"

Name "GPL Ghostscript"
OutFile "${TARGET}.exe"
!if "${WINTYPE}" == "64"
Icon   obj64\gswin.ico
UninstallIcon   obj64\gswin.ico
!else
Icon   obj\gswin.ico
UninstallIcon   obj\gswin.ico
!endif

RequestExecutionLevel admin

!ifndef VERSION
!define VERSION 8.72
!endif

; Some default compiler settings (uncomment and change at will):
; SetCompress auto ; (can be off or force)
; SetDatablockOptimize on ; (can be off)
; CRCCheck on ; (can be off)
; AutoCloseWindow false ; (can be true for the window go away automatically at end)
; ShowInstDetails hide ; (can be show to have them shown, or nevershow to disable)
; SetDateSave off ; (can be on to have files restored to their orginal date)

BrandingText "Artifex Software Inc."
LicenseText "You must agree to this license before installing."
LicenseData "LICENSE"

!if "${WINTYPE}" == "64"
InstallDir "$PROGRAMFILES64\gs\gs${VERSION}"
!else
InstallDir "$PROGRAMFILES\gs\gs${VERSION}"
!endif

DirText "Select the directory to install GPL Ghostscript in:"

Section "" ; (default section)
SetOutPath "$INSTDIR"
CreateDirectory "$INSTDIR\bin"
; add files / whatever that need to be installed here.
File   /r /x contrib /x lcms /x lcms2 /x expat /x jasper /x .svn doc
File   /r /x zlib /x expat /x .svn /x lcms2 examples
File   /r /x contrib /x expat /x luratech /x lwf_jp2 /x lcms /x lcms2 /x .svn lib
File /oname=bin\gsdll${WINTYPE}.dll .\bin\gsdll${WINTYPE}.dll
File /oname=bin\gsdll${WINTYPE}.lib .\bin\gsdll${WINTYPE}.lib
File /oname=bin\gswin${WINTYPE}.exe .\bin\gswin${WINTYPE}.exe
File /oname=bin\gswin${WINTYPE}c.exe .\bin\gswin${WINTYPE}c.exe

!if "${WINTYPE}" == "64"
  SetRegView 64
!endif

WriteRegStr HKEY_LOCAL_MACHINE "Software\GPL Ghostscript\${VERSION}" "GS_DLL" "$INSTDIR\bin\gsdll${WINTYPE}.dll"
WriteRegStr HKEY_LOCAL_MACHINE "Software\GPL Ghostscript\${VERSION}" "GS_LIB" "$INSTDIR\bin;$INSTDIR\lib;$INSTDIR\fonts"
WriteRegStr HKEY_LOCAL_MACHINE "Software\Artifex\GPL Ghostscript\${VERSION}" "" "$INSTDIR"
WriteRegStr HKEY_LOCAL_MACHINE "Software\Microsoft\Windows\CurrentVersion\Uninstall\GPL Ghostscript ${VERSION}" "DisplayName" "GPL Ghostscript"
WriteRegStr HKEY_LOCAL_MACHINE "Software\Microsoft\Windows\CurrentVersion\Uninstall\GPL Ghostscript ${VERSION}" "UninstallString" '"$INSTDIR\uninstgs.exe"'
WriteRegStr HKEY_LOCAL_MACHINE "Software\Microsoft\Windows\CurrentVersion\Uninstall\GPL Ghostscript ${VERSION}" "Publisher" "Artifex Software Inc."
WriteRegStr HKEY_LOCAL_MACHINE "Software\Microsoft\Windows\CurrentVersion\Uninstall\GPL Ghostscript ${VERSION}" "HelpLink" "http://www.ghostscript.com/"
WriteRegStr HKEY_LOCAL_MACHINE "Software\Microsoft\Windows\CurrentVersion\Uninstall\GPL Ghostscript ${VERSION}" "URLInfoAbout" "http://www.ghostscript.com/"
WriteRegStr HKEY_LOCAL_MACHINE "Software\Microsoft\Windows\CurrentVersion\Uninstall\GPL Ghostscript ${VERSION}" "DisplayVersion" "${VERSION}"
WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\GPL Ghostscript ${VERSION}" "NoModify" "1"
WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\GPL Ghostscript ${VERSION}" "NoRepair" "1"
; write out uninstaller
WriteUninstaller "$INSTDIR\uninstgs.exe"
SectionEnd ; end of default section

Function .onInstSuccess
    SetShellVarContext all
    CreateDirectory "$SMPROGRAMS\Ghostscript"
    CreateShortCut "$SMPROGRAMS\Ghostscript\Ghostscript ${VERSION}.LNK" "$INSTDIR\bin\gswin${WINTYPE}.exe" '"-I$INSTDIR\lib;$INSTDIR\..\fonts"'
    CreateShortCut "$SMPROGRAMS\Ghostscript\Ghostscript Readme ${VERSION}.LNK" "$INSTDIR\doc\Readme.htm"
    CreateShortCut "$SMPROGRAMS\Ghostscript\Uninstall Ghostscript ${VERSION}.LNK" "$INSTDIR\uninstgs.exe"
FunctionEnd

Function CJKGen
    ${StrRep} $0 "$FONTS" "\" "/"
    ${StrRep} $1 "$INSTDIR\lib\cidfmap" "\" "/"
    ${StrRep} $2 "$INSTDIR\lib\mkcidfm.ps" "\" "/"
    ExecWait '"$INSTDIR\bin\gswin${WINTYPE}c.exe" -q -dBATCH "-sFONTDIR=$0" "-sCIDFMAP=$1" "$2"'
FunctionEnd

Function .onInit
!if "${WINTYPE}" == "64"
    SetRegView 64
    ${IfNot} ${RunningX64}
        MessageBox MB_OK "64-bit Ghostscript should not be installed on 32-bit machines" /SD IDOK
        Abort
    ${EndIf}
!endif
    System::Call 'kernel32::CreateMutexA(i 0, i 0, t "GhostscriptInstaller") i .r1 ?e'
    Pop $R0
    StrCmp $R0 0 +3
    MessageBox MB_OK "The Ghostscript installer is already running." /SD IDOK
    Abort
FunctionEnd

Function Un.onInit
!if "${WINTYPE}" == "64"
    SetRegView 64
!endif
FunctionEnd

; begin uninstall settings/section
UninstallText "This will uninstall GPL Ghostscript from your system"

Section Uninstall
; add delete commands to delete whatever files/registry keys/etc you installed here.
SetShellVarContext all
Delete   "$SMPROGRAMS\Ghostscript\Ghostscript ${VERSION}.LNK"
Delete   "$SMPROGRAMS\Ghostscript\Ghostscript Readme ${VERSION}.LNK"
Delete   "$SMPROGRAMS\Ghostscript\Uninstall Ghostscript ${VERSION}.LNK"
RMDir    "$SMPROGRAMS\Ghostscript"
Delete   "$INSTDIR\uninstgs.exe"
!if "${WINTYPE}" == "64"
    SetRegView 64
!endif
DeleteRegKey HKEY_LOCAL_MACHINE "SOFTWARE\Artifex\GPL Ghostscript\${VERSION}"
DeleteRegKey HKEY_LOCAL_MACHINE "Software\Microsoft\Windows\CurrentVersion\Uninstall\GPL Ghostscript ${VERSION}"
DeleteRegKey HKEY_LOCAL_MACHINE "Software\GPL Ghostscript\${VERSION}"
RMDir /r "$INSTDIR\doc"
RMDir /r "$INSTDIR\examples"
RMDir /r "$INSTDIR\lib"
Delete   "$INSTDIR\bin\gsdll${WINTYPE}.dll"
Delete   "$INSTDIR\bin\gsdll${WINTYPE}.lib"
Delete   "$INSTDIR\bin\gswin${WINTYPE}.exe"
Delete   "$INSTDIR\bin\gswin${WINTYPE}c.exe"
RMDir    "$INSTDIR\bin"
RMDir    "$INSTDIR"
SectionEnd ; end of uninstall section

; eof
