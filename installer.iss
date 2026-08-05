; Inno Setup Script for OWMB (OpenWav Media Browser)
; Generates standalone Windows Installer (.exe)

#define MyAppName "OWMB"
#define MyAppFullTitle "OpenWav Media Browser"
#define MyAppVersion "1.0.0"
#define MyAppPublisher "OWMB"
#define MyAppURL "https://github.com/samplaman/owmb"
#define MyAppExeName "OWMB.exe"

[Setup]
AppId={{F92E3C41-8A5B-4D1E-9C3A-1234567890AB}
AppName={#MyAppFullTitle}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppPublisherURL={#MyAppURL}
AppSupportURL={#MyAppURL}
AppUpdatesURL={#MyAppURL}/releases
DefaultDirName={autopf}\{#MyAppName}
DefaultGroupName={#MyAppName}
DisableProgramGroupPage=yes
OutputDir=.
OutputBaseFilename=OWMB-Installer
Compression=lzma2/ultra64
SolidCompression=yes
WizardStyle=modern
ArchitecturesInstallIn64BitMode=x64
UninstallDisplayIcon={app}\{#MyAppExeName}

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[Files]
Source: "dist\OWMB-Windows-11-x64\OWMB.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "dist\OWMB-Windows-11-x64\OWMB.vst3\*"; DestDir: "{commoncf}\VST3\OWMB.vst3"; Flags: ignoreversion recursesubdirs createallsubdirs skipifsourcedoesntexist; Check: VST3DirExists

[Icons]
Name: "{group}\{#MyAppFullTitle}"; Filename: "{app}\{#MyAppExeName}"
Name: "{autodesktop}\{#MyAppFullTitle}"; Filename: "{app}\{#MyAppExeName}"; Tasks: desktopicon

[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "{cm:LaunchProgram,{#StringChange(MyAppFullTitle, '&', '&&')}}"; Flags: nowait postinstall skipifsilent

[Code]
function VST3DirExists: Boolean;
begin
  Result := DirExists(ExpandConstant('{commoncf}\VST3'));
end;
