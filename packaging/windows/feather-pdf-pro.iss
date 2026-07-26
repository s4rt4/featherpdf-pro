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
; The release workflow passes the version from CMakeLists.txt as
; `iscc /DAppVersion=<x.y.z>`; the fallback keeps local builds working.
#ifndef AppVersion
  #define AppVersion "1.0.0"
#endif
#define AppExe "feather-pdf-pro.exe"
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

; Branding. All three come from the logo SVG via scripts/make-branding.py; the
; wizard images are listed at several sizes so Setup can pick one that matches
; the user's display scaling instead of stretching a 96-DPI bitmap.
SetupIconFile=..\..\resources\windows\feather.ico
WizardImageFile=wizard-large-164.bmp,wizard-large-246.bmp,wizard-large-328.bmp
WizardSmallImageFile=wizard-small-55.bmp,wizard-small-83.bmp,wizard-small-110.bmp
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

; The OCR engine is only offered when scripts/stage-tesseract.ps1 was run
; before compiling this script; without it the installer simply has no "ocr"
; component and Text Recognition falls back to a system-installed Tesseract.
#define TesseractStaged DirExists(StagingDir + "\bin\tools\tesseract")

[Types]
Name: "full"; Description: "Full installation"
Name: "compact"; Description: "Compact installation (no OCR engine)"
Name: "custom"; Description: "Custom installation"; Flags: iscustom

[Components]
Name: "app"; Description: "{#AppName}"; Types: full compact custom; Flags: fixed
#if TesseractStaged
Name: "ocr"; Description: "OCR engine (Tesseract + English/Indonesian data)"; Types: full
#endif

; The whole staged prefix ships, not just bin: windeployqt puts the Qt plugins
; in a sibling plugins\ directory that bin\qt.conf points at, and without them
; the app cannot initialize a platform plugin at all.
[Files]
Source: "{#StagingDir}\*"; DestDir: "{app}"; Excludes: "\bin\tools\tesseract\*"; Flags: recursesubdirs ignoreversion; Components: app
#if TesseractStaged
Source: "{#StagingDir}\bin\tools\tesseract\*"; DestDir: "{app}\bin\tools\tesseract"; Flags: recursesubdirs ignoreversion; Components: ocr
#endif
; Shell icons for PDF files. They live outside the staged app because nothing
; but the registry entries below refers to them.
Source: "..\..\resources\windows\pdf-document.ico"; DestDir: "{app}\bin"; Flags: ignoreversion; Components: app
Source: "..\..\resources\windows\pdf-badge.ico"; DestDir: "{app}\bin"; Flags: ignoreversion; Components: app

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
; Two icons, the way Acrobat and Foxit do it: DefaultIcon is the full document
; icon Explorer falls back to when a cover cannot be rendered, and TypeOverlay
; is the small badge the shell stamps on covers that *can* be rendered — so a
; page thumbnail still reads as a PDF that Feather opens.
Root: HKLM; Subkey: "SOFTWARE\Classes\FeatherPDF.Document\DefaultIcon"; ValueType: string; ValueData: "{app}\bin\pdf-document.ico"
Root: HKLM; Subkey: "SOFTWARE\Classes\FeatherPDF.Document"; ValueType: string; ValueName: "TypeOverlay"; ValueData: "{app}\bin\pdf-badge.ico"
Root: HKLM; Subkey: "SOFTWARE\Classes\FeatherPDF.Document\shell\open\command"; ValueType: string; ValueData: """{app}\bin\{#AppExe}"" ""%1"""
Root: HKLM; Subkey: "SOFTWARE\Classes\.pdf\OpenWithProgids"; ValueType: string; ValueName: "FeatherPDF.Document"; ValueData: ""; Flags: uninsdeletevalue
Root: HKLM; Subkey: "SOFTWARE\Classes\.pdf"; ValueType: string; ValueData: "FeatherPDF.Document"; Tasks: associate

; Explorer thumbnails: feather-thumb.dll renders page one via Windows.Data.Pdf
; inside the shell's surrogate process. Registered under Feather's ProgID and
; under the Applications key (the ProgID Windows uses when the user picks
; Feather through Open With), so previews appear exactly when Feather is the
; default PDF app. {{e357fccd-…} is the shell's thumbnail-handler category.
#define ThumbClsid "{{C6DD57D7-9B9D-45BE-9881-AACF7F842E7A}"
Root: HKLM; Subkey: "SOFTWARE\Classes\CLSID\{#ThumbClsid}"; ValueType: string; ValueData: "Feather PDF Thumbnail Provider"; Flags: uninsdeletekey
Root: HKLM; Subkey: "SOFTWARE\Classes\CLSID\{#ThumbClsid}\InprocServer32"; ValueType: string; ValueData: "{app}\bin\feather-thumb.dll"
Root: HKLM; Subkey: "SOFTWARE\Classes\CLSID\{#ThumbClsid}\InprocServer32"; ValueType: string; ValueName: "ThreadingModel"; ValueData: "Both"
Root: HKLM; Subkey: "SOFTWARE\Classes\FeatherPDF.Document\shellex\{{e357fccd-a995-4576-b01f-234630154e96}"; ValueType: string; ValueData: "{#ThumbClsid}"
Root: HKLM; Subkey: "SOFTWARE\Classes\Applications\{#AppExe}\shellex\{{e357fccd-a995-4576-b01f-234630154e96}"; ValueType: string; ValueData: "{#ThumbClsid}"
Root: HKLM; Subkey: "SOFTWARE\Microsoft\Windows\CurrentVersion\Shell Extensions\Approved"; ValueType: string; ValueName: "{#ThumbClsid}"; ValueData: "Feather PDF Thumbnail Provider"; Flags: uninsdeletevalue

; Right-click actions on any PDF — run headless, drop the result beside the
; source (the CLI toasts nothing; Explorer shows the new file appear).
Root: HKLM; Subkey: "SOFTWARE\Classes\SystemFileAssociations\.pdf\shell\FeatherCompress"; ValueType: string; ValueData: "Compress with Feather PDF"; Tasks: context; Flags: uninsdeletekey
Root: HKLM; Subkey: "SOFTWARE\Classes\SystemFileAssociations\.pdf\shell\FeatherCompress\command"; ValueType: string; ValueData: """{app}\bin\{#AppExe}"" optimize ""%1"" ""%1.compressed.pdf"""; Tasks: context
Root: HKLM; Subkey: "SOFTWARE\Classes\SystemFileAssociations\.pdf\shell\FeatherSanitize"; ValueType: string; ValueData: "Remove hidden info with Feather PDF"; Tasks: context; Flags: uninsdeletekey
Root: HKLM; Subkey: "SOFTWARE\Classes\SystemFileAssociations\.pdf\shell\FeatherSanitize\command"; ValueType: string; ValueData: """{app}\bin\{#AppExe}"" sanitize ""%1"" ""%1.clean.pdf"""; Tasks: context

[Run]
Filename: "{app}\bin\{#AppExe}"; Description: "Launch {#AppName}"; Flags: nowait postinstall skipifsilent
