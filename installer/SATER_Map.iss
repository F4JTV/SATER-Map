;  ============================================================================
;  SATER Map - Inno Setup script (bilingual English / French)
;
;  Saved as UTF-8 with a byte-order mark: without it Inno Setup may read the
;  file in the system code page, and the French accents come out garbled.
;
;  Built by build_all.bat, which stages everything in dist\ first:
;      ISCC.exe /DAppVersion=x.y.z SATER_Map.iss
;
;  The version is passed by build_all.bat (/DAppVersion=x.y.z), which reads
;  it from CMakeLists.txt. The value below only serves when this file is
;  compiled by hand, and is then wrong: go through build_all.bat.
;  ============================================================================

#ifndef AppVersion
  #define AppVersion "0.0.0"
#endif
#define AppName "SATER Map"
#define AppPublisher "SATER Map contributors"
#define AppExeName "SATER_Map.exe"

[Setup]
; The AppId and folder of SATER Map 2 (Python): this installer upgrades it
; in place - same folder, same Start menu entry, and the tiles it
; downloaded into {app}\tiles stay where the new program looks for them.
AppId={{A1B2C3D4-E5F6-7890-ABCD-EF1234567890}
AppName={#AppName}
AppVersion={#AppVersion}
AppVerName={#AppName} {#AppVersion}
AppPublisher={#AppPublisher}
VersionInfoVersion={#AppVersion}
VersionInfoProductName={#AppName}
VersionInfoDescription=SATER Map - direction finding for SATER missions
DefaultDirName={autopf}\SATER_Map
DefaultGroupName={#AppName}
DisableProgramGroupPage=yes
UninstallDisplayIcon={app}\{#AppExeName}
OutputDir=output
OutputBaseFilename=SATER_Map-{#AppVersion}-setup
SetupIconFile=..\assets\logo.ico
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
; Program Files, and a tiles folder all users may write in.
PrivilegesRequired=admin
LicenseFile=..\LICENSE.txt
MinVersion=10.0

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"
Name: "french";  MessagesFile: "compiler:Languages\French.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: checkedonce

[Files]
; Everything windeployqt and build_all.bat gathered: the program, the Qt
; runtime and its QML modules.
Source: "dist\*"; DestDir: "{app}"; Excludes: "vc_redist.x64.exe"; Flags: ignoreversion recursesubdirs createallsubdirs
; The Visual C++ runtime, unpacked to a temporary folder, run, then deleted.
#ifexist "dist\vc_redist.x64.exe"
Source: "dist\vc_redist.x64.exe"; DestDir: "{tmp}"; Flags: deleteafterinstall
#endif

[Dirs]
; The downloaded map tiles, next to the program as with SATER Map 2,
; writable by every user.
Name: "{app}\tiles"; Permissions: users-modify

[InstallDelete]
; What SATER Map 2 (PyInstaller) left in the same folder: its Python
; runtime and its copy of the images.
Type: filesandordirs; Name: "{app}\_internal"
Type: filesandordirs; Name: "{app}\img"

[Icons]
Name: "{group}\{#AppName}"; Filename: "{app}\{#AppExeName}"; WorkingDir: "{app}"
Name: "{group}\{cm:UninstallProgram,{#AppName}}"; Filename: "{uninstallexe}"
Name: "{autodesktop}\{#AppName}"; Filename: "{app}\{#AppExeName}"; WorkingDir: "{app}"; Tasks: desktopicon

[Run]
; A runtime already present, of the same or a newer version, makes it return
; at once with a non-zero code, which Inno Setup ignores.
#ifexist "dist\vc_redist.x64.exe"
Filename: "{tmp}\vc_redist.x64.exe"; Parameters: "/install /quiet /norestart"; \
    StatusMsg: "{cm:InstallingRuntime}"; Flags: waituntilterminated
#endif
Filename: "{app}\{#AppExeName}"; Description: "{cm:LaunchProgram,{#AppName}}"; Flags: nowait postinstall skipifsilent

[CustomMessages]
english.InstallingRuntime=Installing the Microsoft Visual C++ runtime...
french.InstallingRuntime=Installation du runtime Microsoft Visual C++...
english.DeleteData=Also delete your data?%n%n- the station presets and the current mission (%1)%n- the downloaded map tiles (%2)%n- the settings%n%nAnswer No to keep them for a later installation.
french.DeleteData=Supprimer aussi vos données ?%n%n- les présets de stations et la mission en cours (%1)%n- les tuiles cartographiques téléchargées (%2)%n- les réglages%n%nRépondez Non pour les conserver pour une installation ultérieure.

[Code]
// The user's data: the presets, the saved mission and the tiles stored
// outside the program folder live in %LOCALAPPDATA%\SATER_Map (the folder
// of SATER Map 2), the settings in the registry, the shared tiles in
// the tiles folder next to the program.  The uninstaller asks whether to
// delete them; a silent uninstall keeps them.

var
  DeleteUserData: Boolean;

function InitializeUninstall(): Boolean;
var
  DataPath: String;
  TilesPath: String;
begin
  Result := True;
  DeleteUserData := False;
  DataPath := ExpandConstant('{localappdata}\SATER_Map');
  TilesPath := ExpandConstant('{app}\tiles');
  if (not UninstallSilent()) and (DirExists(DataPath) or DirExists(TilesPath)) then
    DeleteUserData := MsgBox(FmtMessage(CustomMessage('DeleteData'), [DataPath, TilesPath]),
                             mbConfirmation, MB_YESNO or MB_DEFBUTTON2) = IDYES;
end;

procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
begin
  if (CurUninstallStep = usPostUninstall) and DeleteUserData then
  begin
    DelTree(ExpandConstant('{localappdata}\SATER_Map'), True, True, True);
    DelTree(ExpandConstant('{app}\tiles'), True, True, True);
    RegDeleteKeyIncludingSubkeys(HKEY_CURRENT_USER, 'Software\SATER_Map');
    RemoveDir(ExpandConstant('{app}'));
  end;
end;
