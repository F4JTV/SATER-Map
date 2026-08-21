# SATER Map - Guide de compilation Windows

**Éditeur:** F4JTV  
**Version:** 2.0.0

## Prérequis

- Python 3.10+ (64-bit recommandé)
- PyInstaller : `pip install pyinstaller`
- Inno Setup 6 (pour créer l'installateur) : https://jrsoftware.org/isinfo.php

## Compilation rapide

Double-cliquez sur `build_windows.bat` pour compiler automatiquement l'application.

## Compilation manuelle

### 1. Installer les dépendances

```powershell
pip install pyinstaller PyQt6 PyQt6-WebEngine reportlab
```

### 2. Compiler avec PyInstaller

```powershell
pyinstaller --name "SATER_Map" --windowed --onedir --icon "img\logo.ico" --version-file "version.txt" --add-data "img;img" --noconfirm main.py
```

**Important:** Le fichier `version.txt` contient les métadonnées de version (éditeur F4JTV, description, copyright). Ces métadonnées permettent d'éviter le blocage par Windows SmartScreen.

### 3. Créer l'installateur (optionnel)

Ouvrez `installer.iss` avec Inno Setup et compilez, ou en ligne de commande :

```powershell
"C:\Program Files (x86)\Inno Setup 6\ISCC.exe" installer.iss
```

L'installateur sera créé dans le dossier `Output\`.

## Structure des fichiers

```
SATER_Map/
├── main.py              # Code source principal
├── version.txt          # Métadonnées de version (F4JTV)
├── installer.iss        # Script Inno Setup
├── build_windows.bat    # Script de compilation automatique
├── img/
│   ├── logo.jpg         # Logo de l'application
│   └── logo.ico         # Icône Windows (multi-résolution)
└── dist/
    └── SATER_Map/       # Application compilée
        ├── SATER_Map.exe
        ├── img/
        └── ...
```

## Métadonnées de version

Le fichier `version.txt` définit les informations suivantes :
- **Éditeur (CompanyName):** F4JTV
- **Description:** SATER Map - Outil de radiogoniométrie pour missions SATER
- **Copyright:** © 2024-2025 F4JTV - ADRASEC
- **Version:** 2.0.0.0

Ces métadonnées sont visibles dans les propriétés de l'exécutable Windows (clic droit > Propriétés > Détails).

## Résolution des problèmes

### Windows SmartScreen bloque l'application

Si Windows SmartScreen affiche "Windows a protégé votre ordinateur" :

1. **Solution 1 (utilisateur):** Cliquer sur "Informations complémentaires" puis "Exécuter quand même"

2. **Solution 2 (développeur):** S'assurer que le fichier `version.txt` est utilisé lors de la compilation

3. **Solution 3 (signature):** Signer numériquement l'exécutable avec un certificat de signature de code (payant)

### L'icône ne s'affiche pas

Vérifiez que :
- Le fichier `img/logo.ico` existe et est un fichier ICO valide
- L'option `--icon "img\logo.ico"` est présente dans la commande PyInstaller
- L'option `--add-data "img;img"` est présente pour inclure le dossier img

### Les tuiles ne se téléchargent pas

Voir la section correspondante dans le README.md pour les problèmes de téléchargement de tuiles (SSL, permissions, etc.).

### Les présets ne se sauvegardent pas

Les présets sont stockés dans `%LOCALAPPDATA%\SATER_Map\station_presets.json`. 
Vérifiez que ce dossier est accessible en écriture.

## Distribution

Pour distribuer l'application :

1. **Sans installateur:** Compressez le dossier `dist\SATER_Map\` en ZIP
2. **Avec installateur:** Utilisez le fichier `SATER_Map_v2.0.0_Setup.exe` généré par Inno Setup

L'installateur gère automatiquement :
- Création des raccourcis (menu Démarrer, Bureau)
- Permissions d'écriture pour le dossier `tiles`
- Désinstallation propre (suppression des données utilisateur optionnelle)
