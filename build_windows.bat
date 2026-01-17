@echo off
REM Script de compilation SATER Map pour Windows
REM Exécuter ce script dans le dossier du projet

echo ========================================
echo  SATER Map v2.0.0 - Compilation Windows
echo ========================================
echo.

REM Vérifier Python
python --version >nul 2>&1
if errorlevel 1 (
    echo ERREUR: Python n'est pas installe ou pas dans le PATH
    echo Telechargez Python depuis https://www.python.org/downloads/
    pause
    exit /b 1
)

echo [1/4] Installation des dependances...
pip install -r requirements.txt --quiet
pip install pyinstaller --quiet

echo [2/4] Nettoyage des anciennes compilations...
if exist "build" rmdir /s /q build
if exist "dist" rmdir /s /q dist
if exist "*.spec" del /q *.spec

echo [3/4] Compilation avec PyInstaller...
pyinstaller --onedir --windowed --name "SATER_Map" --icon=img/logo.ico --add-data "img;img" main.py

echo [4/4] Copie des fichiers supplementaires...
copy README.md dist\SATER_Map\ >nul 2>&1
copy SATER_Map_Manuel.pdf dist\SATER_Map\ >nul 2>&1

echo.
echo ========================================
echo  Compilation terminee !
echo ========================================
echo.
echo L'application se trouve dans: dist\SATER_Map\
echo Executable: dist\SATER_Map\SATER_Map.exe
echo.
pause
