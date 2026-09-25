; AegisubQT Windows installer
; Modelled after upstream Aegisub packages/win_installer/aegisub3.iss:
;  - full shell file-type registration for subtitle + media extensions
;  - Default Programs (Capabilities) registration
;  - per-user friendly names resolved from the executable's string table
; The [Files] payload is prepared by build-installer.ps1 into a clean staging
; directory (deployed Qt runtime, QML, assets, automation, locale .qm, VC CRT),
; so no build-system leftovers ever end up inside the installer.

#ifndef StagingDir
#define StagingDir "..\..\build\installer-staging"
#endif
#ifndef Version
#define Version "4.0.0"
#endif
#ifndef SourceRoot
#define SourceRoot "..\.."
#endif

[Setup]
AppId={{7A5C3E92-4B8D-4E6F-9C21-D3F04A5B9E17}
AppName=AegisubQT
AppVersion={#Version}
AppVerName=AegisubQT {#Version}
AppPublisher=AegisubQT Project
AppPublisherURL=https://github.com/AegisubQT/AegisubQT
AppSupportURL=https://github.com/AegisubQT/AegisubQT/issues
DefaultDirName={autopf}\AegisubQT
DefaultGroupName=AegisubQT
UninstallDisplayName=AegisubQT {#Version}
LicenseFile={#SourceRoot}\LICENSE
SetupIconFile={#StagingDir}\assets\icons_native\icon.ico
OutputDir=..\..\build
OutputBaseFilename=AegisubQT-{#Version}
ArchitecturesInstallIn64BitMode=x64compatible
ArchitecturesAllowed=x64compatible
PrivilegesRequired=poweruser
Compression=lzma2/max
SolidCompression=yes
WizardStyle=modern
MinVersion=10.0

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"
Name: "chs"; MessagesFile: "langs\ChineseSimplified.isl"
Name: "cht"; MessagesFile: "langs\ChineseTraditional.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[Files]
Source: "{#StagingDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{autoprograms}\AegisubQT\AegisubQT"; Filename: "{app}\AegisubQT.exe"
Name: "{autodesktop}\AegisubQT"; Filename: "{app}\AegisubQT.exe"; Tasks: desktopicon

[Registry]
; Application registration for the Open With dialogue
Root: HKLM; Subkey: "SOFTWARE\Classes\Applications\AegisubQT.exe"; ValueType: none; Flags: uninsdeletekey
Root: HKLM; Subkey: "SOFTWARE\Classes\Applications\AegisubQT.exe"; ValueType: string; ValueName: "FriendlyAppName"; ValueData: "@{app}\AegisubQT.exe,-10000"; Flags: uninsdeletekey
Root: HKLM; Subkey: "SOFTWARE\Classes\Applications\AegisubQT.exe"; ValueType: string; ValueName: "ApplicationCompany"; Flags: uninsdeletekey
Root: HKLM; Subkey: "SOFTWARE\Classes\Applications\AegisubQT.exe\shell"; ValueType: none; Flags: uninsdeletekey
Root: HKLM; Subkey: "SOFTWARE\Classes\Applications\AegisubQT.exe\shell\open"; ValueType: none; Flags: uninsdeletekey
Root: HKLM; Subkey: "SOFTWARE\Classes\Applications\AegisubQT.exe\shell\open"; ValueType: string; ValueName: "FriendlyAppName"; ValueData: "@{app}\AegisubQT.exe,-10000"; Flags: uninsdeletekey
Root: HKLM; Subkey: "SOFTWARE\Classes\Applications\AegisubQT.exe\shell\open\command"; ValueType: string; ValueData: """{app}\AegisubQT.exe"" ""%1"""; Flags: uninsdeletekey
Root: HKLM; Subkey: "SOFTWARE\Classes\Applications\AegisubQT.exe\SupportedTypes"; ValueType: none; Flags: uninsdeletekey
Root: HKLM; Subkey: "SOFTWARE\Classes\Applications\AegisubQT.exe\SupportedTypes"; ValueType: string; ValueName: ".ass"; ValueData: ""; Flags: uninsdeletekey
Root: HKLM; Subkey: "SOFTWARE\Classes\Applications\AegisubQT.exe\SupportedTypes"; ValueType: string; ValueName: ".ssa"; ValueData: ""; Flags: uninsdeletekey
Root: HKLM; Subkey: "SOFTWARE\Classes\Applications\AegisubQT.exe\SupportedTypes"; ValueType: string; ValueName: ".srt"; ValueData: ""; Flags: uninsdeletekey
Root: HKLM; Subkey: "SOFTWARE\Classes\Applications\AegisubQT.exe\SupportedTypes"; ValueType: string; ValueName: ".sub"; ValueData: ""; Flags: uninsdeletekey
Root: HKLM; Subkey: "SOFTWARE\Classes\Applications\AegisubQT.exe\SupportedTypes"; ValueType: string; ValueName: ".ttxt"; ValueData: ""; Flags: uninsdeletekey
Root: HKLM; Subkey: "SOFTWARE\Classes\Applications\AegisubQT.exe\SupportedTypes"; ValueType: string; ValueName: ".txt"; ValueData: ""; Flags: uninsdeletekey
Root: HKLM; Subkey: "SOFTWARE\Classes\Applications\AegisubQT.exe\SupportedTypes"; ValueType: string; ValueName: ".mkv"; ValueData: ""; Flags: uninsdeletekey
Root: HKLM; Subkey: "SOFTWARE\Classes\Applications\AegisubQT.exe\SupportedTypes"; ValueType: string; ValueName: ".mka"; ValueData: ""; Flags: uninsdeletekey
Root: HKLM; Subkey: "SOFTWARE\Classes\Applications\AegisubQT.exe\SupportedTypes"; ValueType: string; ValueName: ".mks"; ValueData: ""; Flags: uninsdeletekey
Root: HKLM; Subkey: "SOFTWARE\Classes\Applications\AegisubQT.exe\SupportedTypes"; ValueType: string; ValueName: ".avi"; ValueData: ""; Flags: uninsdeletekey
Root: HKLM; Subkey: "SOFTWARE\Classes\Applications\AegisubQT.exe\SupportedTypes"; ValueType: string; ValueName: ".mp3"; ValueData: ""; Flags: uninsdeletekey
Root: HKLM; Subkey: "SOFTWARE\Classes\Applications\AegisubQT.exe\SupportedTypes"; ValueType: string; ValueName: ".mp4"; ValueData: ""; Flags: uninsdeletekey
Root: HKLM; Subkey: "SOFTWARE\Classes\Applications\AegisubQT.exe\SupportedTypes"; ValueType: string; ValueName: ".aac"; ValueData: ""; Flags: uninsdeletekey
Root: HKLM; Subkey: "SOFTWARE\Classes\Applications\AegisubQT.exe\SupportedTypes"; ValueType: string; ValueName: ".m4a"; ValueData: ""; Flags: uninsdeletekey
Root: HKLM; Subkey: "SOFTWARE\Classes\Applications\AegisubQT.exe\SupportedTypes"; ValueType: string; ValueName: ".wav"; ValueData: ""; Flags: uninsdeletekey
Root: HKLM; Subkey: "SOFTWARE\Classes\Applications\AegisubQT.exe\SupportedTypes"; ValueType: string; ValueName: ".ogg"; ValueData: ""; Flags: uninsdeletekey
Root: HKLM; Subkey: "SOFTWARE\Classes\Applications\AegisubQT.exe\SupportedTypes"; ValueType: string; ValueName: ".avs"; ValueData: ""; Flags: uninsdeletekey
Root: HKLM; Subkey: "SOFTWARE\Classes\Applications\AegisubQT.exe\SupportedTypes"; ValueType: string; ValueName: ".opus"; ValueData: ""; Flags: uninsdeletekey
Root: HKLM; Subkey: "SOFTWARE\Classes\Applications\AegisubQT.exe\SupportedTypes"; ValueType: string; ValueName: ".h264"; ValueData: ""; Flags: uninsdeletekey
Root: HKLM; Subkey: "SOFTWARE\Classes\Applications\AegisubQT.exe\SupportedTypes"; ValueType: string; ValueName: ".hevc"; ValueData: ""; Flags: uninsdeletekey
Root: HKLM; Subkey: "SOFTWARE\Classes\Applications\AegisubQT.exe\SupportedTypes"; ValueType: string; ValueName: ".eac3"; ValueData: ""; Flags: uninsdeletekey
Root: HKLM; Subkey: "SOFTWARE\Classes\Applications\AegisubQT.exe\SupportedTypes"; ValueType: string; ValueName: ".webm"; ValueData: ""; Flags: uninsdeletekey
; Class for general subtitle formats
Root: HKLM; Subkey: "SOFTWARE\Classes\AegisubQT.Subtitle.1"; ValueType: string; ValueData: "AegisubQT subtitle file"; Flags: uninsdeletekey
Root: HKLM; Subkey: "SOFTWARE\Classes\AegisubQT.Subtitle.1"; ValueType: dword; ValueName: "EditFlags"; ValueData: $af0; Flags: uninsdeletekey
Root: HKLM; Subkey: "SOFTWARE\Classes\AegisubQT.Subtitle.1"; ValueType: string; ValueName: "FriendlyTypeName"; ValueData: "@{app}\AegisubQT.exe,-10101"; Flags: uninsdeletekey
Root: HKLM; Subkey: "SOFTWARE\Classes\AegisubQT.Subtitle.1\DefaultIcon"; ValueType: string; ValueData: "{app}\AegisubQT.exe,0"; Flags: uninsdeletekey
Root: HKLM; Subkey: "SOFTWARE\Classes\AegisubQT.Subtitle.1\shell"; ValueType: string; ValueData: "open"; Flags: uninsdeletekey
Root: HKLM; Subkey: "SOFTWARE\Classes\AegisubQT.Subtitle.1\shell\open"; ValueType: none; Flags: uninsdeletekey
Root: HKLM; Subkey: "SOFTWARE\Classes\AegisubQT.Subtitle.1\shell\open\command"; ValueType: string; ValueData: """{app}\AegisubQT.exe"" ""%L"""; Flags: uninsdeletekey
; Class for .ass files
Root: HKLM; Subkey: "SOFTWARE\Classes\AegisubQT.ASSA.1"; ValueType: string; ValueData: "AegisubQT Advanced SSA subtitles"; Flags: uninsdeletekey
Root: HKLM; Subkey: "SOFTWARE\Classes\AegisubQT.ASSA.1"; ValueType: dword; ValueName: "EditFlags"; ValueData: $af0; Flags: uninsdeletekey
Root: HKLM; Subkey: "SOFTWARE\Classes\AegisubQT.ASSA.1"; ValueType: string; ValueName: "FriendlyTypeName"; ValueData: "@{app}\AegisubQT.exe,-10102"; Flags: uninsdeletekey
Root: HKLM; Subkey: "SOFTWARE\Classes\AegisubQT.ASSA.1\DefaultIcon"; ValueType: string; ValueData: "{app}\AegisubQT.exe,0"; Flags: uninsdeletekey
Root: HKLM; Subkey: "SOFTWARE\Classes\AegisubQT.ASSA.1\shell"; ValueType: string; ValueData: "open"; Flags: uninsdeletekey
Root: HKLM; Subkey: "SOFTWARE\Classes\AegisubQT.ASSA.1\shell\open"; ValueType: none; Flags: uninsdeletekey
Root: HKLM; Subkey: "SOFTWARE\Classes\AegisubQT.ASSA.1\shell\open\command"; ValueType: string; ValueData: """{app}\AegisubQT.exe"" ""%L"""; Flags: uninsdeletekey
; Class for .ssa files
Root: HKLM; Subkey: "SOFTWARE\Classes\AegisubQT.SSA.1"; ValueType: string; ValueData: "AegisubQT SubStation Alpha subtitles"; Flags: uninsdeletekey
Root: HKLM; Subkey: "SOFTWARE\Classes\AegisubQT.SSA.1"; ValueType: dword; ValueName: "EditFlags"; ValueData: $af0; Flags: uninsdeletekey
Root: HKLM; Subkey: "SOFTWARE\Classes\AegisubQT.SSA.1"; ValueType: string; ValueName: "FriendlyTypeName"; ValueData: "@{app}\AegisubQT.exe,-10103"; Flags: uninsdeletekey
Root: HKLM; Subkey: "SOFTWARE\Classes\AegisubQT.SSA.1\DefaultIcon"; ValueType: string; ValueData: "{app}\AegisubQT.exe,0"; Flags: uninsdeletekey
Root: HKLM; Subkey: "SOFTWARE\Classes\AegisubQT.SSA.1\shell"; ValueType: string; ValueData: "open"; Flags: uninsdeletekey
Root: HKLM; Subkey: "SOFTWARE\Classes\AegisubQT.SSA.1\shell\open"; ValueType: none; Flags: uninsdeletekey
Root: HKLM; Subkey: "SOFTWARE\Classes\AegisubQT.SSA.1\shell\open\command"; ValueType: string; ValueData: """{app}\AegisubQT.exe"" ""%L"""; Flags: uninsdeletekey
; Class for .srt files
Root: HKLM; Subkey: "SOFTWARE\Classes\AegisubQT.SRT.1"; ValueType: string; ValueData: "AegisubQT SubRip text subtitles"; Flags: uninsdeletekey
Root: HKLM; Subkey: "SOFTWARE\Classes\AegisubQT.SRT.1"; ValueType: dword; ValueName: "EditFlags"; ValueData: $af0; Flags: uninsdeletekey
Root: HKLM; Subkey: "SOFTWARE\Classes\AegisubQT.SRT.1"; ValueType: string; ValueName: "FriendlyTypeName"; ValueData: "@{app}\AegisubQT.exe,-10104"; Flags: uninsdeletekey
Root: HKLM; Subkey: "SOFTWARE\Classes\AegisubQT.SRT.1\DefaultIcon"; ValueType: string; ValueData: "{app}\AegisubQT.exe,0"; Flags: uninsdeletekey
Root: HKLM; Subkey: "SOFTWARE\Classes\AegisubQT.SRT.1\shell"; ValueType: string; ValueData: "open"; Flags: uninsdeletekey
Root: HKLM; Subkey: "SOFTWARE\Classes\AegisubQT.SRT.1\shell\open"; ValueType: none; Flags: uninsdeletekey
Root: HKLM; Subkey: "SOFTWARE\Classes\AegisubQT.SRT.1\shell\open\command"; ValueType: string; ValueData: """{app}\AegisubQT.exe"" ""%L"""; Flags: uninsdeletekey
; Class for .ttxt files
Root: HKLM; Subkey: "SOFTWARE\Classes\AegisubQT.TTXT.1"; ValueType: string; ValueData: "AegisubQT MPEG-4 timed text"; Flags: uninsdeletekey
Root: HKLM; Subkey: "SOFTWARE\Classes\AegisubQT.TTXT.1"; ValueType: dword; ValueName: "EditFlags"; ValueData: $af0; Flags: uninsdeletekey
Root: HKLM; Subkey: "SOFTWARE\Classes\AegisubQT.TTXT.1"; ValueType: string; ValueName: "FriendlyTypeName"; ValueData: "@{app}\AegisubQT.exe,-10105"; Flags: uninsdeletekey
Root: HKLM; Subkey: "SOFTWARE\Classes\AegisubQT.TTXT.1\DefaultIcon"; ValueType: string; ValueData: "{app}\AegisubQT.exe,0"; Flags: uninsdeletekey
Root: HKLM; Subkey: "SOFTWARE\Classes\AegisubQT.TTXT.1\shell"; ValueType: string; ValueData: "open"; Flags: uninsdeletekey
Root: HKLM; Subkey: "SOFTWARE\Classes\AegisubQT.TTXT.1\shell\open"; ValueType: none; Flags: uninsdeletekey
Root: HKLM; Subkey: "SOFTWARE\Classes\AegisubQT.TTXT.1\shell\open\command"; ValueType: string; ValueData: """{app}\AegisubQT.exe"" ""%L"""; Flags: uninsdeletekey
; Class for .mks files
Root: HKLM; Subkey: "SOFTWARE\Classes\AegisubQT.MKS.1"; ValueType: string; ValueData: "AegisubQT Matroska subtitles"; Flags: uninsdeletekey
Root: HKLM; Subkey: "SOFTWARE\Classes\AegisubQT.MKS.1"; ValueType: dword; ValueName: "EditFlags"; ValueData: $af0; Flags: uninsdeletekey
Root: HKLM; Subkey: "SOFTWARE\Classes\AegisubQT.MKS.1"; ValueType: string; ValueName: "FriendlyTypeName"; ValueData: "@{app}\AegisubQT.exe,-10106"; Flags: uninsdeletekey
Root: HKLM; Subkey: "SOFTWARE\Classes\AegisubQT.MKS.1\DefaultIcon"; ValueType: string; ValueData: "{app}\AegisubQT.exe,0"; Flags: uninsdeletekey
Root: HKLM; Subkey: "SOFTWARE\Classes\AegisubQT.MKS.1\shell"; ValueType: string; ValueData: "open"; Flags: uninsdeletekey
Root: HKLM; Subkey: "SOFTWARE\Classes\AegisubQT.MKS.1\shell\open"; ValueType: none; Flags: uninsdeletekey
Root: HKLM; Subkey: "SOFTWARE\Classes\AegisubQT.MKS.1\shell\open\command"; ValueType: string; ValueData: """{app}\AegisubQT.exe"" ""%L"""; Flags: uninsdeletekey
; Class for .txt files
Root: HKLM; Subkey: "SOFTWARE\Classes\AegisubQT.TXT.1"; ValueType: string; ValueData: "AegisubQT raw text file"; Flags: uninsdeletekey
Root: HKLM; Subkey: "SOFTWARE\Classes\AegisubQT.TXT.1"; ValueType: dword; ValueName: "EditFlags"; ValueData: $af0; Flags: uninsdeletekey
Root: HKLM; Subkey: "SOFTWARE\Classes\AegisubQT.TXT.1"; ValueType: string; ValueName: "FriendlyTypeName"; ValueData: "@{app}\AegisubQT.exe,-10107"; Flags: uninsdeletekey
Root: HKLM; Subkey: "SOFTWARE\Classes\AegisubQT.TXT.1\DefaultIcon"; ValueType: string; ValueData: "{app}\AegisubQT.exe,0"; Flags: uninsdeletekey
Root: HKLM; Subkey: "SOFTWARE\Classes\AegisubQT.TXT.1\shell"; ValueType: string; ValueData: "open"; Flags: uninsdeletekey
Root: HKLM; Subkey: "SOFTWARE\Classes\AegisubQT.TXT.1\shell\open"; ValueType: none; Flags: uninsdeletekey
Root: HKLM; Subkey: "SOFTWARE\Classes\AegisubQT.TXT.1\shell\open\command"; ValueType: string; ValueData: """{app}\AegisubQT.exe"" ""%L"""; Flags: uninsdeletekey
; Class for undecideable media file types
Root: HKLM; Subkey: "SOFTWARE\Classes\AegisubQT.Media.1"; ValueType: string; ValueData: "AegisubQT media file"; Flags: uninsdeletekey
Root: HKLM; Subkey: "SOFTWARE\Classes\AegisubQT.Media.1"; ValueType: dword; ValueName: "EditFlags"; ValueData: $af0; Flags: uninsdeletekey
Root: HKLM; Subkey: "SOFTWARE\Classes\AegisubQT.Media.1"; ValueType: string; ValueName: "FriendlyTypeName"; ValueData: "@{app}\AegisubQT.exe,-10108"; Flags: uninsdeletekey
Root: HKLM; Subkey: "SOFTWARE\Classes\AegisubQT.Media.1\DefaultIcon"; ValueType: string; ValueData: "{app}\AegisubQT.exe,0"; Flags: uninsdeletekey
Root: HKLM; Subkey: "SOFTWARE\Classes\AegisubQT.Media.1\shell"; ValueType: string; ValueData: "open"; Flags: uninsdeletekey
Root: HKLM; Subkey: "SOFTWARE\Classes\AegisubQT.Media.1\shell\open"; ValueType: none; Flags: uninsdeletekey
Root: HKLM; Subkey: "SOFTWARE\Classes\AegisubQT.Media.1\shell\open\command"; ValueType: string; ValueData: """{app}\AegisubQT.exe"" ""%L"""; Flags: uninsdeletekey
; Class for audio file types
Root: HKLM; Subkey: "SOFTWARE\Classes\AegisubQT.Audio.1"; ValueType: string; ValueData: "AegisubQT audio file"; Flags: uninsdeletekey
Root: HKLM; Subkey: "SOFTWARE\Classes\AegisubQT.Audio.1"; ValueType: dword; ValueName: "EditFlags"; ValueData: $af0; Flags: uninsdeletekey
Root: HKLM; Subkey: "SOFTWARE\Classes\AegisubQT.Audio.1"; ValueType: string; ValueName: "FriendlyTypeName"; ValueData: "@{app}\AegisubQT.exe,-10109"; Flags: uninsdeletekey
Root: HKLM; Subkey: "SOFTWARE\Classes\AegisubQT.Audio.1\DefaultIcon"; ValueType: string; ValueData: "{app}\AegisubQT.exe,0"; Flags: uninsdeletekey
Root: HKLM; Subkey: "SOFTWARE\Classes\AegisubQT.Audio.1\shell"; ValueType: string; ValueData: "open"; Flags: uninsdeletekey
Root: HKLM; Subkey: "SOFTWARE\Classes\AegisubQT.Audio.1\shell\open"; ValueType: none; Flags: uninsdeletekey
Root: HKLM; Subkey: "SOFTWARE\Classes\AegisubQT.Audio.1\shell\open\command"; ValueType: string; ValueData: """{app}\AegisubQT.exe"" ""%L"""; Flags: uninsdeletekey
; Class for video file types
Root: HKLM; Subkey: "SOFTWARE\Classes\AegisubQT.Video.1"; ValueType: string; ValueData: "AegisubQT video file"; Flags: uninsdeletekey
Root: HKLM; Subkey: "SOFTWARE\Classes\AegisubQT.Video.1"; ValueType: dword; ValueName: "EditFlags"; ValueData: $af0; Flags: uninsdeletekey
Root: HKLM; Subkey: "SOFTWARE\Classes\AegisubQT.Video.1"; ValueType: string; ValueName: "FriendlyTypeName"; ValueData: "@{app}\AegisubQT.exe,-10110"; Flags: uninsdeletekey
Root: HKLM; Subkey: "SOFTWARE\Classes\AegisubQT.Video.1\DefaultIcon"; ValueType: string; ValueData: "{app}\AegisubQT.exe,0"; Flags: uninsdeletekey
Root: HKLM; Subkey: "SOFTWARE\Classes\AegisubQT.Video.1\shell"; ValueType: string; ValueData: "open"; Flags: uninsdeletekey
Root: HKLM; Subkey: "SOFTWARE\Classes\AegisubQT.Video.1\shell\open"; ValueType: none; Flags: uninsdeletekey
Root: HKLM; Subkey: "SOFTWARE\Classes\AegisubQT.Video.1\shell\open\command"; ValueType: string; ValueData: """{app}\AegisubQT.exe"" ""%L"""; Flags: uninsdeletekey
; Default Programs registration
Root: HKLM; Subkey: "SOFTWARE\AegisubQT"; ValueType: none; Flags: uninsdeletekey
Root: HKLM; Subkey: "SOFTWARE\AegisubQT\Capabilities"; ValueType: none
Root: HKLM; Subkey: "SOFTWARE\AegisubQT\Capabilities"; ValueType: string; ValueName: "ApplicationDescription"; ValueData: "@{app}\AegisubQT.exe,-10001"
Root: HKLM; Subkey: "SOFTWARE\AegisubQT\Capabilities\FileAssociations"; ValueType: none
Root: HKLM; Subkey: "SOFTWARE\AegisubQT\Capabilities\FileAssociations"; ValueType: string; ValueName: ".ass"; ValueData: "AegisubQT.ASSA.1"
Root: HKLM; Subkey: "SOFTWARE\AegisubQT\Capabilities\FileAssociations"; ValueType: string; ValueName: ".ssa"; ValueData: "AegisubQT.SSA.1"
Root: HKLM; Subkey: "SOFTWARE\AegisubQT\Capabilities\FileAssociations"; ValueType: string; ValueName: ".srt"; ValueData: "AegisubQT.SRT.1"
Root: HKLM; Subkey: "SOFTWARE\AegisubQT\Capabilities\FileAssociations"; ValueType: string; ValueName: ".ttxt"; ValueData: "AegisubQT.TTXT.1"
Root: HKLM; Subkey: "SOFTWARE\AegisubQT\Capabilities\FileAssociations"; ValueType: string; ValueName: ".mks"; ValueData: "AegisubQT.MKS.1"
Root: HKLM; Subkey: "SOFTWARE\RegisteredApplications"; ValueType: string; ValueName: "AegisubQT"; ValueData: "SOFTWARE\AegisubQT\Capabilities"; Flags: uninsdeletevalue
; Default handler for .ass (only claim ownership if the type has no owner yet)
Root: HKLM; SubKey: "SOFTWARE\Classes\.ass"; ValueType: string; ValueData: "AegisubQT.ASSA.1"; Flags: createvalueifdoesntexist
Root: HKLM; SubKey: "SOFTWARE\Classes\.ass"; ValueType: string; ValueName: "PerceivedType"; ValueData: "text"; Flags: createvalueifdoesntexist
Root: HKLM; Subkey: "SOFTWARE\Classes\.ass\AegisubQT.ASSA.1"; ValueType: none; Flags: uninsdeletekey
Root: HKLM; Subkey: "SOFTWARE\Classes\.ass\OpenWithProgids"; ValueType: string; ValueName: "AegisubQT.ASSA.1"; Flags: uninsdeletevalue
; Default handler for .ssa
Root: HKLM; SubKey: "SOFTWARE\Classes\.ssa"; ValueType: string; ValueData: "AegisubQT.SSA.1"; Flags: createvalueifdoesntexist
Root: HKLM; SubKey: "SOFTWARE\Classes\.ssa"; ValueType: string; ValueName: "PerceivedType"; ValueData: "text"; Flags: createvalueifdoesntexist
Root: HKLM; Subkey: "SOFTWARE\Classes\.ssa\OpenWithProgids"; ValueType: string; ValueName: "AegisubQT.SSA.1"; Flags: uninsdeletevalue
; Default handler for .srt
Root: HKLM; SubKey: "SOFTWARE\Classes\.srt"; ValueType: string; ValueData: "AegisubQT.SRT.1"; Flags: createvalueifdoesntexist
Root: HKLM; SubKey: "SOFTWARE\Classes\.srt"; ValueType: string; ValueName: "PerceivedType"; ValueData: "text"; Flags: createvalueifdoesntexist
Root: HKLM; Subkey: "SOFTWARE\Classes\.srt\OpenWithProgids"; ValueType: string; ValueName: "AegisubQT.SRT.1"; Flags: uninsdeletevalue
; Default handler for .ttxt
Root: HKLM; SubKey: "SOFTWARE\Classes\.ttxt"; ValueType: string; ValueData: "AegisubQT.TTXT.1"; Flags: createvalueifdoesntexist
Root: HKLM; SubKey: "SOFTWARE\Classes\.ttxt"; ValueType: string; ValueName: "PerceivedType"; ValueData: "text"; Flags: createvalueifdoesntexist
Root: HKLM; Subkey: "SOFTWARE\Classes\.ttxt\OpenWithProgids"; ValueType: string; ValueName: "AegisubQT.TTXT.1"; Flags: uninsdeletevalue
; Default handler for .mks
Root: HKLM; SubKey: "SOFTWARE\Classes\.mks"; ValueType: string; ValueData: "AegisubQT.MKS.1"; Flags: createvalueifdoesntexist
Root: HKLM; SubKey: "SOFTWARE\Classes\.mks"; ValueType: string; ValueName: "PerceivedType"; ValueData: "text"; Flags: createvalueifdoesntexist
Root: HKLM; Subkey: "SOFTWARE\Classes\.mks\OpenWithProgids"; ValueType: string; ValueName: "AegisubQT.MKS.1"; Flags: uninsdeletevalue
; Support opening a bunch more types
Root: HKLM; Subkey: "SOFTWARE\Classes\.sub\OpenWithProgids"; ValueType: string; ValueName: "AegisubQT.Subtitle.1"; Flags: uninsdeletevalue
Root: HKLM; Subkey: "SOFTWARE\Classes\.txt\OpenWithProgids"; ValueType: string; ValueName: "AegisubQT.TXT.1"; Flags: uninsdeletevalue
Root: HKLM; Subkey: "SOFTWARE\Classes\.mkv\OpenWithProgids"; ValueType: string; ValueName: "AegisubQT.Video.1"; Flags: uninsdeletevalue
Root: HKLM; Subkey: "SOFTWARE\Classes\.mka\OpenWithProgids"; ValueType: string; ValueName: "AegisubQT.Audio.1"; Flags: uninsdeletevalue
Root: HKLM; Subkey: "SOFTWARE\Classes\.avi\OpenWithProgids"; ValueType: string; ValueName: "AegisubQT.Video.1"; Flags: uninsdeletevalue
Root: HKLM; Subkey: "SOFTWARE\Classes\.mp3\OpenWithProgids"; ValueType: string; ValueName: "AegisubQT.Audio.1"; Flags: uninsdeletevalue
Root: HKLM; Subkey: "SOFTWARE\Classes\.mp4\OpenWithProgids"; ValueType: string; ValueName: "AegisubQT.Media.1"; Flags: uninsdeletevalue
Root: HKLM; Subkey: "SOFTWARE\Classes\.aac\OpenWithProgids"; ValueType: string; ValueName: "AegisubQT.Audio.1"; Flags: uninsdeletevalue
Root: HKLM; Subkey: "SOFTWARE\Classes\.m4a\OpenWithProgids"; ValueType: string; ValueName: "AegisubQT.Audio.1"; Flags: uninsdeletevalue
Root: HKLM; Subkey: "SOFTWARE\Classes\.wav\OpenWithProgids"; ValueType: string; ValueName: "AegisubQT.Audio.1"; Flags: uninsdeletevalue
Root: HKLM; Subkey: "SOFTWARE\Classes\.ogg\OpenWithProgids"; ValueType: string; ValueName: "AegisubQT.Media.1"; Flags: uninsdeletevalue
Root: HKLM; Subkey: "SOFTWARE\Classes\.avs\OpenWithProgids"; ValueType: string; ValueName: "AegisubQT.Video.1"; Flags: uninsdeletevalue
Root: HKLM; Subkey: "SOFTWARE\Classes\.opus\OpenWithProgids"; ValueType: string; ValueName: "AegisubQT.Audio.1"; Flags: uninsdeletevalue
Root: HKLM; Subkey: "SOFTWARE\Classes\.h264\OpenWithProgids"; ValueType: string; ValueName: "AegisubQT.Video.1"; Flags: uninsdeletevalue
Root: HKLM; Subkey: "SOFTWARE\Classes\.hevc\OpenWithProgids"; ValueType: string; ValueName: "AegisubQT.Video.1"; Flags: uninsdeletevalue
Root: HKLM; Subkey: "SOFTWARE\Classes\.eac3\OpenWithProgids"; ValueType: string; ValueName: "AegisubQT.Audio.1"; Flags: uninsdeletevalue
Root: HKLM; Subkey: "SOFTWARE\Classes\.webm\OpenWithProgids"; ValueType: string; ValueName: "AegisubQT.Media.1"; Flags: uninsdeletevalue

[Run]
Filename: "{app}\AegisubQT.exe"; Description: "{cm:LaunchProgram,AegisubQT}"; Flags: nowait postinstall skipifsilent
