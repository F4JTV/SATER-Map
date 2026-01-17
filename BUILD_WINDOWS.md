# Compilation SATER Map pour Windows

## Prérequis

1. **Python 3.10+** installé depuis [python.org](https://www.python.org/downloads/)
   - Cocher "Add Python to PATH" lors de l'installation

2. **Ouvrir PowerShell** ou Command Prompt

## Installation des dépendances

```powershell
# Installer les dépendances
pip install -r requirements.txt
pip install pyinstaller

# Vérifier que tout fonctionne
python main.py
```

## Compilation avec PyInstaller

### Méthode simple (un seul fichier .exe)

```powershell
pyinstaller --onefile --windowed --name "SATER_Map" --icon=img/logo.ico main.py
```

### Méthode recommandée (dossier avec dépendances)

```powershell
pyinstaller --onedir --windowed --name "SATER_Map" --icon=img/logo.ico --add-data "img;img" main.py
```

### Options expliquées :
- `--onefile` : Crée un seul fichier .exe (plus lent au démarrage)
- `--onedir` : Crée un dossier avec l'exe et les DLL (démarrage plus rapide)
- `--windowed` : Pas de console noire au démarrage
- `--icon` : Icône de l'application
- `--add-data` : Inclut le dossier img/

## Résultat

L'exécutable se trouve dans :
```
dist/SATER_Map/SATER_Map.exe
```

## Fichier .spec avancé (optionnel)

Pour plus de contrôle, créez `SATER_Map.spec` :

```python
# -*- mode: python ; coding: utf-8 -*-

a = Analysis(
    ['main.py'],
    pathex=[],
    binaries=[],
    datas=[('img', 'img')],
    hiddenimports=['PyQt6.QtWebEngineWidgets', 'PyQt6.QtWebEngineCore'],
    hookspath=[],
    hooksconfig={},
    runtime_hooks=[],
    excludes=[],
    noarchive=False,
)

pyz = PYZ(a.pure)

exe = EXE(
    pyz,
    a.scripts,
    [],
    exclude_binaries=True,
    name='SATER_Map',
    debug=False,
    bootloader_ignore_signals=False,
    strip=False,
    upx=True,
    console=False,
    disable_windowed_traceback=False,
    argv_emulation=False,
    target_arch=None,
    codesign_identity=None,
    entitlements_file=None,
    icon='img/logo.ico',
)

coll = COLLECT(
    exe,
    a.binaries,
    a.datas,
    strip=False,
    upx=True,
    upx_exclude=[],
    name='SATER_Map',
)
```

Puis compiler avec :
```powershell
pyinstaller SATER_Map.spec
```

## Distribution

Le dossier `dist/SATER_Map/` contient tout le nécessaire.
Vous pouvez :
1. Le zipper et le distribuer
2. Créer un installateur avec NSIS ou Inno Setup

## Création d'un installateur (optionnel)

### Avec Inno Setup

1. Télécharger [Inno Setup](https://jrsoftware.org/isinfo.php)
2. Créer un script `installer.iss` :

```iss
[Setup]
AppName=SATER Map
AppVersion=2.0.0
DefaultDirName={autopf}\SATER_Map
DefaultGroupName=SATER Map
OutputBaseFilename=SATER_Map_Setup
Compression=lzma
SolidCompression=yes

[Files]
Source: "dist\SATER_Map\*"; DestDir: "{app}"; Flags: recursesubdirs

[Icons]
Name: "{group}\SATER Map"; Filename: "{app}\SATER_Map.exe"
Name: "{commondesktop}\SATER Map"; Filename: "{app}\SATER_Map.exe"
```

3. Compiler avec Inno Setup Compiler

## Dépannage

### Erreur "QtWebEngine not found"
```powershell
pip uninstall PyQt6-WebEngine PyQt6
pip install PyQt6 PyQt6-WebEngine --force-reinstall
```

### Erreur DLL manquante
Utiliser `--onedir` au lieu de `--onefile`

### L'exe ne démarre pas
Lancer depuis PowerShell pour voir les erreurs :
```powershell
.\dist\SATER_Map\SATER_Map.exe
```

## Taille approximative

- Mode `--onedir` : ~150-200 Mo (dossier complet)
- Mode `--onefile` : ~80-100 Mo (exe unique)
- Installateur : ~50-70 Mo (compressé)
