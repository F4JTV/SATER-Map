#!/usr/bin/env python3
"""Génère le manuel utilisateur PDF pour SATER Map v2.0.0"""

import os
from reportlab.lib import colors
from reportlab.lib.pagesizes import A4
from reportlab.lib.styles import getSampleStyleSheet, ParagraphStyle
from reportlab.lib.units import mm, cm
from reportlab.platypus import SimpleDocTemplate, Paragraph, Spacer, Table, TableStyle, Image, PageBreak
from reportlab.lib.enums import TA_CENTER, TA_LEFT, TA_JUSTIFY

def generate_manual(output_path, img_dir="./img"):
    doc = SimpleDocTemplate(output_path, pagesize=A4,
                           rightMargin=2*cm, leftMargin=2*cm,
                           topMargin=1.5*cm, bottomMargin=1.5*cm,
                           title="SATER Map - Manuel Utilisateur",
                           author="SATER Map",
                           subject="Manuel d'utilisation de l'application SATER Map",
                           keywords="SATER, ADRASEC, radiogoniométrie, balise, détresse",
                           creator="SATER Map")
    
    styles = getSampleStyleSheet()
    
    # Styles personnalisés
    title_style = ParagraphStyle(
        'TitleStyle',
        parent=styles['Heading1'],
        fontSize=28,
        alignment=TA_CENTER,
        spaceAfter=10,
        textColor=colors.HexColor('#2c3e50')
    )
    
    subtitle_style = ParagraphStyle(
        'SubtitleStyle',
        parent=styles['Normal'],
        fontSize=14,
        alignment=TA_CENTER,
        spaceAfter=20,
        textColor=colors.HexColor('#7f8c8d')
    )
    
    section_style = ParagraphStyle(
        'SectionStyle',
        parent=styles['Heading1'],
        fontSize=16,
        spaceBefore=20,
        spaceAfter=10,
        textColor=colors.HexColor('#e65c00')
    )
    
    subsection_style = ParagraphStyle(
        'SubsectionStyle',
        parent=styles['Heading2'],
        fontSize=13,
        spaceBefore=15,
        spaceAfter=8,
        textColor=colors.HexColor('#003d80')
    )
    
    normal_style = ParagraphStyle(
        'NormalStyle',
        parent=styles['Normal'],
        fontSize=10,
        spaceAfter=8,
        alignment=TA_JUSTIFY
    )
    
    bullet_style = ParagraphStyle(
        'BulletStyle',
        parent=styles['Normal'],
        fontSize=10,
        leftIndent=20,
        spaceAfter=4
    )
    
    code_style = ParagraphStyle(
        'CodeStyle',
        parent=styles['Normal'],
        fontSize=9,
        fontName='Courier',
        backColor=colors.HexColor('#f5f5f5'),
        leftIndent=10,
        rightIndent=10,
        spaceBefore=5,
        spaceAfter=5
    )
    
    elements = []
    
    # === PAGE DE TITRE ===
    logo_path = os.path.join(img_dir, "logo.jpg")
    
    if os.path.exists(logo_path):
        logo = Image(logo_path, width=5*cm, height=5*cm)
        logo.hAlign = 'CENTER'
        elements.append(logo)
        elements.append(Spacer(1, 20))
    
    # Ligne de séparation orange/bleue
    line_data = [['', '']]
    line_table = Table(line_data, colWidths=[8.5*cm, 8.5*cm])
    line_table.setStyle(TableStyle([
        ('BACKGROUND', (0, 0), (0, 0), colors.HexColor('#e65c00')),
        ('BACKGROUND', (1, 0), (1, 0), colors.HexColor('#003d80')),
        ('LINEABOVE', (0, 0), (-1, 0), 4, colors.HexColor('#e65c00')),
    ]))
    elements.append(line_table)
    elements.append(Spacer(1, 25))
    
    elements.append(Paragraph("SATER Map", title_style))
    elements.append(Paragraph("Manuel Utilisateur - Version 2.0.0", subtitle_style))
    elements.append(Spacer(1, 30))
    
    elements.append(Paragraph(
        "Application de radiogoniométrie pour la localisation<br/>"
        "de balises de détresse lors des missions SATER",
        normal_style
    ))
    elements.append(Spacer(1, 40))
    
    # Table des matières
    toc_data = [
        ["Table des matières", "Page"],
        ["1. Introduction", "2"],
        ["2. Installation", "2"],
        ["3. Interface utilisateur", "3"],
        ["4. Gestion des stations", "4"],
        ["5. Balise de détresse", "5"],
        ["6. Présets de stations", "5"],
        ["7. Carte et navigation", "6"],
        ["8. Zone d'intersection", "7"],
        ["9. Historique des relevés", "8"],
        ["10. Exports et rapports", "8"],
        ["11. Mode hors-ligne", "9"],
        ["12. Raccourcis clavier", "10"],
    ]
    
    toc_table = Table(toc_data, colWidths=[12*cm, 2*cm])
    toc_table.setStyle(TableStyle([
        ('BACKGROUND', (0, 0), (-1, 0), colors.HexColor('#e65c00')),
        ('TEXTCOLOR', (0, 0), (-1, 0), colors.white),
        ('FONTNAME', (0, 0), (-1, 0), 'Helvetica-Bold'),
        ('FONTSIZE', (0, 0), (-1, -1), 10),
        ('PADDING', (0, 0), (-1, -1), 8),
        ('GRID', (0, 0), (-1, -1), 0.5, colors.HexColor('#bdc3c7')),
        ('ALIGN', (1, 0), (1, -1), 'CENTER'),
    ]))
    elements.append(toc_table)
    
    elements.append(PageBreak())
    
    # === 1. INTRODUCTION ===
    elements.append(Paragraph("1. Introduction", section_style))
    elements.append(Paragraph(
        "SATER Map est une application de radiogoniométrie développée pour les ADRASEC "
        "(Association Départementale des Radioamateurs au Service de la Sécurité Civile). "
        "Elle permet de localiser des balises de détresse lors des missions SATER (Sauvetage Aéroterrestre) "
        "en calculant l'intersection des azimuts relevés par plusieurs stations.",
        normal_style
    ))
    
    elements.append(Paragraph("Fonctionnalités principales :", subsection_style))
    features = [
        "Positionnement de stations de radiogoniométrie sur une carte interactive",
        "Saisie des azimuts avec incertitude angulaire (cônes d'incertitude)",
        "Calcul automatique de la zone d'intersection probable",
        "Positionnement de la balise de détresse (manuelle ou calculée)",
        "Report du signal S-mètre (S0 à S9+30) pour chaque relevé",
        "Affichage des coordonnées en DD (degrés décimaux) et DMS (degrés/minutes/secondes)",
        "Grille UTM/MGRS superposable avec couleurs personnalisables",
        "Historique des relevés d'azimut avec horodatage et enregistrement manuel",
        "Suivi des kilomètres parcourus par station",
        "Présets de stations pour les positions habituelles",
        "Génération de rapport PDF de mission avec capture de carte",
        "Export HTML autonome, KML et JSON",
        "Mode hors-ligne avec tuiles téléchargées",
        "Chronomètre de mission intégré",
    ]
    for f in features:
        elements.append(Paragraph(f"• {f}", bullet_style))
    
    elements.append(Spacer(1, 10))
    
    # === 2. INSTALLATION ===
    elements.append(Paragraph("2. Installation", section_style))
    
    elements.append(Paragraph("Prérequis :", subsection_style))
    elements.append(Paragraph("• Python 3.8 ou supérieur", bullet_style))
    elements.append(Paragraph("• PyQt6 et PyQt6-WebEngine", bullet_style))
    elements.append(Paragraph("• reportlab (pour la génération de PDF)", bullet_style))
    elements.append(Paragraph("• Pillow (pour le traitement des images)", bullet_style))
    
    elements.append(Paragraph("Installation des dépendances :", subsection_style))
    elements.append(Paragraph("pip install -r requirements.txt", code_style))
    
    elements.append(Paragraph("Lancement de l'application :", subsection_style))
    elements.append(Paragraph("python main.py", code_style))
    
    elements.append(Paragraph(
        "L'application peut également être compilée en exécutable Windows avec PyInstaller. "
        "Consultez le fichier BUILD_WINDOWS.md pour les instructions détaillées.",
        normal_style
    ))
    
    elements.append(PageBreak())
    
    # === 3. INTERFACE UTILISATEUR ===
    elements.append(Paragraph("3. Interface utilisateur", section_style))
    
    elements.append(Paragraph(
        "L'interface est divisée en deux parties principales : le panneau de contrôle à gauche "
        "et la carte interactive à droite. Au démarrage, la carte est centrée sur la France.",
        normal_style
    ))
    
    elements.append(Paragraph("Panneau de contrôle", subsection_style))
    elements.append(Paragraph("Le panneau de contrôle contient :", normal_style))
    elements.append(Paragraph("• <b>Chronomètre Mission</b> : chronométrage de la durée de mission", bullet_style))
    elements.append(Paragraph("• <b>Paramètres de la carte</b> : sélection du fond de carte, mode hors-ligne, grille UTM/MGRS", bullet_style))
    elements.append(Paragraph("• <b>Longueur d'azimut</b> : distance de projection des lignes d'azimut (en km)", bullet_style))
    elements.append(Paragraph("• <b>Zone d'intersection</b> : personnalisation des couleurs et opacité", bullet_style))
    elements.append(Paragraph("• <b>Grille UTM/MGRS</b> : affichage et couleur de la grille", bullet_style))
    elements.append(Paragraph("• <b>Tableau des stations</b> : gestion des stations de radiogoniométrie", bullet_style))
    elements.append(Paragraph("• <b>Informations</b> : affichage de la zone d'intersection et accès à l'historique", bullet_style))
    
    elements.append(Paragraph("Carte interactive", subsection_style))
    elements.append(Paragraph(
        "La carte Leaflet permet de visualiser les stations, les azimuts, la zone d'intersection "
        "et la position de la balise de détresse. Elle supporte le glisser-déposer des stations "
        "et de la balise, et affiche les coordonnées au clic droit.",
        normal_style
    ))
    
    # === 4. GESTION DES STATIONS ===
    elements.append(Paragraph("4. Gestion des stations", section_style))
    
    elements.append(Paragraph("Ajouter une station", subsection_style))
    elements.append(Paragraph(
        "Cliquez sur le bouton '+ Ajouter station' pour créer une nouvelle station. "
        "Chaque station possède les champs suivants :",
        normal_style
    ))
    
    station_fields = [
        ["Champ", "Description"],
        ["Indicatif", "Indicatif radio de la station (ex: STATION1/P)"],
        ["Latitude", "Coordonnées en degrés/minutes/secondes (DMS)"],
        ["Longitude", "Coordonnées en degrés/minutes/secondes (DMS)"],
        ["Signal", "Report S-mètre (S0 à S9+30)"],
        ["Azimut", "Direction du signal reçu (0-359°)"],
        ["Incertitude", "Marge d'erreur angulaire (±0-30°)"],
        ["Couleur", "Couleur d'affichage sur la carte"],
        ["Visible", "Afficher/masquer la station"],
        ["📝", "Enregistrer le relevé dans l'historique"],
        ["🗑", "Supprimer la station"],
    ]
    
    t = Table(station_fields, colWidths=[3*cm, 10*cm])
    t.setStyle(TableStyle([
        ('BACKGROUND', (0, 0), (-1, 0), colors.HexColor('#003d80')),
        ('TEXTCOLOR', (0, 0), (-1, 0), colors.white),
        ('FONTNAME', (0, 0), (-1, 0), 'Helvetica-Bold'),
        ('FONTSIZE', (0, 0), (-1, -1), 9),
        ('PADDING', (0, 0), (-1, -1), 6),
        ('GRID', (0, 0), (-1, -1), 0.5, colors.HexColor('#bdc3c7')),
        ('VALIGN', (0, 0), (-1, -1), 'MIDDLE'),
    ]))
    elements.append(t)
    elements.append(Spacer(1, 10))
    
    elements.append(Paragraph("Déplacer une station", subsection_style))
    elements.append(Paragraph(
        "Les stations peuvent être déplacées directement sur la carte par glisser-déposer. "
        "Les coordonnées sont automatiquement mises à jour dans le tableau.",
        normal_style
    ))
    
    elements.append(Paragraph("Enregistrement des relevés", subsection_style))
    elements.append(Paragraph(
        "Cliquez sur le bouton <b>📝</b> de la ligne de station pour enregistrer le relevé "
        "dans l'historique. Cela permet un contrôle précis des données enregistrées.",
        normal_style
    ))
    
    elements.append(PageBreak())
    
    # === 5. BALISE DE DÉTRESSE ===
    elements.append(Paragraph("5. Balise de détresse", section_style))
    
    elements.append(Paragraph(
        "L'application permet de positionner la balise de détresse sur la carte, "
        "soit manuellement, soit en utilisant le centre de la zone d'intersection calculée.",
        normal_style
    ))
    
    elements.append(Paragraph("Définir la position", subsection_style))
    elements.append(Paragraph("Menu <b>Édition > Définir position balise</b> ouvre un dialogue permettant :", normal_style))
    elements.append(Paragraph("• Saisie en <b>Degrés Décimaux</b> (DD) : latitude et longitude décimales", bullet_style))
    elements.append(Paragraph("• Saisie en <b>Degrés Minutes Secondes</b> (DMS) : format complet avec N/S/E/W", bullet_style))
    elements.append(Paragraph("• Bouton pour utiliser le centre de la zone d'intersection", bullet_style))
    
    elements.append(Paragraph("Déplacer la balise", subsection_style))
    elements.append(Paragraph(
        "La balise (représentée par un cercle rouge pulsant) peut être déplacée directement "
        "sur la carte par glisser-déposer, comme les stations.",
        normal_style
    ))
    
    elements.append(Paragraph("Effacer la position", subsection_style))
    elements.append(Paragraph(
        "Menu <b>Édition > Effacer position balise</b> supprime le marqueur de la carte.",
        normal_style
    ))
    
    # === 6. PRÉSETS DE STATIONS ===
    elements.append(Paragraph("6. Présets de stations", section_style))
    
    elements.append(Paragraph(
        "Les présets permettent de sauvegarder les positions habituelles des stations pour les réutiliser "
        "rapidement lors de missions futures.",
        normal_style
    ))
    
    elements.append(Paragraph("Créer un préset", subsection_style))
    elements.append(Paragraph("1. Configurez une station avec les coordonnées souhaitées", bullet_style))
    elements.append(Paragraph("2. Menu <b>Stations > Sauvegarder station comme préset</b>", bullet_style))
    elements.append(Paragraph("3. Donnez un nom descriptif au préset", bullet_style))
    
    elements.append(Paragraph("Gérer les présets", subsection_style))
    elements.append(Paragraph(
        "Le menu <b>Stations > Gérer les présets</b> ouvre une fenêtre permettant de :",
        normal_style
    ))
    elements.append(Paragraph("• Visualiser tous les présets enregistrés", bullet_style))
    elements.append(Paragraph("• Ajouter de nouveaux présets manuellement", bullet_style))
    elements.append(Paragraph("• Modifier les présets existants", bullet_style))
    elements.append(Paragraph("• Supprimer des présets", bullet_style))
    elements.append(Paragraph("• Charger des présets sélectionnés comme nouvelles stations", bullet_style))
    
    elements.append(Paragraph("Charger des présets", subsection_style))
    elements.append(Paragraph(
        "Menu <b>Stations > Charger présets</b> : sélectionnez un ou plusieurs présets à charger "
        "comme nouvelles stations.",
        normal_style
    ))
    
    elements.append(Paragraph(
        "Les présets sont sauvegardés dans le fichier <b>station_presets.json</b> à côté de l'application.",
        normal_style
    ))
    
    elements.append(PageBreak())
    
    # === 7. CARTE ET NAVIGATION ===
    elements.append(Paragraph("7. Carte et navigation", section_style))
    
    elements.append(Paragraph("Fonds de carte disponibles", subsection_style))
    elements.append(Paragraph("• OpenStreetMap : carte généraliste", bullet_style))
    elements.append(Paragraph("• OpenStreetMap France : variante française", bullet_style))
    elements.append(Paragraph("• OpenTopoMap : carte topographique", bullet_style))
    elements.append(Paragraph("• IGN France : cartes officielles IGN", bullet_style))
    elements.append(Paragraph("• IGN Satellite : images satellitaires IGN", bullet_style))
    elements.append(Paragraph("• CartoDB : styles épurés (clair et sombre)", bullet_style))
    elements.append(Paragraph("• ESRI Satellite : images satellitaires mondiales", bullet_style))
    
    elements.append(Paragraph(
        "<b>Note</b> : Lors du changement de fond de carte, la position et le niveau de zoom "
        "sont conservés.",
        normal_style
    ))
    
    elements.append(Paragraph("Grille UTM/MGRS", subsection_style))
    elements.append(Paragraph(
        "Cochez 'Afficher grille UTM/MGRS' pour superposer une grille de coordonnées militaires. "
        "La couleur de la grille est personnalisable via le bouton de couleur adjacent. "
        "Les labels se repositionnent automatiquement lors du déplacement de la carte.",
        normal_style
    ))
    
    elements.append(Paragraph("Clic droit", subsection_style))
    elements.append(Paragraph(
        "Un clic droit sur la carte affiche les coordonnées du point en trois formats : "
        "DD (degrés décimaux), DMS (degrés/minutes/secondes) et zone UTM.",
        normal_style
    ))
    
    elements.append(Paragraph("Centrage", subsection_style))
    elements.append(Paragraph("• <b>Édition > Centrer France</b> : vue d'ensemble du pays", bullet_style))
    elements.append(Paragraph("• <b>Édition > Centrer sur zone d'intersection</b> : zoom sur la zone calculée", bullet_style))
    
    # === 8. ZONE D'INTERSECTION ===
    elements.append(Paragraph("8. Zone d'intersection", section_style))
    
    elements.append(Paragraph(
        "Lorsque deux stations ou plus sont configurées avec des azimuts, l'application calcule "
        "automatiquement les points d'intersection et affiche une zone circulaire englobante.",
        normal_style
    ))
    
    elements.append(Paragraph("Calcul de la zone", subsection_style))
    elements.append(Paragraph(
        "L'algorithme utilise le plus petit cercle englobant pour déterminer "
        "la zone la plus probable de localisation de la balise. Le calcul est indépendant "
        "de la longueur d'affichage des azimuts. Le panneau d'informations affiche :",
        normal_style
    ))
    elements.append(Paragraph("• Les coordonnées du centre (DD et DMS)", bullet_style))
    elements.append(Paragraph("• La référence MGRS", bullet_style))
    elements.append(Paragraph("• Le rayon en kilomètres", bullet_style))
    elements.append(Paragraph("• La surface en km²", bullet_style))
    
    elements.append(Paragraph("Personnalisation de l'affichage", subsection_style))
    elements.append(Paragraph("Dans le groupe 'Zone d'intersection' du panneau de contrôle :", normal_style))
    elements.append(Paragraph("• <b>Bordure</b> : couleur du contour de la zone", bullet_style))
    elements.append(Paragraph("• <b>Remplissage</b> : couleur de fond de la zone", bullet_style))
    elements.append(Paragraph("• <b>Opacité</b> : transparence du remplissage (0-100%)", bullet_style))
    
    elements.append(PageBreak())
    
    # === 9. HISTORIQUE DES RELEVÉS ===
    elements.append(Paragraph("9. Historique des relevés", section_style))
    
    elements.append(Paragraph(
        "L'historique conserve tous les relevés enregistrés manuellement pendant la mission. "
        "Chaque entrée contient la date, l'heure, les coordonnées et les mesures.",
        normal_style
    ))
    
    elements.append(Paragraph("Enregistrer un relevé", subsection_style))
    elements.append(Paragraph(
        "Cliquez sur le bouton <b>📝</b> dans la ligne de la station pour enregistrer "
        "le relevé actuel dans l'historique avec un horodatage complet (date et heure).",
        normal_style
    ))
    
    elements.append(Paragraph("Consulter l'historique", subsection_style))
    elements.append(Paragraph(
        "Cliquez sur le bouton '📜 Historique' pour ouvrir la fenêtre d'historique. "
        "Le tableau affiche pour chaque relevé :",
        normal_style
    ))
    elements.append(Paragraph("• Date et heure du relevé", bullet_style))
    elements.append(Paragraph("• Indicatif de la station", bullet_style))
    elements.append(Paragraph("• Azimut et incertitude", bullet_style))
    elements.append(Paragraph("• Signal S-mètre", bullet_style))
    elements.append(Paragraph("• Coordonnées en DD et DMS", bullet_style))
    
    elements.append(Paragraph("Gestion de l'historique", subsection_style))
    elements.append(Paragraph("• <b>Supprimer</b> : supprime les relevés sélectionnés", bullet_style))
    elements.append(Paragraph("• <b>Tout effacer</b> : vide l'intégralité de l'historique", bullet_style))
    elements.append(Paragraph("• <b>Exporter CSV</b> : sauvegarde au format CSV pour analyse", bullet_style))
    
    # === 10. EXPORTS ET RAPPORTS ===
    elements.append(Paragraph("10. Exports et rapports", section_style))
    
    elements.append(Paragraph("Rapport PDF de mission", subsection_style))
    elements.append(Paragraph(
        "Menu <b>Fichier > Générer rapport PDF</b> (ou Ctrl+P) ouvre un dialogue pour :",
        normal_style
    ))
    elements.append(Paragraph("• Saisir le titre et le sujet du rapport", bullet_style))
    elements.append(Paragraph("• Définir les mots-clés", bullet_style))
    elements.append(Paragraph("• Ajouter des commentaires et notes", bullet_style))
    
    elements.append(Paragraph("Le rapport PDF inclut :", normal_style))
    elements.append(Paragraph("• Logo ADRASEC centré en en-tête", bullet_style))
    elements.append(Paragraph("• Informations de mission (date, durée, nombre de stations)", bullet_style))
    elements.append(Paragraph("• Commentaires saisis", bullet_style))
    elements.append(Paragraph("• Tableau des stations avec positions et azimuts", bullet_style))
    elements.append(Paragraph("• Kilomètres parcourus par station", bullet_style))
    elements.append(Paragraph("• Position de la balise de détresse", bullet_style))
    elements.append(Paragraph("• Données de la zone d'intersection", bullet_style))
    elements.append(Paragraph("• Capture de la carte (avec proportions conservées)", bullet_style))
    elements.append(Paragraph("• Historique des relevés", bullet_style))
    
    elements.append(Paragraph("Export HTML autonome", subsection_style))
    elements.append(Paragraph(
        "Menu <b>Fichier > Sauvegarder HTML</b> (ou Ctrl+S) génère un fichier HTML autonome "
        "avec le même fond de carte et la même vue que l'écran actuel.",
        normal_style
    ))
    
    elements.append(Paragraph("Export PNG", subsection_style))
    elements.append(Paragraph(
        "Menu <b>Fichier > Exporter PNG</b> capture la carte visible en image PNG.",
        normal_style
    ))
    
    elements.append(Paragraph("Export KML", subsection_style))
    elements.append(Paragraph(
        "Menu <b>Fichier > Exporter KML</b> crée un fichier compatible Google Earth.",
        normal_style
    ))
    
    elements.append(Paragraph("Export/Import JSON", subsection_style))
    elements.append(Paragraph(
        "Les menus <b>Exporter JSON</b> et <b>Importer JSON</b> permettent de sauvegarder et restaurer "
        "l'état complet de l'application (stations, historique, balise, paramètres).",
        normal_style
    ))
    
    elements.append(PageBreak())
    
    # === 11. MODE HORS-LIGNE ===
    elements.append(Paragraph("11. Mode hors-ligne", section_style))
    
    elements.append(Paragraph(
        "L'application peut fonctionner sans connexion Internet en utilisant des tuiles de carte "
        "préalablement téléchargées.",
        normal_style
    ))
    
    elements.append(Paragraph("Télécharger les tuiles", subsection_style))
    elements.append(Paragraph("1. Positionnez la carte sur la zone souhaitée", bullet_style))
    elements.append(Paragraph("2. Menu <b>Édition > Télécharger les tuiles visibles</b>", bullet_style))
    elements.append(Paragraph("3. Confirmez le téléchargement (nombre de tuiles affiché)", bullet_style))
    elements.append(Paragraph("4. Attendez la fin du téléchargement", bullet_style))
    
    elements.append(Paragraph("Activer le mode hors-ligne", subsection_style))
    elements.append(Paragraph(
        "Cochez 'Mode hors-ligne' dans les paramètres de la carte. L'application utilisera "
        "les tuiles stockées localement dans le dossier './tiles/'.",
        normal_style
    ))
    
    elements.append(Paragraph(
        "<b>Note</b> : Le téléchargement des tuiles respecte les conditions d'utilisation des "
        "fournisseurs de cartes. Utilisez cette fonctionnalité de manière responsable.",
        normal_style
    ))
    
    # === 12. RACCOURCIS CLAVIER ===
    elements.append(Paragraph("12. Raccourcis clavier", section_style))
    
    shortcuts = [
        ["Raccourci", "Action"],
        ["Ctrl+S", "Sauvegarder en HTML"],
        ["Ctrl+P", "Générer rapport PDF"],
        ["Ctrl+Q", "Quitter l'application"],
    ]
    
    t = Table(shortcuts, colWidths=[4*cm, 10*cm])
    t.setStyle(TableStyle([
        ('BACKGROUND', (0, 0), (-1, 0), colors.HexColor('#e65c00')),
        ('TEXTCOLOR', (0, 0), (-1, 0), colors.white),
        ('FONTNAME', (0, 0), (-1, 0), 'Helvetica-Bold'),
        ('FONTSIZE', (0, 0), (-1, -1), 10),
        ('PADDING', (0, 0), (-1, -1), 8),
        ('GRID', (0, 0), (-1, -1), 0.5, colors.HexColor('#bdc3c7')),
    ]))
    elements.append(t)
    
    elements.append(Spacer(1, 40))
    
    # Ligne de séparation
    line_data2 = [['', '']]
    line_table2 = Table(line_data2, colWidths=[8.5*cm, 8.5*cm])
    line_table2.setStyle(TableStyle([
        ('BACKGROUND', (0, 0), (0, 0), colors.HexColor('#e65c00')),
        ('BACKGROUND', (1, 0), (1, 0), colors.HexColor('#003d80')),
    ]))
    elements.append(line_table2)
    elements.append(Spacer(1, 15))
    
    # Pied de page
    elements.append(Paragraph(
        "<para align='center'><b>ADRASEC</b><br/>"
        "Association Départementale des Radioamateurs<br/>"
        "au Service de la Sécurité Civile<br/><br/>"
        "SATER Map v2.0.0</para>",
        normal_style
    ))
    
    doc.build(elements)
    print(f"Manuel généré : {output_path}")


if __name__ == "__main__":
    import sys
    
    # Déterminer le répertoire du script
    script_dir = os.path.dirname(os.path.abspath(__file__))
    img_dir = os.path.join(script_dir, "img")
    output = os.path.join(script_dir, "SATER_Map_Manuel.pdf")
    
    # Permettre de spécifier un chemin de sortie en argument
    if len(sys.argv) > 1:
        output = sys.argv[1]
    
    generate_manual(output, img_dir)
