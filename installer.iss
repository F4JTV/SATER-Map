[Setup]
AppName=SATER Map
AppVersion=2.0.0
DefaultDirName={autopf}\SATER_Map
DefaultGroupName=SATER Map
OutputBaseFilename=SATER_Map_Setup
Compression=lzma
SolidCompression=yes
SignTool=ADRASEC06 $f

[Files]
Source: "dist\SATER_Map\*"; DestDir: "{app}"; Flags: recursesubdirs

[Icons]
Name: "{group}\SATER Map"; Filename: "{app}\SATER_Map.exe"
Name: "{commondesktop}\SATER Map"; Filename: "{app}\SATER_Map.exe"