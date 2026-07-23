; Feather PDF Pro — Inno Setup installer.
;
; Build the app first (cmake --preset windows-msvc, then
; cmake --install build --prefix staging --config RelWithDebInfo, which also
; runs windeployqt), then compile this script with Inno Setup 6:
;   iscc packaging\windows\feather-pdf-pro.iss
;
; Registers the app, a PDF file association ("Open with Feather PDF Pro"),
; and Explorer right-click actions that run the headless CLI — the Windows
; counterparts of the Linux build's Nautilus/Dolphin integration.

#define AppName "Feather PDF Pro"
#define AppVersion "0.0.1"
#define AppExe "feather-pdf.exe"
#define AppPublisher "Feather PDF contributors"
#define AppURL "https://github.com/s4rt4/featherpdf-pro"
#define StagingDir "..\..\staging"

[Setup]
AppId={{9A2B7C61-4E4D-4F5B-9B1E-A7FE41D0C201}
AppName={#AppName}
AppVersion={#AppVersion}
AppPublisher={#AppPublisher}
AppPublisherURL={#AppURL}
AppSupportURL={#AppURL}/issues
DefaultDirName={autopf}\{#AppName}
DefaultGroupName={#AppName}
UninstallDisplayIcon={app}\bin\{#AppExe}
LicenseFile=..\..\LICENSE
OutputBaseFilename=feather-pdf-pro-{#AppVersion}-setup
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
ChangesAssociations=yes

[Tasks]
Name: "associate"; Description: "Make {#AppName} the default app for PDF files"; Flags: unchecked
Name: "context"; Description: "Add Feather PDF actions to the Explorer right-click menu"

[Files]
Source: "{#StagingDir}\bin\*"; DestDir: "{app}\bin"; Flags: recursesubdirs ignoreversion

[Icons]
Name: "{group}\{#AppName}"; Filename: "{app}\bin\{#AppExe}"
Name: "{group}\Uninstall {#AppName}"; Filename: "{uninstallexe}"

[Registry]
; App registration so "Open with…" always lists Feather.
Root: HKLM; Subkey: "SOFTWARE\Classes\Applications\{#AppExe}"; ValueType: string; ValueName: "FriendlyAppName"; ValueData: "{#AppName}"; Flags: uninsdeletekey
Root: HKLM; Subkey: "SOFTWARE\Classes\Applications\{#AppExe}\shell\open\command"; ValueType: string; ValueData: """{app}\bin\{#AppExe}"" ""%1"""
Root: HKLM; Subkey: "SOFTWARE\Classes\Applications\{#AppExe}\SupportedTypes"; ValueType: string; ValueName: ".pdf"; ValueData: ""

; Feather's ProgID + optional default association.
Root: HKLM; Subkey: "SOFTWARE\Classes\FeatherPDF.Document"; ValueType: string; ValueData: "PDF Document"; Flags: uninsdeletekey
Root: HKLM; Subkey: "SOFTWARE\Classes\FeatherPDF.Document\DefaultIcon"; ValueType: string; ValueData: "{app}\bin\{#AppExe},0"
Root: HKLM; Subkey: "SOFTWARE\Classes\FeatherPDF.Document\shell\open\command"; ValueType: string; ValueData: """{app}\bin\{#AppExe}"" ""%1"""
Root: HKLM; Subkey: "SOFTWARE\Classes\.pdf\OpenWithProgids"; ValueType: string; ValueName: "FeatherPDF.Document"; ValueData: ""; Flags: uninsdeletevalue
Root: HKLM; Subkey: "SOFTWARE\Classes\.pdf"; ValueType: string; ValueData: "FeatherPDF.Document"; Tasks: associate

; Right-click actions on any PDF — run headless, drop the result beside the
; source (the CLI toasts nothing; Explorer shows the new file appear).
Root: HKLM; Subkey: "SOFTWARE\Classes\SystemFileAssociations\.pdf\shell\FeatherCompress"; ValueType: string; ValueData: "Compress with Feather PDF"; Tasks: context; Flags: uninsdeletekey
Root: HKLM; Subkey: "SOFTWARE\Classes\SystemFileAssociations\.pdf\shell\FeatherCompress\command"; ValueType: string; ValueData: """{app}\bin\{#AppExe}"" optimize ""%1"" ""%1.compressed.pdf"""; Tasks: context
Root: HKLM; Subkey: "SOFTWARE\Classes\SystemFileAssociations\.pdf\shell\FeatherSanitize"; ValueType: string; ValueData: "Remove hidden info with Feather PDF"; Tasks: context; Flags: uninsdeletekey
Root: HKLM; Subkey: "SOFTWARE\Classes\SystemFileAssociations\.pdf\shell\FeatherSanitize\command"; ValueType: string; ValueData: """{app}\bin\{#AppExe}"" sanitize ""%1"" ""%1.clean.pdf"""; Tasks: context

[Run]
Filename: "{app}\bin\{#AppExe}"; Description: "Launch {#AppName}"; Flags: nowait postinstall skipifsilent
