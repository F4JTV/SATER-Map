; SATER Map - Script d'installation Inno Setup
; Version 2.0.0

#define MyAppName "SATER Map"
#define MyAppVersion "2.0.0"
#define MyAppPublisher "ADRASEC"
#define MyAppExeName "SATER_Map.exe"

[Setup]
AppId={{A1B2C3D4-E5F6-7890-ABCD-EF1234567890}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
DefaultDirName={autopf}\SATER_Map
DefaultGroupName={#MyAppName}
OutputBaseFilename=SATER_Map_v{#MyAppVersion}_Setup
Compression=lzma2
SolidCompression=yes
; Demander les droits admin pour installer dans Program Files
PrivilegesRequired=admin
; Permettre à l'utilisateur de choisir le dossier
DisableDirPage=no
; Architecture
ArchitecturesInstallIn64BitMode=x64compatible
; Icône de l'installateur (optionnel - décommenter si vous avez l'icône)
SetupIconFile=dist\SATER_Map\_internal\img\logo.ico
; Informations affichées
AppContact=ADRASEC06
AppSupportURL=https://www.adrasec06.fr/
AppUpdatesURL=https://www.adrasec06.fr/
; Version Windows minimale (Windows 10)
MinVersion=10.0

[Languages]
Name: "french"; MessagesFile: "compiler:Languages\French.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: checkedonce

[Files]
; Copier tous les fichiers de l'application
Source: "dist\SATER_Map\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs
; Le dossier img est inclus via recursesubdirs

[Dirs]
; Créer le dossier tiles avec permissions d'écriture pour les utilisateurs
Name: "{app}\tiles"; Permissions: users-modify

[Icons]
; Raccourci dans le menu Démarrer
Name: "{group}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"
Name: "{group}\Désinstaller {#MyAppName}"; Filename: "{uninstallexe}"
; Raccourci sur le bureau (si coché)
Name: "{commondesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Tasks: desktopicon

[Run]
; Lancer l'application après l'installation (optionnel)
Filename: "{app}\{#MyAppExeName}"; Description: "{cm:LaunchProgram,{#StringChange(MyAppName, '&', '&&')}}"; Flags: nowait postinstall skipifsilent

[UninstallDelete]
; Supprimer le dossier tiles et tout son contenu
Type: filesandordirs; Name: "{app}\tiles"
; Supprimer les fichiers de configuration dans le dossier de l'application
Type: files; Name: "{app}\*.json"
Type: files; Name: "{app}\*.log"
; Supprimer le dossier des données utilisateur dans AppData\Local
Type: filesandordirs; Name: "{localappdata}\SATER_Map"

[Code]
// Code Pascal pour des actions personnalisées

procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
var
  AppDataPath: String;
  TilesPath: String;
begin
  if CurUninstallStep = usPostUninstall then
  begin
    // Chemin du dossier AppData\Local\SATER_Map
    AppDataPath := ExpandConstant('{localappdata}\SATER_Map');
    
    // Supprimer le dossier des données utilisateur s'il existe
    if DirExists(AppDataPath) then
    begin
      if not DelTree(AppDataPath, True, True, True) then
      begin
        // Log si la suppression échoue (pas bloquant)
        Log('Impossible de supprimer le dossier: ' + AppDataPath);
      end;
    end;
    
    // S'assurer que le dossier tiles est supprimé
    TilesPath := ExpandConstant('{app}\tiles');
    if DirExists(TilesPath) then
    begin
      DelTree(TilesPath, True, True, True);
    end;
    
    // Supprimer le dossier principal de l'application s'il est vide
    RemoveDir(ExpandConstant('{app}'));
  end;
end;

// Demander confirmation avant de supprimer les données utilisateur
function InitializeUninstall(): Boolean;
var
  AppDataPath: String;
  TilesPath: String;
  Msg: String;
  HasUserData: Boolean;
  HasTiles: Boolean;
begin
  Result := True;
  
  AppDataPath := ExpandConstant('{localappdata}\SATER_Map');
  TilesPath := ExpandConstant('{app}\tiles');
  
  HasUserData := DirExists(AppDataPath);
  HasTiles := DirExists(TilesPath);
  
  if HasUserData or HasTiles then
  begin
    Msg := 'La désinstallation va également supprimer :' + #13#10 + #13#10;
    
    if HasUserData then
      Msg := Msg + '• Vos présets de stations (dans ' + AppDataPath + ')' + #13#10;
    
    if HasTiles then
      Msg := Msg + '• Les tuiles cartographiques téléchargées (dans ' + TilesPath + ')' + #13#10;
    
    Msg := Msg + #13#10 + 'Voulez-vous continuer ?';
    
    Result := MsgBox(Msg, mbConfirmation, MB_YESNO) = IDYES;
  end;
end;
