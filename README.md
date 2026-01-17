# SATER Map v2.0.0

Application de radiogoniométrie pour les opérations SATER (Sauvetage Aéro-Terrestre).

## Fonctionnalités

- 📍 Visualisation des azimuts sur carte interactive
- 🎯 Calcul automatique de la zone d'intersection
- 📐 Affichage de la surface de recherche en km²
- 🚨 Positionnement de la balise de détresse (manuel ou calculé)
- 🖱️ Drag & drop des stations et de la balise sur la carte
- 🗺️ 8 fonds de carte (OSM, IGN, Satellite...)
- 📴 Mode hors-ligne avec tuiles téléchargeables
- 💾 Export HTML, PNG, KML, JSON
- 📊 Rapport PDF de mission avec capture de carte
- ⏱️ Chronomètre de mission intégré
- 📜 Historique des relevés avec horodatage
- 🚗 Suivi des kilomètres parcourus

## Installation

### Prérequis
- Python 3.8 ou supérieur
- Connexion Internet (pour les fonds de carte en ligne)

```bash
pip install -r requirements.txt
python main.py
```

## Compilation Windows (.exe)

Voir `BUILD_WINDOWS.md` pour les instructions détaillées.

**Méthode rapide :**
```powershell
.\build_windows.bat
```

## Documentation

- `SATER_Map_Manuel.pdf` - Manuel d'utilisation complet

## Structure des fichiers

```
SATER_Map/
├── main.py                     # Application principale
├── requirements.txt            # Dépendances Python
├── README.md                   # Ce fichier
├── BUILD_WINDOWS.md            # Guide de compilation Windows
├── build_windows.bat           # Script de compilation automatique
├── generate_manual.py          # Script de génération du manuel
├── SATER_Map_Manuel.pdf        # Manuel utilisateur
├── img/
│   ├── logo.jpg                # Logo ADRASEC
│   └── logo.ico                # Icône Windows
└── tiles/                      # Tuiles cartographiques (créé automatiquement)
    ├── osm/
    ├── osm-fr/
    └── ...
```

## Utilisation

1. Lancer l'application (la carte s'ouvre centrée sur la France)
2. Cliquer sur "+ Ajouter une station"
3. Entrer l'indicatif, les coordonnées et l'azimut
4. Cliquer sur 📝 pour enregistrer le relevé
5. Répéter pour chaque station
6. La zone d'intersection s'affiche automatiquement
7. Optionnel : définir la position de la balise via Édition > Définir position balise

## Mode hors-ligne

1. Naviguer vers la zone d'opération
2. Menu "Édition" → "Télécharger les tuiles visibles"
3. Cocher "Mode hors-ligne" dans les paramètres

## Licence

Développé pour les ADRASEC - Licence libre
