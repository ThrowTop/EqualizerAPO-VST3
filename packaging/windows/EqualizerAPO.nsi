Unicode True
RequestExecutionLevel admin

!include "MUI2.nsh"
!include "nsDialogs.nsh"
!include "LogicLib.nsh"
!include "FileFunc.nsh"
!include "x64.nsh"

!ifndef STAGE_DIR
    !error "STAGE_DIR was not defined"
!endif
!ifndef OUT_FILE
    !error "OUT_FILE was not defined"
!endif
!ifndef VERSION
    !define VERSION "development"
!endif

Name "EqualizerAPO-VST3 ${VERSION}"
OutFile "${OUT_FILE}"
InstallDir "$PROGRAMFILES64\EqualizerAPO-VST3"
InstallDirRegKey HKLM "SOFTWARE\EqualizerAPO-VST3" "InstallPath"
VIProductVersion "${VERSION}.0"
VIAddVersionKey /LANG=1033 "ProductName" "EqualizerAPO-VST3"
VIAddVersionKey /LANG=1033 "FileDescription" "EqualizerAPO-VST3 Setup"
VIAddVersionKey /LANG=1033 "FileVersion" "${VERSION}"
VIAddVersionKey /LANG=1033 "ProductVersion" "${VERSION}"
VIAddVersionKey /LANG=1033 "LegalCopyright" "EqualizerAPO-VST3 contributors"
SetCompressor /SOLID lzma
SetOverwrite on
ShowInstDetails show

Var ConfigFile
Var ConfigText
Var ConfigBrowse

!define MUI_ABORTWARNING
!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_LICENSE "${STAGE_DIR}\License.txt"
!insertmacro MUI_PAGE_DIRECTORY
Page custom ConfigPageCreate ConfigPageLeave
!insertmacro MUI_PAGE_INSTFILES
!define MUI_FINISHPAGE_RUN "$INSTDIR\Editor.exe"
!define MUI_FINISHPAGE_RUN_TEXT "Open Configuration Editor"
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_LANGUAGE "English"

Function .onInit
    ${IfNot} ${RunningX64}
        MessageBox MB_OK|MB_ICONSTOP "This Equalizer APO build requires 64-bit Windows."
        Abort
    ${EndIf}
    SetRegView 64
    ReadRegStr $0 HKLM "SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\EqualizerAPO" "UninstallString"
    ${If} $0 != ""
        MessageBox MB_OK|MB_ICONSTOP "Equalizer APO is currently installed. EqualizerAPO-VST3 uses the same Windows audio registrations and cannot run side by side with it. Uninstall Equalizer APO, then run this setup again."
        Abort
    ${EndIf}
    ReadRegStr $ConfigFile HKLM "SOFTWARE\EqualizerAPO" "ConfigFile"
    ${If} $ConfigFile == ""
        StrCpy $ConfigFile "$DOCUMENTS\EqualizerAPO-VST3\config.txt"
    ${EndIf}
FunctionEnd

Function ConfigPageCreate
    !insertmacro MUI_HEADER_TEXT "Active configuration" "Choose the exact .txt file loaded by the system APO."
    nsDialogs::Create 1018
    Pop $0
    ${If} $0 == error
        Abort
    ${EndIf}

    ${NSD_CreateLabel} 0 0 100% 26u "Keep an existing Equalizer APO configuration or choose where a minimal new one should be created. You can change this later in Configuration Editor."
    Pop $0
    ${NSD_CreateText} 0 35u 82% 13u "$ConfigFile"
    Pop $ConfigText
    ${NSD_CreateBrowseButton} 84% 34u 16% 15u "Browse..."
    Pop $ConfigBrowse
    ${NSD_OnClick} $ConfigBrowse ConfigBrowseClicked
    nsDialogs::Show
FunctionEnd

Function ConfigBrowseClicked
    nsDialogs::SelectFileDialog save "$ConfigFile" "EqualizerAPO-VST3 configurations (*.txt)|*.txt"
    Pop $0
    ${If} $0 != error
        StrCpy $ConfigFile $0
        ${NSD_SetText} $ConfigText $ConfigFile
    ${EndIf}
FunctionEnd

Function ConfigPageLeave
    ${NSD_GetText} $ConfigText $ConfigFile
    ${If} $ConfigFile == ""
        MessageBox MB_OK|MB_ICONEXCLAMATION "Choose a .txt configuration file."
        Abort
    ${EndIf}
    GetFullPathName $ConfigFile $ConfigFile
    ${GetFileExt} "$ConfigFile" $0
    StrCmp $0 "txt" config_extension_valid
    MessageBox MB_OK|MB_ICONEXCLAMATION "The active configuration must be a .txt file."
    Abort
config_extension_valid:
FunctionEnd

Section "EqualizerAPO-VST3" SecMain
    SetRegView 64
    SetOutPath "$INSTDIR"

    ; Windows permits a loaded DLL to be renamed. The new DLL can then be
    ; copied into place and is picked up by the Audio Service restart below.
    Delete /REBOOTOK "$INSTDIR\EqualizerAPO.dll.previous"
    Rename "$INSTDIR\EqualizerAPO.dll" "$INSTDIR\EqualizerAPO.dll.previous"
    File /r "${STAGE_DIR}\*.*"
    CreateDirectory "$INSTDIR\VSTPlugins"
    CreateDirectory "$INSTDIR\VST3"

    ExecWait '"$WINDIR\Sysnative\WindowsPowerShell\v1.0\powershell.exe" -NoLogo -NoProfile -ExecutionPolicy Bypass -File "$INSTDIR\install.ps1" -ConfigFile "$ConfigFile"' $0
    ${If} $0 != 0
        MessageBox MB_OK|MB_ICONSTOP "EqualizerAPO-VST3 registration failed (exit code $0)."
        Abort
    ${EndIf}

    ExecWait '"$INSTDIR\ConfigControl.exe" "$ConfigFile"' $0
    ${If} $0 != 0
        MessageBox MB_OK|MB_ICONSTOP "The configuration was saved, but Windows Audio could not be restarted (exit code $0)."
        Abort
    ${EndIf}

    Delete /REBOOTOK "$INSTDIR\EqualizerAPO.dll.previous"
    WriteUninstaller "$INSTDIR\Uninstall.exe"

    CreateDirectory "$SMPROGRAMS\EqualizerAPO-VST3"
    CreateShortcut "$SMPROGRAMS\EqualizerAPO-VST3\Configuration Editor.lnk" "$INSTDIR\Editor.exe"
    CreateShortcut "$SMPROGRAMS\EqualizerAPO-VST3\Uninstall.lnk" "$INSTDIR\Uninstall.exe"

    WriteRegStr HKLM "SOFTWARE\EqualizerAPO-VST3" "InstallPath" "$INSTDIR"
    WriteRegStr HKLM "SOFTWARE\EqualizerAPO-VST3" "Version" "${VERSION}"
    WriteRegStr HKLM "SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\EqualizerAPO-VST3" "DisplayName" "EqualizerAPO-VST3"
    WriteRegStr HKLM "SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\EqualizerAPO-VST3" "DisplayVersion" "${VERSION}"
    WriteRegStr HKLM "SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\EqualizerAPO-VST3" "Publisher" "EqualizerAPO-VST3 contributors"
    WriteRegStr HKLM "SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\EqualizerAPO-VST3" "URLInfoAbout" "https://github.com/ThrowTop/EqualizerAPO-VST3"
    WriteRegStr HKLM "SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\EqualizerAPO-VST3" "InstallLocation" "$INSTDIR"
    WriteRegStr HKLM "SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\EqualizerAPO-VST3" "UninstallString" '"$INSTDIR\Uninstall.exe"'
    WriteRegDWORD HKLM "SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\EqualizerAPO-VST3" "NoModify" 1
    WriteRegDWORD HKLM "SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\EqualizerAPO-VST3" "NoRepair" 1
SectionEnd

Section "Uninstall"
    SetRegView 64
    ExecWait '"$INSTDIR\DeviceControl.exe" --uninstall-all'
    ExecWait '"$SYSDIR\regsvr32.exe" /s /u "$INSTDIR\EqualizerAPO.dll"'
    ; Remove only files owned by this package. VSTPlugins and VST3 may contain
    ; user-provided plugins, so never recursively delete the install root.
    Delete /REBOOTOK "$INSTDIR\EqualizerAPO.dll"
    Delete /REBOOTOK "$INSTDIR\EqualizerAPO.dll.previous"
    Delete /REBOOTOK "$INSTDIR\ConfigControl.exe"
    Delete /REBOOTOK "$INSTDIR\DeviceControl.exe"
    Delete /REBOOTOK "$INSTDIR\Editor.exe"
    Delete /REBOOTOK "$INSTDIR\VoicemeeterClient.exe"
    Delete /REBOOTOK "$INSTDIR\UpdateChecker.exe"
    Delete /REBOOTOK "$INSTDIR\Benchmark.exe"
    Delete /REBOOTOK "$INSTDIR\concrt140.dll"
    Delete /REBOOTOK "$INSTDIR\fftw3f.dll"
    Delete /REBOOTOK "$INSTDIR\FLAC.dll"
    Delete /REBOOTOK "$INSTDIR\libmp3lame.DLL"
    Delete /REBOOTOK "$INSTDIR\mpg123.dll"
    Delete /REBOOTOK "$INSTDIR\msvcp140.dll"
    Delete /REBOOTOK "$INSTDIR\msvcp140_1.dll"
    Delete /REBOOTOK "$INSTDIR\msvcp140_2.dll"
    Delete /REBOOTOK "$INSTDIR\msvcp140_atomic_wait.dll"
    Delete /REBOOTOK "$INSTDIR\msvcp140_codecvt_ids.dll"
    Delete /REBOOTOK "$INSTDIR\ogg.dll"
    Delete /REBOOTOK "$INSTDIR\opus.dll"
    Delete /REBOOTOK "$INSTDIR\Qt6Core.dll"
    Delete /REBOOTOK "$INSTDIR\Qt6Gui.dll"
    Delete /REBOOTOK "$INSTDIR\Qt6Svg.dll"
    Delete /REBOOTOK "$INSTDIR\Qt6Widgets.dll"
    Delete /REBOOTOK "$INSTDIR\sndfile.dll"
    Delete /REBOOTOK "$INSTDIR\vcruntime140.dll"
    Delete /REBOOTOK "$INSTDIR\vcruntime140_1.dll"
    Delete /REBOOTOK "$INSTDIR\vorbis.dll"
    Delete /REBOOTOK "$INSTDIR\vorbisenc.dll"
    Delete /REBOOTOK "$INSTDIR\install.bat"
    Delete /REBOOTOK "$INSTDIR\install.ps1"
    Delete /REBOOTOK "$INSTDIR\License.txt"
    Delete /REBOOTOK "$INSTDIR\README.txt"
    Delete /REBOOTOK "$INSTDIR\THIRD_PARTY_NOTICES.md"
    Delete /REBOOTOK "$INSTDIR\qt.conf"
    RMDir /r /REBOOTOK "$INSTDIR\docs"
    RMDir /r /REBOOTOK "$INSTDIR\licenses"
    RMDir /r /REBOOTOK "$INSTDIR\qt"
    Delete /REBOOTOK "$INSTDIR\Uninstall.exe"
    RMDir "$INSTDIR\VSTPlugins"
    RMDir "$INSTDIR\VST3"
    RMDir "$INSTDIR"
    RMDir /r "$SMPROGRAMS\EqualizerAPO-VST3"
    DeleteRegKey HKLM "SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\EqualizerAPO-VST3"
    DeleteRegKey HKLM "SOFTWARE\EqualizerAPO-VST3"
    DeleteRegKey HKLM "SOFTWARE\EqualizerAPO"
SectionEnd
