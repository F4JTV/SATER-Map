@echo off
REM ============================================
REM SATER Map - Script de compilation Windows
REM Editeur: F4JTV
REM ============================================

echo.
echo ========================================
echo   SATER Map - Compilation Windows
echo   Version 2.0.0 - Editeur: F4JTV
echo ========================================
echo.

REM Verifier que Python est installe
python --version >nul 2>&1
if errorlevel 1 (
    echo [ERREUR] Python n'est pas installe ou n'est pas dans le PATH
    pause
    exit /b 1
)

REM Verifier que PyInstaller est installe
pip show pyinstaller >nul 2>&1
if errorlevel 1 (
    echo [INFO] Installation de PyInstaller...
    pip install pyinstaller
)

REM Nettoyer les anciens builds
echo [INFO] Nettoyage des anciens builds...
if exist "dist" rmdir /s /q dist
if exist "build" rmdir /s /q build
if exist "*.spec" del /q *.spec

REM Compilation avec PyInstaller
echo.
echo [INFO] Compilation avec PyInstaller...
echo [INFO] Inclusion de QtWebEngine (peut prendre quelques minutes)...
echo.

python -m PyInstaller ^
    --name "SATER_Map" ^
    --windowed ^
    --onedir ^
    --icon "img\logo.ico" ^
    --version-file "version.txt" ^
    --add-data "img;img" ^
    --collect-all PyQt6 ^
    --collect-all PyQt6.QtWebEngineWidgets ^
    --collect-all PyQt6.QtWebEngineCore ^
    --hidden-import PyQt6.QtWebEngineWidgets ^
    --hidden-import PyQt6.QtWebEngineCore ^
    --hidden-import PyQt6.QtWebEngine ^
    --hidden-import PyQt6.QtPrintSupport ^
    --noconfirm ^
    main.py

if errorlevel 1 (
    echo.
    echo [ERREUR] La compilation a echoue!
    pause
    exit /b 1
)

echo.
echo [INFO] Compilation terminee avec succes!
echo.
echo [INFO] L'executable se trouve dans: dist\SATER_Map\SATER_Map.exe
echo.

REM Verifier si Inno Setup est installe (version 6 ou 7)
set INNO_PATH=
if exist "%ProgramFiles(x86)%\Inno Setup 6\ISCC.exe" set INNO_PATH=%ProgramFiles(x86)%\Inno Setup 6\ISCC.exe
if exist "%ProgramFiles%\Inno Setup 7\ISCC.exe" set INNO_PATH=%ProgramFiles%\Inno Setup 7\ISCC.exe
if exist "%ProgramFiles(x86)%\Inno Setup 7\ISCC.exe" set INNO_PATH=%ProgramFiles(x86)%\Inno Setup 7\ISCC.exe

if defined INNO_PATH (
    echo [INFO] Inno Setup detecte: %INNO_PATH%
    set /p CREATE_INSTALLER="Creer l'installateur? (O/N): "
    if /i "%CREATE_INSTALLER%"=="O" (
        echo.
        echo [INFO] Creation de l'installateur...
        "%INNO_PATH%" installer.iss
        if errorlevel 1 (
            echo [ERREUR] La creation de l'installateur a echoue!
        ) else (
            echo.
            echo [INFO] Installateur cree avec succes!
            echo [INFO] Fichier: Output\SATER_Map_v2.0.0_Setup.exe
        )
    )
) else (
    echo [INFO] Inno Setup non detecte. Pour creer un installateur:
    echo        1. Installez Inno Setup depuis https://jrsoftware.org/isinfo.php
    echo        2. Ouvrez installer.iss avec Inno Setup
    echo        3. Compilez le script
)

echo.
pause
