#!/usr/bin/python3
# -*- coding: utf-8 -*-
"""
SATER  Map - Version 2.0
"""

import json
import os
import shutil
import sys
import tempfile
import threading
import urllib.request
from dataclasses import dataclass, asdict
from datetime import datetime, timedelta
from math import cos, degrees, radians, sin, tan, atan2, sqrt, pi, asin, log
from typing import Optional, List, Tuple, Dict

from PyQt6.QtCore import QObject, Qt, QTimer, QUrl, pyqtSignal
from PyQt6.QtGui import QAction, QIcon, QColor
from PyQt6.QtWebEngineCore import QWebEngineSettings
from PyQt6.QtWebEngineWidgets import QWebEngineView
from PyQt6.QtWidgets import (QApplication, QCheckBox, QColorDialog, QComboBox,
                             QDoubleSpinBox, QFileDialog, QGroupBox,
                             QHBoxLayout, QLabel, QLineEdit, QMainWindow,
                             QMessageBox, QPushButton, QSlider, QSpinBox, QStyle,
                             QVBoxLayout, QWidget, QStatusBar, QFormLayout,
                             QTextEdit, QSplitter, QProgressDialog,
                             QLCDNumber, QTableWidget, QTableWidgetItem, QHeaderView,
                             QTabWidget, QDialog, QDialogButtonBox, QInputDialog,
                             QListWidget, QListWidgetItem, QAbstractItemView, QSizePolicy)

try:
    from reportlab.lib import colors
    from reportlab.lib.pagesizes import A4
    from reportlab.lib.styles import getSampleStyleSheet, ParagraphStyle
    from reportlab.lib.units import mm, cm
    from reportlab.platypus import SimpleDocTemplate, Paragraph, Spacer, Table, TableStyle, Image
    from reportlab.lib.enums import TA_CENTER, TA_LEFT, TA_RIGHT
    HAS_REPORTLAB = True
except ImportError:
    HAS_REPORTLAB = False

APP_NAME = "SATER  Map"
APP_VERSION = "2.0.0"
ICON_PATH = "./img/logo.jpg"
AZIMUTH_LENGTH_KM = 100
TILES_DIR = "./tiles"
PRESETS_FILE = "./station_presets.json"
STATION_COLORS = ["#e74c3c", "#3498db", "#2ecc71", "#9b59b6", 
                  "#f39c12", "#1abc9c", "#e91e63", "#795548"]
SIGNAL_LEVELS = ["S0", "S1", "S2", "S3", "S4", "S5", "S6", "S7", "S8", "S9", "S9+10", "S9+20", "S9+30"]
TILE_PROVIDERS = {
    'osm': {
        'name': 'OpenStreetMap',
        'url': 'https://{s}.tile.openstreetmap.org/{z}/{x}/{y}.png',
        'subdomains': ['a', 'b', 'c'],
        'attribution': '© OpenStreetMap',
        'max_zoom': 19
    },
    'osm-fr': {
        'name': 'OpenStreetMap France',
        'url': 'https://{s}.tile.openstreetmap.fr/osmfr/{z}/{x}/{y}.png',
        'subdomains': ['a', 'b', 'c'],
        'attribution': '© OpenStreetMap France',
        'max_zoom': 19
    },
    'topo': {
        'name': 'OpenTopoMap',
        'url': 'https://{s}.tile.opentopomap.org/{z}/{x}/{y}.png',
        'subdomains': ['a', 'b', 'c'],
        'attribution': '© OpenTopoMap',
        'max_zoom': 17
    },
    'cartodb-dark': {
        'name': 'CartoDB Dark',
        'url': 'https://{s}.basemaps.cartocdn.com/dark_all/{z}/{x}/{y}{r}.png',
        'subdomains': ['a', 'b', 'c', 'd'],
        'attribution': '© CartoDB © OSM',
        'max_zoom': 20
    },
    'geoportail-plan': {
        'name': 'Géoportail Plan',
        'url': 'https://data.geopf.fr/wmts?SERVICE=WMTS&REQUEST=GetTile&VERSION=1.0.0&TILEMATRIXSET=PM&LAYER=GEOGRAPHICALGRIDSYSTEMS.PLANIGNV2&STYLE=normal&FORMAT=image/png&TILECOL={x}&TILEROW={y}&TILEMATRIX={z}',
        'subdomains': [],
        'attribution': '© IGN Géoportail',
        'max_zoom': 18
    },
    'geoportail-sat': {
        'name': 'Géoportail Satellite',
        'url': 'https://data.geopf.fr/wmts?SERVICE=WMTS&REQUEST=GetTile&VERSION=1.0.0&TILEMATRIXSET=PM&LAYER=ORTHOIMAGERY.ORTHOPHOTOS&STYLE=normal&FORMAT=image/jpeg&TILECOL={x}&TILEROW={y}&TILEMATRIX={z}',
        'subdomains': [],
        'attribution': '© IGN Géoportail',
        'max_zoom': 19
    },
    'esri-sat': {
        'name': 'Esri Satellite',
        'url': 'https://server.arcgisonline.com/ArcGIS/rest/services/World_Imagery/MapServer/tile/{z}/{y}/{x}',
        'subdomains': [],
        'attribution': '© Esri',
        'max_zoom': 19
    }
}

@dataclass
class AzimuthRecord:
    """Enregistrement d'un relevé d'azimut"""
    timestamp: str
    callsign: str
    azimuth: float
    uncertainty: float
    lat: float
    lon: float
    signal: str = "S5"
    
    def to_dict(self) -> dict:
        return asdict(self)

@dataclass
class Station:
    callsign: str
    lat: float
    lon: float
    azimuth: float
    uncertainty: float = 5.0
    color: str = "#e74c3c"
    visible: bool = True
    signal: str = "S5"
    notes: str = ""
    station_id: str = ""  # Identifiant unique technique
    
    def __post_init__(self):
        # Générer un ID unique si non fourni
        if not self.station_id:
            import uuid
            self.station_id = str(uuid.uuid4())[:8]
    
    def to_dict(self) -> dict:
        return asdict(self)


@dataclass
class StationPreset:
    """Préset de station (position habituelle)"""
    name: str
    callsign: str
    lat: float
    lon: float
    color: str = "#e74c3c"
    default_uncertainty: float = 5.0
    
    def to_dict(self) -> dict:
        return asdict(self)
    
    @classmethod
    def from_dict(cls, data: dict) -> 'StationPreset':
        return cls(**data)


def load_presets(filepath: str = PRESETS_FILE) -> List[StationPreset]:
    """Charge les présets depuis un fichier JSON"""
    if os.path.exists(filepath):
        try:
            with open(filepath, 'r', encoding='utf-8') as f:
                data = json.load(f)
                return [StationPreset.from_dict(p) for p in data]
        except:
            pass
    return []


def save_presets(presets: List[StationPreset], filepath: str = PRESETS_FILE):
    """Sauvegarde les présets dans un fichier JSON"""
    with open(filepath, 'w', encoding='utf-8') as f:
        json.dump([p.to_dict() for p in presets], f, indent=2, ensure_ascii=False)


def dms_to_dd(deg: int, minutes: int, seconds: float, direction: str) -> float:
    dd = deg + (minutes / 60) + (seconds / 3600)
    if direction in ['W', 'S']:
        dd = -dd
    return round(dd, 6)


def dd_to_dms(dd: float, coord_type: str) -> tuple:
    is_positive = dd >= 0
    dd = abs(dd)
    deg = int(dd)
    minutes = int((dd - deg) * 60)
    seconds = round((dd - deg - minutes / 60) * 3600, 2)
    if coord_type == "lat":
        direction = "N" if is_positive else "S"
    else:
        direction = "E" if is_positive else "W"
    return deg, minutes, seconds, direction


def dd_to_dms_str(dd: float, coord_type: str) -> str:
    """Convertit DD en chaîne DMS formatée"""
    deg, minutes, seconds, direction = dd_to_dms(dd, coord_type)
    return f"{deg}°{minutes}'{seconds:.1f}\"{direction}"


def lat_lon_to_utm(lat: float, lon: float) -> Tuple[int, str, float, float]:
    """Convertit lat/lon en coordonnées UTM."""
    zone = int((lon + 180) / 6) + 1
    
    if 56 <= lat < 64 and 3 <= lon < 12:
        zone = 32
    elif 72 <= lat < 84:
        if 0 <= lon < 9:
            zone = 31
        elif 9 <= lon < 21:
            zone = 33
        elif 21 <= lon < 33:
            zone = 35
        elif 33 <= lon < 42:
            zone = 37
    
    letters = "CDEFGHJKLMNPQRSTUVWXX"
    if -80 <= lat <= 84:
        letter = letters[int((lat + 80) / 8)]
    else:
        letter = 'Z'
    
    a = 6378137.0
    f = 1 / 298.257223563
    k0 = 0.9996
    
    e = sqrt(f * (2 - f))
    e2 = e * e
    ep2 = e2 / (1 - e2)
    
    lon0 = radians((zone - 1) * 6 - 180 + 3)
    lat_rad = radians(lat)
    lon_rad = radians(lon)
    
    N = a / sqrt(1 - e2 * sin(lat_rad)**2)
    T = tan(lat_rad)**2
    C = ep2 * cos(lat_rad)**2
    A = (lon_rad - lon0) * cos(lat_rad)
    
    M = a * ((1 - e2/4 - 3*e2**2/64 - 5*e2**3/256) * lat_rad
             - (3*e2/8 + 3*e2**2/32 + 45*e2**3/1024) * sin(2*lat_rad)
             + (15*e2**2/256 + 45*e2**3/1024) * sin(4*lat_rad)
             - (35*e2**3/3072) * sin(6*lat_rad))
    
    easting = k0 * N * (A + (1-T+C)*A**3/6 + (5-18*T+T**2+72*C-58*ep2)*A**5/120) + 500000
    northing = k0 * (M + N * tan(lat_rad) * (A**2/2 + (5-T+9*C+4*C**2)*A**4/24
                     + (61-58*T+T**2+600*C-330*ep2)*A**6/720))
    
    if lat < 0:
        northing += 10000000
    
    return zone, letter, easting, northing


def lat_lon_to_mgrs(lat: float, lon: float) -> str:
    """Convertit lat/lon en coordonnées MGRS"""
    zone, letter, easting, northing = lat_lon_to_utm(lat, lon)
    
    set_number = zone % 6
    if set_number == 0:
        set_number = 6
    
    col_letters = ["ABCDEFGH", "JKLMNPQR", "STUVWXYZ", "ABCDEFGH", "JKLMNPQR", "STUVWXYZ"]
    col_idx = int(easting / 100000) - 1
    if col_idx < 0:
        col_idx = 0
    if col_idx > 7:
        col_idx = 7
    col_letter = col_letters[set_number - 1][col_idx]
    
    row_letters = "ABCDEFGHJKLMNPQRSTUV"
    row_idx = int(northing / 100000) % 20
    row_letter = row_letters[row_idx % 20]
    
    e_100k = int(easting % 100000)
    n_100k = int(northing % 100000)
    
    return f"{zone}{letter} {col_letter}{row_letter} {e_100k:05d} {n_100k:05d}"


def calc_endpoint_haversine(lat: float, lon: float, azimuth: float, dist_km: float) -> Tuple[float, float]:
    R = 6371.0
    lat1 = radians(lat)
    lon1 = radians(lon)
    brng = radians(azimuth)
    
    lat2 = asin(sin(lat1) * cos(dist_km / R) + cos(lat1) * sin(dist_km / R) * cos(brng))
    lon2 = lon1 + atan2(sin(brng) * sin(dist_km / R) * cos(lat1),
                        cos(dist_km / R) - sin(lat1) * sin(lat2))
    
    return degrees(lat2), degrees(lon2)


def line_intersection(p1: Tuple[float, float], p2: Tuple[float, float],
                      p3: Tuple[float, float], p4: Tuple[float, float]) -> Optional[Tuple[float, float]]:
    x1, y1 = p1
    x2, y2 = p2
    x3, y3 = p3
    x4, y4 = p4
    
    denom = (x1 - x2) * (y3 - y4) - (y1 - y2) * (x3 - x4)
    
    if abs(denom) < 1e-10:
        return None
    
    t = ((x1 - x3) * (y3 - y4) - (y1 - y3) * (x3 - x4)) / denom
    
    px = x1 + t * (x2 - x1)
    py = y1 + t * (y2 - y1)
    
    return (px, py)


def calculate_all_intersections(stations: List[Station], azimuth_length_km: float = 100) -> List[Tuple[float, float]]:
    """Calcule les intersections entre tous les azimuts."""
    intersections = []
    
    for i in range(len(stations)):
        for j in range(i + 1, len(stations)):
            s1, s2 = stations[i], stations[j]
            
            # Calculer les endpoints avec la longueur d'affichage
            end1 = calc_endpoint_haversine(s1.lat, s1.lon, s1.azimuth, azimuth_length_km)
            end2 = calc_endpoint_haversine(s2.lat, s2.lon, s2.azimuth, azimuth_length_km)
            
            # Calculer l'intersection des lignes
            inter = line_intersection(
                (s1.lon, s1.lat), (end1[1], end1[0]),
                (s2.lon, s2.lat), (end2[1], end2[0])
            )
            
            if inter:
                int_lon, int_lat = inter
                
                # Vérifier que l'intersection est dans la direction de l'azimut
                # (pas derrière la station)
                dir1_to_inter = atan2(int_lon - s1.lon, int_lat - s1.lat)
                dir1_azimuth = radians(s1.azimuth)
                dir2_to_inter = atan2(int_lon - s2.lon, int_lat - s2.lat)
                dir2_azimuth = radians(s2.azimuth)
                
                # Normaliser les différences d'angle
                diff1 = abs(dir1_to_inter - dir1_azimuth)
                diff2 = abs(dir2_to_inter - dir2_azimuth)
                
                if diff1 > pi:
                    diff1 = 2 * pi - diff1
                if diff2 > pi:
                    diff2 = 2 * pi - diff2
                
                # L'intersection doit être "devant" les deux stations (différence < 90°)
                if diff1 < pi / 2 and diff2 < pi / 2:
                    if -90 <= int_lat <= 90 and -180 <= int_lon <= 180:
                        intersections.append((int_lat, int_lon))
    
    return intersections


def distance_km(lat1: float, lon1: float, lat2: float, lon2: float) -> float:
    dlat = (lat2 - lat1) * 111.0
    dlon = (lon2 - lon1) * 111.0 * cos(radians((lat1 + lat2) / 2))
    return sqrt(dlat**2 + dlon**2)


def smallest_enclosing_circle(points: List[Tuple[float, float]]) -> Tuple[float, float, float]:
    if not points:
        return (0, 0, 0)
    
    if len(points) == 1:
        return (points[0][0], points[0][1], 0.5)
    
    if len(points) == 2:
        p1, p2 = points
        center_lat = (p1[0] + p2[0]) / 2
        center_lon = (p1[1] + p2[1]) / 2
        radius = distance_km(center_lat, center_lon, p1[0], p1[1])
        return (center_lat, center_lon, max(radius, 0.5))
    
    max_dist = 0
    p1, p2 = points[0], points[1]
    for i in range(len(points)):
        for j in range(i + 1, len(points)):
            d = distance_km(points[i][0], points[i][1], points[j][0], points[j][1])
            if d > max_dist:
                max_dist = d
                p1, p2 = points[i], points[j]
    
    center_lat = (p1[0] + p2[0]) / 2
    center_lon = (p1[1] + p2[1]) / 2
    radius = max_dist / 2
    
    for p in points:
        d = distance_km(center_lat, center_lon, p[0], p[1])
        if d > radius:
            new_radius = (radius + d) / 2
            ratio = (new_radius - radius) / d if d > 0 else 0
            center_lat = center_lat + ratio * (p[0] - center_lat)
            center_lon = center_lon + ratio * (p[1] - center_lon)
            radius = new_radius
    
    for p in points:
        d = distance_km(center_lat, center_lon, p[0], p[1])
        if d > radius:
            radius = d
    
    return (center_lat, center_lon, max(radius, 0.5))


def calculate_uncertainty_circle(intersections: List[Tuple[float, float]]) -> Optional[Tuple[float, float, float, float]]:
    if not intersections:
        return None
    
    center_lat, center_lon, radius_km = smallest_enclosing_circle(intersections)
    surface_km2 = pi * radius_km * radius_km
    
    return (center_lat, center_lon, radius_km, surface_km2)


def lat_lon_to_tile(lat: float, lon: float, zoom: int) -> Tuple[int, int]:
    lat_rad = radians(lat)
    n = 2 ** zoom
    x = int((lon + 180.0) / 360.0 * n)
    y = int((1.0 - log(tan(lat_rad) + 1.0 / cos(lat_rad)) / pi) / 2.0 * n)
    return x, y


def get_tiles_for_bounds(min_lat: float, min_lon: float, max_lat: float, max_lon: float, 
                         min_zoom: int, max_zoom: int) -> List[Tuple[int, int, int]]:
    tiles = []
    for z in range(min_zoom, max_zoom + 1):
        x_min, y_max = lat_lon_to_tile(min_lat, min_lon, z)
        x_max, y_min = lat_lon_to_tile(max_lat, max_lon, z)
        for x in range(x_min, x_max + 1):
            for y in range(y_min, y_max + 1):
                tiles.append((z, x, y))
    return tiles


def get_tile_url(provider_key: str, z: int, x: int, y: int, subdomain_index: int = 0) -> str:
    provider = TILE_PROVIDERS.get(provider_key, TILE_PROVIDERS['osm'])
    url = provider['url']
    if provider['subdomains']:
        subdomain = provider['subdomains'][subdomain_index % len(provider['subdomains'])]
        url = url.replace('{s}', subdomain)
    url = url.replace('{z}', str(z))
    url = url.replace('{x}', str(x))
    url = url.replace('{y}', str(y))
    return url


class TileDownloader(QObject):
    progress = pyqtSignal(int, int, str)
    finished = pyqtSignal(int, int)
    
    def __init__(self, tiles: List[Tuple[int, int, int]], tiles_dir: str, provider_key: str):
        super().__init__()
        self.tiles = tiles
        self.tiles_dir = tiles_dir
        self.provider_key = provider_key
        self.cancelled = False
    
    def cancel(self):
        self.cancelled = True
    
    def download(self):
        downloaded = 0
        failed = 0
        total = len(self.tiles)
        
        for i, (z, x, y) in enumerate(self.tiles):
            if self.cancelled:
                break
            
            tile_path = os.path.join(self.tiles_dir, self.provider_key, str(z), str(x), f"{y}.png")
            
            if os.path.exists(tile_path):
                downloaded += 1
                self.progress.emit(i + 1, total, f"{self.provider_key}/{z}/{x}/{y} (cache)")
                continue
            
            os.makedirs(os.path.dirname(tile_path), exist_ok=True)
            url = get_tile_url(self.provider_key, z, x, y, i)
            
            try:
                req = urllib.request.Request(url, headers={
                    'User-Agent': 'SATER_Map/2.0 (Amateur Radio Emergency Service)'
                })
                with urllib.request.urlopen(req, timeout=15) as response:
                    with open(tile_path, 'wb') as f:
                        f.write(response.read())
                downloaded += 1
                self.progress.emit(i + 1, total, f"{self.provider_key}/{z}/{x}/{y} OK")
            except Exception as e:
                failed += 1
                self.progress.emit(i + 1, total, f"{self.provider_key}/{z}/{x}/{y} ERREUR")
        
        self.finished.emit(downloaded, failed)


class HistoryDialog(QDialog):
    def __init__(self, history: List[AzimuthRecord], parent=None):
        super().__init__(parent)
        self.setWindowTitle("Historique des relevés")
        self.setMinimumSize(650, 500)
        
        layout = QVBoxLayout(self)
        
        self.table = QTableWidget()
        self.table.setColumnCount(8)
        self.table.setHorizontalHeaderLabels([
            "Heure", "Indicatif", "Azimut", "±", "Signal",
            "Latitude (DD)", "Longitude (DD)", "DMS"
        ])
        self.table.horizontalHeader().setSectionResizeMode(QHeaderView.ResizeMode.ResizeToContents)
        self.table.setSelectionBehavior(QTableWidget.SelectionBehavior.SelectRows)
        self.table.setSelectionMode(QTableWidget.SelectionMode.ExtendedSelection)
        
        self.history = history.copy()  # Copie pour modifications
        self.deleted_indices = []  # Indices supprimés
        self._populate_table()
        
        layout.addWidget(self.table)
        
        btn_layout = QHBoxLayout()
        
        delete_selected_btn = QPushButton("Supprimer sélection")
        delete_selected_btn.clicked.connect(self._delete_selected)
        btn_layout.addWidget(delete_selected_btn)
        
        export_btn = QPushButton("Exporter CSV")
        export_btn.clicked.connect(self.export_csv)
        btn_layout.addWidget(export_btn)
        
        btn_layout.addStretch()
        
        clear_btn = QPushButton("Tout effacer")
        clear_btn.clicked.connect(self._clear_all)
        btn_layout.addWidget(clear_btn)
        
        close_btn = QPushButton("Fermer")
        close_btn.clicked.connect(self.reject)
        btn_layout.addWidget(close_btn)
        
        layout.addLayout(btn_layout)
    
    def _populate_table(self):
        """Remplit le tableau avec l'historique"""
        self.table.setRowCount(len(self.history))
        for i, record in enumerate(reversed(self.history)):
            self.table.setItem(i, 0, QTableWidgetItem(record.timestamp))
            self.table.setItem(i, 1, QTableWidgetItem(record.callsign))
            self.table.setItem(i, 2, QTableWidgetItem(f"{record.azimuth:.0f}°"))
            self.table.setItem(i, 3, QTableWidgetItem(f"±{record.uncertainty:.1f}°"))
            self.table.setItem(i, 4, QTableWidgetItem(getattr(record, 'signal', 'S5')))
            self.table.setItem(i, 5, QTableWidgetItem(f"{record.lat:.6f}"))
            self.table.setItem(i, 6, QTableWidgetItem(f"{record.lon:.6f}"))
            dms_str = f"{dd_to_dms_str(record.lat, 'lat')} {dd_to_dms_str(record.lon, 'lon')}"
            self.table.setItem(i, 7, QTableWidgetItem(dms_str))
    
    def _delete_selected(self):
        """Supprime les lignes sélectionnées"""
        selected_rows = sorted(set(item.row() for item in self.table.selectedItems()), reverse=True)
        if not selected_rows:
            return
        
        # Convertir les indices de tableau (inversé) en indices d'historique
        total = len(self.history)
        for row in selected_rows:
            history_idx = total - 1 - row
            if 0 <= history_idx < len(self.history):
                self.history.pop(history_idx)
                total -= 1
        
        self._populate_table()
    
    def _clear_all(self):
        """Efface tout l'historique"""
        if QMessageBox.question(self, "Confirmation", 
            "Effacer tout l'historique ?") == QMessageBox.StandardButton.Yes:
            self.history.clear()
            self._populate_table()
            self.accept()
    
    def get_history(self) -> List[AzimuthRecord]:
        """Retourne l'historique modifié"""
        return self.history
    
    def export_csv(self):
        fn, _ = QFileDialog.getSaveFileName(self, "Exporter CSV", "historique_releves.csv", "CSV (*.csv)")
        if fn:
            with open(fn, 'w', encoding='utf-8') as f:
                f.write("Heure;Indicatif;Azimut;Incertitude;Signal;Lat_DD;Lon_DD;Lat_DMS;Lon_DMS\n")
                for record in self.history:
                    lat_dms = dd_to_dms_str(record.lat, 'lat')
                    lon_dms = dd_to_dms_str(record.lon, 'lon')
                    sig = getattr(record, 'signal', 'S5')
                    f.write(f"{record.timestamp};{record.callsign};{record.azimuth};{record.uncertainty};"
                            f"{sig};{record.lat};{record.lon};{lat_dms};{lon_dms}\n")
            QMessageBox.information(self, "Export", f"Historique exporté : {fn}")


class KilometersDialog(QDialog):
    """Dialogue pour saisir les kilomètres parcourus par station"""
    
    def __init__(self, callsigns: List[str], kilometers: Dict[str, float], parent=None):
        super().__init__(parent)
        self.setWindowTitle("Kilomètres parcourus")
        self.setMinimumWidth(350)
        
        self.kilometers = kilometers.copy()
        self.spinboxes: Dict[str, QDoubleSpinBox] = {}  # Toujours initialiser
        
        layout = QVBoxLayout(self)
        
        if not callsigns:
            layout.addWidget(QLabel("Aucune station définie.\nAjoutez des stations avec un indicatif."))
        else:
            # Créer un formulaire pour chaque station
            form_layout = QFormLayout()
            
            for callsign in callsigns:
                spin = QDoubleSpinBox()
                spin.setRange(0, 9999)
                spin.setDecimals(1)
                spin.setSuffix(" km")
                spin.setValue(self.kilometers.get(callsign, 0.0))
                self.spinboxes[callsign] = spin
                form_layout.addRow(f"{callsign}:", spin)
            
            layout.addLayout(form_layout)
            
            # Total
            self.total_label = QLabel()
            self._update_total()
            layout.addWidget(self.total_label)
            
            # Connecter pour mise à jour du total
            for spin in self.spinboxes.values():
                spin.valueChanged.connect(self._update_total)
        
        # Boutons
        btn_layout = QHBoxLayout()
        btn_layout.addStretch()
        
        ok_btn = QPushButton("OK")
        ok_btn.clicked.connect(self.accept)
        btn_layout.addWidget(ok_btn)
        
        cancel_btn = QPushButton("Annuler")
        cancel_btn.clicked.connect(self.reject)
        btn_layout.addWidget(cancel_btn)
        
        layout.addLayout(btn_layout)
    
    def _update_total(self):
        total = sum(spin.value() for spin in self.spinboxes.values())
        self.total_label.setText(f"<b>Total: {total:.1f} km</b>")
    
    def get_kilometers(self) -> Dict[str, float]:
        """Retourne le dictionnaire des kilomètres"""
        result = {}
        for callsign, spin in self.spinboxes.items():
            if spin.value() > 0:
                result[callsign] = spin.value()
        return result


class PresetManagerDialog(QDialog):
    """Dialogue de gestion des présets de stations"""
    
    def __init__(self, presets: List[StationPreset], parent=None):
        super().__init__(parent)
        self.setWindowTitle("Gestion des présets de stations")
        self.setMinimumSize(600, 400)
        self.presets = presets.copy()
        self.selected_presets = []
        
        layout = QVBoxLayout(self)
        
        # Liste des présets
        self.list_widget = QListWidget()
        self.list_widget.setSelectionMode(QAbstractItemView.SelectionMode.MultiSelection)
        self._refresh_list()
        layout.addWidget(self.list_widget)
        
        # Boutons de gestion
        btn_layout = QHBoxLayout()
        
        add_btn = QPushButton("➕ Ajouter")
        add_btn.clicked.connect(self._add_preset)
        btn_layout.addWidget(add_btn)
        
        edit_btn = QPushButton("✏️ Modifier")
        edit_btn.clicked.connect(self._edit_preset)
        btn_layout.addWidget(edit_btn)
        
        delete_btn = QPushButton("🗑️ Supprimer")
        delete_btn.clicked.connect(self._delete_preset)
        btn_layout.addWidget(delete_btn)
        
        layout.addLayout(btn_layout)
        
        # Boutons OK/Annuler
        btn_box = QDialogButtonBox()
        load_btn = btn_box.addButton("📥 Charger sélection", QDialogButtonBox.ButtonRole.AcceptRole)
        btn_box.addButton(QDialogButtonBox.StandardButton.Cancel)
        btn_box.accepted.connect(self._on_accept)
        btn_box.rejected.connect(self.reject)
        layout.addWidget(btn_box)
    
    def _refresh_list(self):
        self.list_widget.clear()
        for preset in self.presets:
            lat_dms = dd_to_dms_str(preset.lat, 'lat')
            lon_dms = dd_to_dms_str(preset.lon, 'lon')
            item = QListWidgetItem(f"{preset.name} ({preset.callsign}) - {lat_dms} {lon_dms}")
            item.setData(Qt.ItemDataRole.UserRole, preset)
            self.list_widget.addItem(item)
    
    def _add_preset(self):
        dialog = PresetEditDialog(parent=self)
        if dialog.exec() == QDialog.DialogCode.Accepted:
            preset = dialog.get_preset()
            if preset:
                self.presets.append(preset)
                self._refresh_list()
    
    def _edit_preset(self):
        items = self.list_widget.selectedItems()
        if not items:
            return
        item = items[0]
        preset = item.data(Qt.ItemDataRole.UserRole)
        dialog = PresetEditDialog(preset, parent=self)
        if dialog.exec() == QDialog.DialogCode.Accepted:
            new_preset = dialog.get_preset()
            if new_preset:
                idx = self.presets.index(preset)
                self.presets[idx] = new_preset
                self._refresh_list()
    
    def _delete_preset(self):
        items = self.list_widget.selectedItems()
        if not items:
            return
        if QMessageBox.question(self, "Confirmation", 
            f"Supprimer {len(items)} préset(s) ?") == QMessageBox.StandardButton.Yes:
            for item in items:
                preset = item.data(Qt.ItemDataRole.UserRole)
                if preset in self.presets:
                    self.presets.remove(preset)
            self._refresh_list()
    
    def _on_accept(self):
        self.selected_presets = []
        for item in self.list_widget.selectedItems():
            self.selected_presets.append(item.data(Qt.ItemDataRole.UserRole))
        self.accept()
    
    def get_presets(self) -> List[StationPreset]:
        return self.presets
    
    def get_selected_presets(self) -> List[StationPreset]:
        return self.selected_presets


class PresetEditDialog(QDialog):
    """Dialogue d'édition d'un préset"""
    
    def __init__(self, preset: StationPreset = None, parent=None):
        super().__init__(parent)
        self.setWindowTitle("Édition préset" if preset else "Nouveau préset")
        self.preset = preset
        
        layout = QFormLayout(self)
        
        self.name_edit = QLineEdit()
        self.name_edit.setPlaceholderText("Ex: Station Directrice")
        layout.addRow("Nom:", self.name_edit)
        
        self.callsign_edit = QLineEdit()
        self.callsign_edit.setPlaceholderText("Ex: FXXXX")
        layout.addRow("Indicatif:", self.callsign_edit)
        
        # Latitude
        lat_layout = QHBoxLayout()
        self.lat_deg = QSpinBox()
        self.lat_deg.setRange(0, 90)
        self.lat_deg.setSuffix("°")
        lat_layout.addWidget(self.lat_deg)
        self.lat_min = QSpinBox()
        self.lat_min.setRange(0, 59)
        self.lat_min.setSuffix("'")
        lat_layout.addWidget(self.lat_min)
        self.lat_sec = QDoubleSpinBox()
        self.lat_sec.setRange(0, 59.99)
        self.lat_sec.setDecimals(1)
        self.lat_sec.setSuffix('"')
        lat_layout.addWidget(self.lat_sec)
        self.lat_dir = QComboBox()
        self.lat_dir.addItems(["N", "S"])
        lat_layout.addWidget(self.lat_dir)
        layout.addRow("Latitude:", lat_layout)
        
        # Longitude
        lon_layout = QHBoxLayout()
        self.lon_deg = QSpinBox()
        self.lon_deg.setRange(0, 180)
        self.lon_deg.setSuffix("°")
        lon_layout.addWidget(self.lon_deg)
        self.lon_min = QSpinBox()
        self.lon_min.setRange(0, 59)
        self.lon_min.setSuffix("'")
        lon_layout.addWidget(self.lon_min)
        self.lon_sec = QDoubleSpinBox()
        self.lon_sec.setRange(0, 59.99)
        self.lon_sec.setDecimals(1)
        self.lon_sec.setSuffix('"')
        lon_layout.addWidget(self.lon_sec)
        self.lon_dir = QComboBox()
        self.lon_dir.addItems(["E", "W"])
        lon_layout.addWidget(self.lon_dir)
        layout.addRow("Longitude:", lon_layout)
        
        self.uncertainty_spin = QDoubleSpinBox()
        self.uncertainty_spin.setRange(0, 30)
        self.uncertainty_spin.setValue(5.0)
        self.uncertainty_spin.setSuffix("°")
        layout.addRow("Incertitude par défaut:", self.uncertainty_spin)
        
        self.color_btn = QPushButton()
        self.color = "#e74c3c"
        self._update_color_btn()
        self.color_btn.clicked.connect(self._choose_color)
        layout.addRow("Couleur:", self.color_btn)
        
        # Boutons
        btn_box = QDialogButtonBox(QDialogButtonBox.StandardButton.Ok | QDialogButtonBox.StandardButton.Cancel)
        btn_box.accepted.connect(self.accept)
        btn_box.rejected.connect(self.reject)
        layout.addRow(btn_box)
        
        # Remplir si modification
        if preset:
            self.name_edit.setText(preset.name)
            self.callsign_edit.setText(preset.callsign)
            lat_dms = dd_to_dms(preset.lat, "lat")
            self.lat_deg.setValue(lat_dms[0])
            self.lat_min.setValue(lat_dms[1])
            self.lat_sec.setValue(lat_dms[2])
            self.lat_dir.setCurrentText(lat_dms[3])
            lon_dms = dd_to_dms(preset.lon, "lon")
            self.lon_deg.setValue(lon_dms[0])
            self.lon_min.setValue(lon_dms[1])
            self.lon_sec.setValue(lon_dms[2])
            self.lon_dir.setCurrentText(lon_dms[3])
            self.uncertainty_spin.setValue(preset.default_uncertainty)
            self.color = preset.color
            self._update_color_btn()
    
    def _update_color_btn(self):
        self.color_btn.setStyleSheet(f"background-color: {self.color}; min-width: 60px;")
    
    def _choose_color(self):
        color = QColorDialog.getColor(QColor(self.color), self)
        if color.isValid():
            self.color = color.name()
            self._update_color_btn()
    
    def get_preset(self) -> Optional[StationPreset]:
        name = self.name_edit.text().strip()
        callsign = self.callsign_edit.text().strip()
        if not name or not callsign:
            return None
        lat = dms_to_dd(self.lat_deg.value(), self.lat_min.value(),
                       self.lat_sec.value(), self.lat_dir.currentText())
        lon = dms_to_dd(self.lon_deg.value(), self.lon_min.value(),
                       self.lon_sec.value(), self.lon_dir.currentText())
        return StationPreset(
            name=name,
            callsign=callsign,
            lat=lat,
            lon=lon,
            color=self.color,
            default_uncertainty=self.uncertainty_spin.value()
        )


def generate_pdf_report(filename: str, mission_data: dict, img_dir: str = "./img") -> bool:
    """Génère un rapport PDF de mission avec logo et header ADRASEC"""
    if not HAS_REPORTLAB:
        return False
    
    try:
        doc = SimpleDocTemplate(filename, pagesize=A4,
                               rightMargin=1.5*cm, leftMargin=1.5*cm,
                               topMargin=1*cm, bottomMargin=1.5*cm,
                               title=mission_data.get('title', 'Rapport de mission SATER'),
                               author='SATER Map',
                               subject=mission_data.get('subject', 'Radiogoniométrie'),
                               keywords=mission_data.get('keywords', 'SATER, ADRASEC, radiogoniométrie'),
                               creator='SATER Map')
        
        styles = getSampleStyleSheet()
        
        # Styles personnalisés
        title_style = ParagraphStyle(
            'TitleStyle',
            parent=styles['Heading1'],
            fontSize=20,
            alignment=TA_CENTER,
            spaceAfter=10,
            textColor=colors.HexColor('#2c3e50')
        )
        
        subtitle_style = ParagraphStyle(
            'SubtitleStyle',
            parent=styles['Normal'],
            fontSize=12,
            alignment=TA_CENTER,
            spaceAfter=20,
            textColor=colors.HexColor('#7f8c8d')
        )
        
        section_style = ParagraphStyle(
            'SectionStyle',
            parent=styles['Heading2'],
            fontSize=14,
            spaceBefore=15,
            spaceAfter=8,
            textColor=colors.HexColor('#e65c00')
        )
        
        normal_style = ParagraphStyle(
            'NormalStyle',
            parent=styles['Normal'],
            fontSize=10,
            spaceAfter=6
        )
        
        comment_style = ParagraphStyle(
            'CommentStyle',
            parent=styles['Normal'],
            fontSize=10,
            spaceAfter=6,
            leftIndent=10,
            rightIndent=10,
            backColor=colors.HexColor('#f5f5f5'),
            borderPadding=8
        )
        
        elements = []
        
        # === EN-TÊTE AVEC LOGO  ===
        logo_path = os.path.join(img_dir, "logo.jpg")
        # header_path = os.path.join(img_dir, "header.jpg")
        
        # Logo centré en haut de la page
        if os.path.exists(logo_path):
            logo_img = Image(logo_path, width=4*cm, height=4*cm)
            logo_img.hAlign = 'CENTER'
            elements.append(logo_img)
            elements.append(Spacer(1, 10))
        
        """# Header si présent
                                if os.path.exists(header_path):
                                    header_img = Image(header_path, width=16*cm, height=1.4*cm)
                                    header_img.hAlign = 'CENTER'
                                    elements.append(header_img)
                                    elements.append(Spacer(1, 10))"""
        
        # Ligne de séparation orange/bleue
        line_data = [['', '']]
        line_table = Table(line_data, colWidths=[9*cm, 8*cm])
        line_table.setStyle(TableStyle([
            ('BACKGROUND', (0, 0), (0, 0), colors.HexColor('#e65c00')),
            ('BACKGROUND', (1, 0), (1, 0), colors.HexColor('#003d80')),
        ]))
        elements.append(line_table)
        elements.append(Spacer(1, 15))
        
        # Titre (personnalisé ou par défaut)
        pdf_title = mission_data.get('title', 'RAPPORT DE MISSION SATER')
        elements.append(Paragraph(pdf_title.upper(), title_style))
        pdf_subject = mission_data.get('subject', 'Recherche de balise de détresse')
        elements.append(Paragraph(pdf_subject, subtitle_style))
        
        # Commentaire si présent
        if mission_data.get('comment'):
            elements.append(Paragraph("📝 Commentaires / Notes", section_style))
            # Remplacer les retours à la ligne par des <br/>
            comment_text = mission_data['comment'].replace('\n', '<br/>')
            elements.append(Paragraph(comment_text, comment_style))
            elements.append(Spacer(1, 15))
        
        # Information mission
        elements.append(Paragraph("📋 Informations de la mission", section_style))
        
        mission_info = [
            ["Date de début:", mission_data.get('start_date', 'N/A')],
            ["Heure de début:", mission_data.get('start_time', 'N/A')],
            ["Durée:", mission_data.get('duration', 'N/A')],
            ["Nombre de stations:", str(mission_data.get('station_count', 0))],
            ["Nombre de relevés:", str(mission_data.get('record_count', 0))],
        ]
        
        t = Table(mission_info, colWidths=[5*cm, 10*cm])
        t.setStyle(TableStyle([
            ('BACKGROUND', (0, 0), (0, -1), colors.HexColor('#fff3e6')),
            ('FONTNAME', (0, 0), (0, -1), 'Helvetica-Bold'),
            ('FONTSIZE', (0, 0), (-1, -1), 10),
            ('PADDING', (0, 0), (-1, -1), 8),
            ('GRID', (0, 0), (-1, -1), 0.5, colors.HexColor('#e65c00')),
        ]))
        elements.append(t)
        elements.append(Spacer(1, 15))
        
        # Stations
        if mission_data.get('stations'):
            elements.append(Paragraph("📍 Stations de radiogoniométrie", section_style))
            
            station_data = [["Indicatif", "Position (DMS)", "Azimut", "Signal"]]
            for s in mission_data['stations']:
                lat_dms = dd_to_dms_str(s['lat'], 'lat')
                lon_dms = dd_to_dms_str(s['lon'], 'lon')
                station_data.append([
                    s['callsign'],
                    f"{lat_dms}\n{lon_dms}",
                    f"{s['azimuth']}° ±{s['uncertainty']}°",
                    s.get('signal', 'S5')
                ])
            
            t = Table(station_data, colWidths=[3*cm, 6*cm, 4*cm, 2*cm])
            t.setStyle(TableStyle([
                ('BACKGROUND', (0, 0), (-1, 0), colors.HexColor('#e65c00')),
                ('TEXTCOLOR', (0, 0), (-1, 0), colors.white),
                ('FONTNAME', (0, 0), (-1, 0), 'Helvetica-Bold'),
                ('FONTSIZE', (0, 0), (-1, -1), 9),
                ('PADDING', (0, 0), (-1, -1), 6),
                ('GRID', (0, 0), (-1, -1), 0.5, colors.HexColor('#bdc3c7')),
                ('ALIGN', (2, 0), (-1, -1), 'CENTER'),
                ('VALIGN', (0, 0), (-1, -1), 'MIDDLE'),
            ]))
            elements.append(t)
            elements.append(Spacer(1, 15))
        
        # Kilomètres parcourus
        if mission_data.get('kilometers') and len(mission_data['kilometers']) > 0:
            elements.append(Paragraph("🚗 Kilomètres parcourus", section_style))
            
            km_data = [["Station", "Distance"]]
            total_km = 0.0
            for callsign, km in mission_data['kilometers'].items():
                km_data.append([callsign, f"{km:.1f} km"])
                total_km += km
            km_data.append(["TOTAL", f"{total_km:.1f} km"])
            
            t = Table(km_data, colWidths=[6*cm, 4*cm])
            t.setStyle(TableStyle([
                ('BACKGROUND', (0, 0), (-1, 0), colors.HexColor('#27ae60')),
                ('TEXTCOLOR', (0, 0), (-1, 0), colors.white),
                ('FONTNAME', (0, 0), (-1, 0), 'Helvetica-Bold'),
                ('BACKGROUND', (0, -1), (-1, -1), colors.HexColor('#e8f8f0')),
                ('FONTNAME', (0, -1), (-1, -1), 'Helvetica-Bold'),
                ('FONTSIZE', (0, 0), (-1, -1), 10),
                ('PADDING', (0, 0), (-1, -1), 6),
                ('GRID', (0, 0), (-1, -1), 0.5, colors.HexColor('#27ae60')),
                ('ALIGN', (1, 0), (1, -1), 'RIGHT'),
            ]))
            elements.append(t)
            elements.append(Spacer(1, 15))
        
        # Zone d'intersection
        if mission_data.get('intersection'):
            elements.append(Paragraph("🎯 Zone d'intersection", section_style))
            
            inter = mission_data['intersection']
            lat_dms = dd_to_dms_str(inter['center_lat'], 'lat')
            lon_dms = dd_to_dms_str(inter['center_lon'], 'lon')
            
            inter_info = [
                ["Centre (DD):", f"{inter['center_lat']:.6f}, {inter['center_lon']:.6f}"],
                ["Centre (DMS):", f"{lat_dms} {lon_dms}"],
                ["MGRS:", inter.get('mgrs', 'N/A')],
                ["Rayon:", f"{inter['radius_km']:.2f} km"],
                ["Surface:", f"{inter['surface_km2']:.2f} km²"],
                ["Points d'intersection:", str(inter.get('point_count', 0))],
            ]
            
            t = Table(inter_info, colWidths=[5*cm, 10*cm])
            t.setStyle(TableStyle([
                ('BACKGROUND', (0, 0), (0, -1), colors.HexColor('#e6f3ff')),
                ('FONTNAME', (0, 0), (0, -1), 'Helvetica-Bold'),
                ('FONTSIZE', (0, 0), (-1, -1), 10),
                ('PADDING', (0, 0), (-1, -1), 8),
                ('GRID', (0, 0), (-1, -1), 0.5, colors.HexColor('#003d80')),
            ]))
            elements.append(t)
            elements.append(Spacer(1, 15))
        
        # Position de la balise
        if mission_data.get('beacon'):
            elements.append(Paragraph("🎯 Position de la balise de détresse", section_style))
            
            beacon = mission_data['beacon']
            lat_dms = dd_to_dms_str(beacon['lat'], 'lat')
            lon_dms = dd_to_dms_str(beacon['lon'], 'lon')
            
            beacon_info = [
                ["Coordonnées (DD):", f"{beacon['lat']:.6f}, {beacon['lon']:.6f}"],
                ["Coordonnées (DMS):", f"{lat_dms} {lon_dms}"],
                ["MGRS:", beacon.get('mgrs', 'N/A')],
            ]
            
            t = Table(beacon_info, colWidths=[5*cm, 10*cm])
            t.setStyle(TableStyle([
                ('BACKGROUND', (0, 0), (0, -1), colors.HexColor('#ffe6e6')),
                ('FONTNAME', (0, 0), (0, -1), 'Helvetica-Bold'),
                ('FONTSIZE', (0, 0), (-1, -1), 10),
                ('PADDING', (0, 0), (-1, -1), 8),
                ('GRID', (0, 0), (-1, -1), 0.5, colors.HexColor('#ff0000')),
            ]))
            elements.append(t)
            elements.append(Spacer(1, 15))
        
        # Capture de la carte
        if mission_data.get('map_image') and os.path.exists(mission_data['map_image']):
            elements.append(Paragraph("🗺️ Carte de la mission", section_style))
            try:
                # Charger l'image pour obtenir ses dimensions
                from PIL import Image as PILImage
                pil_img = PILImage.open(mission_data['map_image'])
                img_width, img_height = pil_img.size
                aspect_ratio = img_height / img_width
                
                # Largeur max 17cm, calculer la hauteur proportionnelle
                max_width = 17 * cm
                max_height = 12 * cm
                
                # Calculer les dimensions en gardant le ratio
                if img_width / img_height > max_width / max_height:
                    # Image plus large que haute - limiter par la largeur
                    final_width = max_width
                    final_height = max_width * aspect_ratio
                else:
                    # Image plus haute que large - limiter par la hauteur
                    final_height = max_height
                    final_width = max_height / aspect_ratio
                
                map_img = Image(mission_data['map_image'], width=final_width, height=final_height)
                map_img.hAlign = 'CENTER'
                elements.append(map_img)
                elements.append(Spacer(1, 15))
            except Exception as e:
                elements.append(Paragraph(f"<i>Erreur lors du chargement de la carte: {e}</i>", normal_style))
        
        # Historique des relevés
        if mission_data.get('history') and len(mission_data['history']) > 0:
            elements.append(Paragraph("📜 Historique des relevés", section_style))
            
            history_data = [["Date/Heure", "Station", "Azimut", "Signal", "Position"]]
            for h in mission_data['history'][-20:]:  # Limiter aux 20 derniers
                lat_dms = dd_to_dms_str(h['lat'], 'lat')
                lon_dms = dd_to_dms_str(h['lon'], 'lon')
                history_data.append([
                    h['timestamp'],
                    h['callsign'],
                    f"{h['azimuth']}° ±{h['uncertainty']}°",
                    h.get('signal', 'S5'),
                    f"{lat_dms}\n{lon_dms}"
                ])
            
            t = Table(history_data, colWidths=[3*cm, 2*cm, 3*cm, 1.5*cm, 5*cm])
            t.setStyle(TableStyle([
                ('BACKGROUND', (0, 0), (-1, 0), colors.HexColor('#003d80')),
                ('TEXTCOLOR', (0, 0), (-1, 0), colors.white),
                ('FONTNAME', (0, 0), (-1, 0), 'Helvetica-Bold'),
                ('FONTSIZE', (0, 0), (-1, -1), 8),
                ('PADDING', (0, 0), (-1, -1), 4),
                ('GRID', (0, 0), (-1, -1), 0.5, colors.HexColor('#bdc3c7')),
                ('ALIGN', (0, 0), (-1, -1), 'CENTER'),
                ('VALIGN', (0, 0), (-1, -1), 'MIDDLE'),
            ]))
            elements.append(t)
            
            if len(mission_data['history']) > 20:
                elements.append(Paragraph(
                    f"<i>... et {len(mission_data['history']) - 20} autres relevés</i>",
                    normal_style
                ))
        
        # Pied de page
        elements.append(Spacer(1, 25))
        
        # Ligne de séparation
        line_data2 = [['', '']]
        line_table2 = Table(line_data2, colWidths=[9*cm, 8*cm])
        line_table2.setStyle(TableStyle([
            ('BACKGROUND', (0, 0), (0, 0), colors.HexColor('#e65c00')),
            ('BACKGROUND', (1, 0), (1, 0), colors.HexColor('#003d80')),
        ]))
        elements.append(line_table2)
        elements.append(Spacer(1, 8))
        
        footer_style = ParagraphStyle(
            'FooterStyle',
            parent=styles['Normal'],
            fontSize=8,
            alignment=TA_CENTER,
            textColor=colors.HexColor('#95a5a6')
        )
        elements.append(Paragraph(
            f"ADRASEC 06 - Rapport généré par SATER Map v{APP_VERSION} - {datetime.now().strftime('%d/%m/%Y %H:%M')}",
            footer_style
        ))
        
        doc.build(elements)
        return True
        
    except Exception as e:
        print(f"Erreur génération PDF: {e}")
        import traceback
        traceback.print_exc()
        return False


class StationRowData:
    """Stocke les widgets d'une ligne de station dans le tableau"""
    def __init__(self):
        self.station_id = None  # Identifiant unique technique
        self.callsign_edit = None
        self.lat_deg = None
        self.lat_min = None
        self.lat_sec = None
        self.lat_dir = None
        self.lon_deg = None
        self.lon_min = None
        self.lon_sec = None
        self.lon_dir = None
        self.azimuth_spin = None
        self.uncertainty_spin = None
        self.signal_combo = None
        self.color_btn = None
        self.visible_check = None
        self.record_btn = None  # Bouton pour enregistrer dans l'historique
        self.delete_btn = None
        self._color = "#e74c3c"
        self._updating = False


class StationsTableWidget(QTableWidget):
    """Tableau des stations avec en-têtes alignés"""
    stationDataChanged = pyqtSignal()
    stationDeleted = pyqtSignal(int)
    # Signal émis quand l'utilisateur clique sur le bouton d'enregistrement
    # (callsign, azimuth, uncertainty, lat, lon, signal)
    recordingRequested = pyqtSignal(str, float, float, float, float, str)
    
    COLUMNS = ["Indicatif", "Lat °", "Lat '", "Lat \"", "N/S", 
               "Lon °", "Lon '", "Lon \"", "E/W", "Signal", "Azimut", "±", 
               "🎨", "👁", "📝", "🗑"]
    
    def __init__(self, parent=None):
        super().__init__(parent)
        self.station_rows: List[StationRowData] = []
        self._setup_table()
        
    def _record_station(self, row: int):
        """Enregistre la station dans l'historique"""
        station = self.get_station(row)
        if station:
            self.recordingRequested.emit(
                station.callsign,
                station.azimuth,
                station.uncertainty,
                station.lat,
                station.lon,
                station.signal
            )
        
    def _setup_table(self):
        self.setColumnCount(len(self.COLUMNS))
        self.setHorizontalHeaderLabels(self.COLUMNS)
        self.setSelectionBehavior(QTableWidget.SelectionBehavior.SelectRows)
        self.setSelectionMode(QTableWidget.SelectionMode.SingleSelection)
        self.verticalHeader().setVisible(False)
        self.setAlternatingRowColors(True)
        
        # Largeurs minimales des colonnes (avec bouton record)
        col_widths = [70, 50, 45, 55, 40, 50, 45, 55, 40, 60, 55, 60, 30, 30, 30, 30]
        for i, w in enumerate(col_widths):
            self.setColumnWidth(i, w)
        
        # Permettre au tableau de s'étendre
        self.horizontalHeader().setStretchLastSection(True)
        # Mode interactif pour permettre le redimensionnement manuel mais avec étirement
        self.horizontalHeader().setSectionResizeMode(QHeaderView.ResizeMode.Interactive)
        # La colonne Indicatif s'étend pour prendre l'espace supplémentaire
        self.horizontalHeader().setSectionResizeMode(0, QHeaderView.ResizeMode.Stretch)
        
        self.setSizePolicy(QSizePolicy.Policy.Expanding, QSizePolicy.Policy.Expanding)
        
    def add_station(self, color: str = None) -> int:
        """Ajoute une nouvelle station et retourne l'index de la ligne"""
        import uuid
        
        if color is None:
            color = STATION_COLORS[len(self.station_rows) % len(STATION_COLORS)]
        
        row = self.rowCount()
        self.insertRow(row)
        self.setRowHeight(row, 30)
        
        data = StationRowData()
        data._color = color
        data.station_id = str(uuid.uuid4())[:8]  # Générer un ID unique
        
        # Col 0: Indicatif
        data.callsign_edit = QLineEdit()
        data.callsign_edit.setPlaceholderText("Indicatif")
        data.callsign_edit.textChanged.connect(self._emit_change)
        self.setCellWidget(row, 0, data.callsign_edit)
        
        # Col 1-4: Latitude
        data.lat_deg = QSpinBox()
        data.lat_deg.setRange(0, 90)
        data.lat_deg.setButtonSymbols(QSpinBox.ButtonSymbols.NoButtons)
        data.lat_deg.valueChanged.connect(self._emit_change)
        self.setCellWidget(row, 1, data.lat_deg)
        
        data.lat_min = QSpinBox()
        data.lat_min.setRange(0, 59)
        data.lat_min.setButtonSymbols(QSpinBox.ButtonSymbols.NoButtons)
        data.lat_min.valueChanged.connect(self._emit_change)
        self.setCellWidget(row, 2, data.lat_min)
        
        data.lat_sec = QDoubleSpinBox()
        data.lat_sec.setRange(0, 59.99)
        data.lat_sec.setDecimals(1)
        data.lat_sec.setButtonSymbols(QSpinBox.ButtonSymbols.NoButtons)
        data.lat_sec.valueChanged.connect(self._emit_change)
        self.setCellWidget(row, 3, data.lat_sec)
        
        data.lat_dir = QComboBox()
        data.lat_dir.addItems(["N", "S"])
        data.lat_dir.currentIndexChanged.connect(self._emit_change)
        self.setCellWidget(row, 4, data.lat_dir)
        
        # Col 5-8: Longitude
        data.lon_deg = QSpinBox()
        data.lon_deg.setRange(0, 180)
        data.lon_deg.setButtonSymbols(QSpinBox.ButtonSymbols.NoButtons)
        data.lon_deg.valueChanged.connect(self._emit_change)
        self.setCellWidget(row, 5, data.lon_deg)
        
        data.lon_min = QSpinBox()
        data.lon_min.setRange(0, 59)
        data.lon_min.setButtonSymbols(QSpinBox.ButtonSymbols.NoButtons)
        data.lon_min.valueChanged.connect(self._emit_change)
        self.setCellWidget(row, 6, data.lon_min)
        
        data.lon_sec = QDoubleSpinBox()
        data.lon_sec.setRange(0, 59.99)
        data.lon_sec.setDecimals(1)
        data.lon_sec.setButtonSymbols(QSpinBox.ButtonSymbols.NoButtons)
        data.lon_sec.valueChanged.connect(self._emit_change)
        self.setCellWidget(row, 7, data.lon_sec)
        
        data.lon_dir = QComboBox()
        data.lon_dir.addItems(["E", "W"])
        data.lon_dir.currentIndexChanged.connect(self._emit_change)
        self.setCellWidget(row, 8, data.lon_dir)
        
        # Col 9: Signal (avant Azimut)
        data.signal_combo = QComboBox()
        data.signal_combo.addItems(SIGNAL_LEVELS)
        data.signal_combo.setCurrentText("S5")
        data.signal_combo.currentIndexChanged.connect(self._emit_change)
        self.setCellWidget(row, 9, data.signal_combo)
        
        # Col 10: Azimut
        data.azimuth_spin = QSpinBox()
        data.azimuth_spin.setRange(0, 359)
        data.azimuth_spin.setSuffix("°")
        data.azimuth_spin.valueChanged.connect(self._emit_change)
        self.setCellWidget(row, 10, data.azimuth_spin)
        
        # Col 11: Incertitude
        data.uncertainty_spin = QDoubleSpinBox()
        data.uncertainty_spin.setRange(0, 30)
        data.uncertainty_spin.setDecimals(1)
        data.uncertainty_spin.setValue(5.0)
        data.uncertainty_spin.setPrefix("±")
        data.uncertainty_spin.setSuffix("°")
        data.uncertainty_spin.setButtonSymbols(QSpinBox.ButtonSymbols.NoButtons)
        data.uncertainty_spin.valueChanged.connect(self._emit_change)
        self.setCellWidget(row, 11, data.uncertainty_spin)
        
        # Col 12: Couleur
        data.color_btn = QPushButton()
        data.color_btn.setStyleSheet(f"background-color: {color}; border: 1px solid #666;")
        data.color_btn.clicked.connect(lambda checked, r=row: self._choose_color(r))
        self.setCellWidget(row, 12, data.color_btn)
        
        # Col 13: Visible
        visible_widget = QWidget()
        visible_layout = QHBoxLayout(visible_widget)
        visible_layout.setContentsMargins(0, 0, 0, 0)
        visible_layout.setAlignment(Qt.AlignmentFlag.AlignCenter)
        data.visible_check = QCheckBox()
        data.visible_check.setChecked(True)
        data.visible_check.stateChanged.connect(self._emit_change)
        visible_layout.addWidget(data.visible_check)
        self.setCellWidget(row, 13, visible_widget)
        
        # Col 14: Enregistrer dans l'historique
        data.record_btn = QPushButton("📝")
        data.record_btn.setToolTip("Enregistrer ce relevé dans l'historique")
        data.record_btn.clicked.connect(lambda checked, r=row: self._record_station(r))
        self.setCellWidget(row, 14, data.record_btn)
        
        # Col 15: Supprimer
        data.delete_btn = QPushButton()
        data.delete_btn.setIcon(self.style().standardIcon(QStyle.StandardPixmap.SP_TrashIcon))
        data.delete_btn.clicked.connect(lambda checked, r=row: self._delete_station(r))
        self.setCellWidget(row, 15, data.delete_btn)
        
        self.station_rows.append(data)
        return row
    
    def _emit_change(self):
        """Émet le signal de changement"""
        # Vérifier qu'on n'est pas en mode mise à jour
        for data in self.station_rows:
            if data._updating:
                return
        self.stationDataChanged.emit()
    
    def _choose_color(self, row: int):
        """Ouvre le dialogue de choix de couleur"""
        if row >= len(self.station_rows):
            return
        data = self.station_rows[row]
        color = QColorDialog.getColor(QColor(data._color), self)
        if color.isValid():
            data._color = color.name()
            data.color_btn.setStyleSheet(f"background-color: {data._color}; border: 1px solid #666;")
            self.stationDataChanged.emit()
    
    def _delete_station(self, row: int):
        """Supprime une station"""
        if row >= len(self.station_rows):
            return
        self.removeRow(row)
        self.station_rows.pop(row)
        # Mettre à jour les références des lambdas pour les lignes suivantes
        for i in range(row, len(self.station_rows)):
            data = self.station_rows[i]
            # Reconnecter les signaux avec le bon index
            data.color_btn.clicked.disconnect()
            data.color_btn.clicked.connect(lambda checked, r=i: self._choose_color(r))
            data.record_btn.clicked.disconnect()
            data.record_btn.clicked.connect(lambda checked, r=i: self._record_station(r))
            data.delete_btn.clicked.disconnect()
            data.delete_btn.clicked.connect(lambda checked, r=i: self._delete_station(r))
        self.stationDeleted.emit(row)
        self.stationDataChanged.emit()
    
    def get_station(self, row: int) -> Optional[Station]:
        """Récupère les données d'une station"""
        if row >= len(self.station_rows):
            return None
        data = self.station_rows[row]
        callsign = data.callsign_edit.text().strip()
        if not callsign:
            return None
        try:
            lat = dms_to_dd(data.lat_deg.value(), data.lat_min.value(),
                           data.lat_sec.value(), data.lat_dir.currentText())
            lon = dms_to_dd(data.lon_deg.value(), data.lon_min.value(),
                           data.lon_sec.value(), data.lon_dir.currentText())
            return Station(
                callsign=callsign, lat=lat, lon=lon,
                azimuth=data.azimuth_spin.value(),
                uncertainty=data.uncertainty_spin.value(),
                color=data._color,
                visible=data.visible_check.isChecked(),
                signal=data.signal_combo.currentText(),
                station_id=data.station_id  # Inclure l'ID unique
            )
        except:
            return None
    
    def get_all_stations(self) -> List[Station]:
        """Récupère toutes les stations"""
        stations = []
        for i in range(len(self.station_rows)):
            station = self.get_station(i)
            if station:
                stations.append(station)
        return stations
    
    def set_station(self, row: int, station: Station):
        """Définit les données d'une station"""
        if row >= len(self.station_rows):
            return
        data = self.station_rows[row]
        data._updating = True
        
        # Conserver le station_id si fourni dans l'objet Station
        if hasattr(station, 'station_id') and station.station_id:
            data.station_id = station.station_id
        
        data.callsign_edit.setText(station.callsign)
        lat_dms = dd_to_dms(station.lat, "lat")
        data.lat_deg.setValue(lat_dms[0])
        data.lat_min.setValue(lat_dms[1])
        data.lat_sec.setValue(lat_dms[2])
        data.lat_dir.setCurrentText(lat_dms[3])
        
        lon_dms = dd_to_dms(station.lon, "lon")
        data.lon_deg.setValue(lon_dms[0])
        data.lon_min.setValue(lon_dms[1])
        data.lon_sec.setValue(lon_dms[2])
        data.lon_dir.setCurrentText(lon_dms[3])
        
        data.azimuth_spin.setValue(int(station.azimuth))
        data.uncertainty_spin.setValue(station.uncertainty)
        data._color = station.color
        data.color_btn.setStyleSheet(f"background-color: {data._color}; border: 1px solid #666;")
        data.visible_check.setChecked(station.visible)
        if hasattr(station, 'signal'):
            data.signal_combo.setCurrentText(station.signal)
        
        data._updating = False
    
    def set_coordinates(self, row: int, lat: float, lon: float):
        """Met à jour les coordonnées d'une station (drag & drop)"""
        if row >= len(self.station_rows):
            return
        data = self.station_rows[row]
        data._updating = True
        
        lat_dms = dd_to_dms(lat, "lat")
        data.lat_deg.setValue(lat_dms[0])
        data.lat_min.setValue(lat_dms[1])
        data.lat_sec.setValue(lat_dms[2])
        data.lat_dir.setCurrentText(lat_dms[3])
        
        lon_dms = dd_to_dms(lon, "lon")
        data.lon_deg.setValue(lon_dms[0])
        data.lon_min.setValue(lon_dms[1])
        data.lon_sec.setValue(lon_dms[2])
        data.lon_dir.setCurrentText(lon_dms[3])
        
        data._updating = False
        self.stationDataChanged.emit()
    
    def clear_all(self):
        """Supprime toutes les stations"""
        while self.rowCount() > 0:
            self.removeRow(0)
        self.station_rows.clear()
        self.stationDataChanged.emit()
    
    def get_row_by_callsign(self, callsign: str) -> int:
        """Trouve l'index d'une station par son indicatif"""
        for i, data in enumerate(self.station_rows):
            if data.callsign_edit.text().strip() == callsign:
                return i
        return -1
    
    def get_row_by_station_id(self, station_id: str) -> int:
        """Trouve l'index d'une station par son ID technique unique"""
        for i, data in enumerate(self.station_rows):
            if data.station_id == station_id:
                return i
        return -1


class MissionTimer(QWidget):
    def __init__(self, parent=None):
        super().__init__(parent)
        self.start_time = None
        self.elapsed = timedelta()
        self.running = False
        
        layout = QHBoxLayout(self)
        layout.setContentsMargins(5, 5, 5, 5)
        layout.setSpacing(8)
        
        # LCD compact
        self.lcd = QLCDNumber()
        self.lcd.setDigitCount(8)
        self.lcd.setSegmentStyle(QLCDNumber.SegmentStyle.Flat)
        self.lcd.setStyleSheet("background-color: #1a1a2e; color: #00ff00;")
        self.lcd.setFixedHeight(35)
        self.lcd.setMinimumWidth(100)
        self.lcd.display("00:00:00")
        layout.addWidget(self.lcd, 2)  # stretch factor 1
        
        # Boutons compacts
        self.start_btn = QPushButton("▶")
        # self.start_btn.setFixedWidth(35)
        self.start_btn.setToolTip("Démarrer")
        self.start_btn.clicked.connect(self.start)
        layout.addWidget(self.start_btn, 3)
        
        self.stop_btn = QPushButton("⏹")
        # self.stop_btn.setFixedWidth(35)
        self.stop_btn.setToolTip("Arrêter")
        self.stop_btn.clicked.connect(self.stop)
        self.stop_btn.setEnabled(False)
        layout.addWidget(self.stop_btn, 3)
        
        self.reset_btn = QPushButton("↺")
        # self.reset_btn.setFixedWidth(35)
        self.reset_btn.setToolTip("Reset")
        self.reset_btn.clicked.connect(self.reset)
        layout.addWidget(self.reset_btn, 3)
        
        # Label heure de début - s'étend pour remplir l'espace
        self.start_label = QLabel("")
        self.start_label.setStyleSheet("color: #666; font-size: 10px;")
        layout.addWidget(self.start_label, 1)  # stretch factor 1
        
        self.timer = QTimer()
        self.timer.timeout.connect(self.update_display)
    
    def start(self):
        if not self.running:
            if self.start_time is None:
                self.start_time = datetime.now()
                self.start_label.setText(f"Début: {self.start_time.strftime('%H:%M:%S')}")
            self.running = True
            self.timer.start(1000)
            self.start_btn.setEnabled(False)
            self.stop_btn.setEnabled(True)
    
    def stop(self):
        if self.running:
            self.running = False
            self.timer.stop()
            self.start_btn.setEnabled(True)
            self.stop_btn.setEnabled(False)
    
    def reset(self):
        self.stop()
        self.start_time = None
        self.elapsed = timedelta()
        self.lcd.display("00:00:00")
        self.start_label.setText("")
        self.start_btn.setEnabled(True)
    
    def update_display(self):
        if self.running and self.start_time:
            self.elapsed = datetime.now() - self.start_time
            hours, remainder = divmod(int(self.elapsed.total_seconds()), 3600)
            minutes, seconds = divmod(remainder, 60)
            self.lcd.display(f"{hours:02d}:{minutes:02d}:{seconds:02d}")
    
    def get_elapsed_str(self) -> str:
        hours, remainder = divmod(int(self.elapsed.total_seconds()), 3600)
        minutes, seconds = divmod(remainder, 60)
        return f"{hours:02d}:{minutes:02d}:{seconds:02d}"
    
    def get_start_time(self) -> Optional[datetime]:
        return self.start_time


class MainWindow(QMainWindow):
    def __init__(self):
        super().__init__()
        self.azimuth_length_km = AZIMUTH_LENGTH_KM
        self.show_intersection = True
        self.show_utm_grid = False
        self.offline_mode = False
        self.current_provider = 'osm'
        self.map_ready = False
        self.temp_dir = None
        self.tiles_dir = os.path.abspath(TILES_DIR)
        self.azimuth_history: List[AzimuthRecord] = []
        self.station_kilometers: Dict[str, float] = {}  # indicatif -> km parcourus
        self.beacon_position: Optional[Tuple[float, float]] = None  # (lat, lon) de la balise
        self.last_intersection_center: Optional[Tuple[float, float]] = None  # Pour centrer sur la zone
        
        # Position et zoom de la carte (centré sur la France au démarrage)
        self.map_center_lat = 46.6
        self.map_center_lon = 2.5
        self.map_zoom = 6
        
        # Couleurs personnalisables
        self.grid_color = "#00ff00"
        self.zone_border_color = "#f44336"
        self.zone_fill_color = "#ffeb3b"
        self.zone_opacity = 0.3
        
        self._setup_ui()
        self._setup_menu()
        self._connect_signals()

        self.update_timer = QTimer()
        self.update_timer.setSingleShot(True)
        self.update_timer.timeout.connect(self._do_update_map)
        
        self.drag_check_timer = QTimer()
        self.drag_check_timer.timeout.connect(self._check_drag)
        
        # Initialiser QWebEngineView avec un délai pour éviter les segfaults
        QTimer.singleShot(100, self._init_map_view)
        QTimer.singleShot(200, self.showMaximized)

    def _init_map_view(self):
        """Initialise QWebEngineView avec un délai pour éviter les erreurs de segmentation"""
        try:
            self.map_view = QWebEngineView()
            self.map_view.setMinimumWidth(500)
            
            # Remplacer le label de chargement par la carte
            self.loading_label.setParent(None)
            self.loading_label.deleteLater()
            self.map_container_layout.addWidget(self.map_view)
            
            # Maintenant configurer la carte
            self._setup_map()
            
            # Démarrer le timer de vérification du drag
            self.drag_check_timer.start(200)
        except Exception as e:
            print(f"Erreur lors de l'initialisation de la carte: {e}")

    def closeEvent(self, event):
        self.drag_check_timer.stop()
        if self.temp_dir and os.path.exists(self.temp_dir):
            shutil.rmtree(self.temp_dir)
        event.accept()

    def _setup_ui(self):
        self.setWindowTitle(f"{APP_NAME} v{APP_VERSION}")
        if os.path.exists(ICON_PATH):
            self.setWindowIcon(QIcon(ICON_PATH))
        self.setMinimumSize(1200, 700)

        central_widget = QWidget()
        self.setCentralWidget(central_widget)
        main_layout = QHBoxLayout(central_widget)
        splitter = QSplitter(Qt.Orientation.Horizontal)
        main_layout.addWidget(splitter)

        left_panel = QWidget()
        left_layout = QVBoxLayout(left_panel)

        # Chronomètre
        timer_group = QGroupBox("Chronomètre Mission")
        timer_layout = QHBoxLayout(timer_group)  # Horizontal pour s'étendre
        self.mission_timer = MissionTimer()
        timer_layout.addWidget(self.mission_timer, 1)  # stretch factor 1
        left_layout.addWidget(timer_group)

        btn_layout = QHBoxLayout()
        self.add_btn = QPushButton("➕ Ajouter une station")
        btn_layout.addWidget(self.add_btn)
        self.clear_btn = QPushButton("🗑️ Tout effacer")
        btn_layout.addWidget(self.clear_btn)
        left_layout.addLayout(btn_layout)

        # Tableau des stations avec en-têtes intégrés
        self.stations_table = StationsTableWidget()
        self.stations_table.setMinimumHeight(150)
        left_layout.addWidget(self.stations_table)

        # Paramètres carte (horizontal)
        settings_group = QGroupBox("Paramètres carte")
        settings_layout = QHBoxLayout(settings_group)
        
        settings_layout.addWidget(QLabel("Longueur:"))
        self.length_spin = QSpinBox()
        self.length_spin.setRange(10, 500)
        self.length_spin.setValue(AZIMUTH_LENGTH_KM)
        self.length_spin.setSuffix(" km")
        settings_layout.addWidget(self.length_spin)
        
        settings_layout.addWidget(QLabel("Fond:"))
        self.tiles_combo = QComboBox()
        self.tiles_combo.setSizePolicy(QSizePolicy.Policy.Expanding, QSizePolicy.Policy.Fixed)
        for key, provider in TILE_PROVIDERS.items():
            self.tiles_combo.addItem(provider['name'], key)
        settings_layout.addWidget(self.tiles_combo, 1)  # stretch factor 1
        
        self.offline_check = QCheckBox("Hors-ligne")
        settings_layout.addWidget(self.offline_check)

        left_layout.addWidget(settings_group)

        # Zone d'intersection (horizontal)
        zone_group = QGroupBox("Zone d'intersection")
        zone_layout = QHBoxLayout(zone_group)

        self.intersection_check = QCheckBox("Afficher")
        self.intersection_check.setChecked(True)
        zone_layout.addWidget(self.intersection_check)

        zone_layout.addWidget(QLabel("Bordure:"))
        self.zone_border_btn = QPushButton()
        self.zone_border_btn.setFixedWidth(30)
        self.zone_border_btn.setStyleSheet(f"background-color: {self.zone_border_color};")
        self.zone_border_btn.setToolTip("Couleur bordure")
        self.zone_border_btn.clicked.connect(self._choose_zone_border_color)
        zone_layout.addWidget(self.zone_border_btn)
        
        zone_layout.addWidget(QLabel("Fond:"))
        self.zone_fill_btn = QPushButton()
        self.zone_fill_btn.setFixedWidth(30)
        self.zone_fill_btn.setStyleSheet(f"background-color: {self.zone_fill_color};")
        self.zone_fill_btn.setToolTip("Couleur remplissage")
        self.zone_fill_btn.clicked.connect(self._choose_zone_fill_color)
        zone_layout.addWidget(self.zone_fill_btn)

        zone_layout.addWidget(QLabel("Opacité:"))
        self.zone_opacity_slider = QSlider(Qt.Orientation.Horizontal)
        self.zone_opacity_slider.setRange(0, 100)
        self.zone_opacity_slider.setValue(30)
        self.zone_opacity_slider.valueChanged.connect(self._on_zone_opacity_changed)
        zone_layout.addWidget(self.zone_opacity_slider, 1)  # stretch factor 1
        self.zone_opacity_label = QLabel("30%")
        self.zone_opacity_label.setFixedWidth(35)
        zone_layout.addWidget(self.zone_opacity_label)

        left_layout.addWidget(zone_group)

        # Grille UTM/MGRS (horizontal)
        grid_group = QGroupBox("Grille UTM/MGRS")
        grid_layout = QHBoxLayout(grid_group)

        self.utm_grid_check = QCheckBox("Afficher")
        grid_layout.addWidget(self.utm_grid_check)

        grid_layout.addWidget(QLabel("Couleur:"))
        self.grid_color_btn = QPushButton()
        self.grid_color_btn.setFixedWidth(30)
        self.grid_color_btn.setStyleSheet(f"background-color: {self.grid_color};")
        self.grid_color_btn.clicked.connect(self._choose_grid_color)
        grid_layout.addWidget(self.grid_color_btn)
        grid_layout.addStretch(1)  # Stretch pour remplir l'espace

        left_layout.addWidget(grid_group)

        # Informations
        info_group = QGroupBox("Informations")
        info_layout = QVBoxLayout(info_group)
        self.intersection_label = QLabel("Zone d'intersection: -")
        self.intersection_label.setWordWrap(True)
        info_layout.addWidget(self.intersection_label)
        self.mgrs_label = QLabel("MGRS: -")
        self.mgrs_label.setWordWrap(True)
        self.mgrs_label.setStyleSheet("font-family: monospace; color: #2980b9;")
        info_layout.addWidget(self.mgrs_label)
        
        # Ligne avec les deux boutons
        buttons_row = QHBoxLayout()
        history_btn = QPushButton("📜 Historique")
        history_btn.clicked.connect(self.show_history)
        buttons_row.addWidget(history_btn)
        
        km_btn = QPushButton("🚗 Kilomètres")
        km_btn.clicked.connect(self.show_kilometers)
        buttons_row.addWidget(km_btn)
        info_layout.addLayout(buttons_row)
        
        self.history_count_label = QLabel("0 relevés")
        info_layout.addWidget(self.history_count_label)
        
        self.tiles_info_label = QLabel("")
        info_layout.addWidget(self.tiles_info_label)
        left_layout.addWidget(info_group)

        splitter.addWidget(left_panel)

        # Placeholder pour la carte (QWebEngineView sera créé avec délai)
        self.map_container = QWidget()
        self.map_container_layout = QVBoxLayout(self.map_container)
        self.map_container_layout.setContentsMargins(0, 0, 0, 0)
        
        # Label de chargement temporaire
        self.loading_label = QLabel("Chargement de la carte...")
        self.loading_label.setAlignment(Qt.AlignmentFlag.AlignCenter)
        self.loading_label.setStyleSheet("font-size: 14px; color: #666;")
        self.map_container_layout.addWidget(self.loading_label)
        
        self.map_view = None  # Sera créé après un délai
        self.map_container.setMinimumWidth(200)
        left_panel.setMinimumWidth(800)
        splitter.addWidget(self.map_container)
        splitter.setSizes([450, 750])
        
        # Garder une référence au splitter pour plus tard
        self.splitter = splitter

        self.status_bar = QStatusBar()
        self.setStatusBar(self.status_bar)
        self.status_bar.showMessage("Prêt")
        
        self._update_tiles_info()

    def _choose_grid_color(self):
        color = QColorDialog.getColor(QColor(self.grid_color), self, "Couleur de la grille")
        if color.isValid():
            self.grid_color = color.name()
            self.grid_color_btn.setStyleSheet(f"background-color: {self.grid_color};")
            if self.map_ready and self.show_utm_grid:
                self._run_js(f"setGridColor('{self.grid_color}');")

    def _choose_zone_border_color(self):
        color = QColorDialog.getColor(QColor(self.zone_border_color), self, "Couleur bordure zone")
        if color.isValid():
            self.zone_border_color = color.name()
            self.zone_border_btn.setStyleSheet(f"background-color: {self.zone_border_color};")
            self._schedule_update()

    def _choose_zone_fill_color(self):
        color = QColorDialog.getColor(QColor(self.zone_fill_color), self, "Couleur remplissage zone")
        if color.isValid():
            self.zone_fill_color = color.name()
            self.zone_fill_btn.setStyleSheet(f"background-color: {self.zone_fill_color};")
            self._schedule_update()

    def _on_zone_opacity_changed(self, value):
        self.zone_opacity = value / 100.0
        self.zone_opacity_label.setText(f"{value}%")
        self._schedule_update()

    def _update_tiles_info(self):
        total_count = 0
        for key in TILE_PROVIDERS.keys():
            provider_dir = os.path.join(self.tiles_dir, key)
            if os.path.exists(provider_dir):
                for root, dirs, files in os.walk(provider_dir):
                    total_count += len([f for f in files if f.endswith('.png')])
        
        self.tiles_info_label.setText(f"📦 Tuiles: {total_count}")

    def _setup_menu(self):
        menubar = self.menuBar()
        file_menu = menubar.addMenu("&Fichier")

        save_action = QAction("&Sauvegarder HTML", self)
        save_action.setShortcut("Ctrl+S")
        save_action.triggered.connect(self.save_map_html)
        file_menu.addAction(save_action)

        export_png = QAction("Exporter &PNG", self)
        export_png.triggered.connect(self.export_map_png)
        file_menu.addAction(export_png)

        export_kml = QAction("Exporter &KML", self)
        export_kml.triggered.connect(self.export_kml)
        file_menu.addAction(export_kml)

        export_json = QAction("Exporter &JSON", self)
        export_json.triggered.connect(self.export_json)
        file_menu.addAction(export_json)

        import_json = QAction("&Importer JSON", self)
        import_json.triggered.connect(self.import_json)
        file_menu.addAction(import_json)

        file_menu.addSeparator()
        
        export_pdf = QAction("Générer rapport &PDF", self)
        export_pdf.setShortcut("Ctrl+P")
        export_pdf.triggered.connect(self.generate_pdf_report)
        file_menu.addAction(export_pdf)

        file_menu.addSeparator()
        exit_action = QAction("&Quitter", self)
        exit_action.setShortcut("Ctrl+Q")
        exit_action.triggered.connect(self.close)
        file_menu.addAction(exit_action)

        # Menu Stations
        stations_menu = menubar.addMenu("&Stations")
        
        manage_presets = QAction("📋 &Gérer les présets", self)
        manage_presets.triggered.connect(self.manage_presets)
        stations_menu.addAction(manage_presets)
        
        load_presets = QAction("📥 &Charger présets", self)
        load_presets.triggered.connect(self.load_presets_dialog)
        stations_menu.addAction(load_presets)
        
        stations_menu.addSeparator()
        
        save_as_preset = QAction("💾 &Sauvegarder station comme préset", self)
        save_as_preset.triggered.connect(self.save_station_as_preset)
        stations_menu.addAction(save_as_preset)

        edit_menu = menubar.addMenu("&Édition")
        center_france = QAction("Centrer &France", self)
        center_france.triggered.connect(lambda: self.center_map(46.6796, 3.0761, 6))
        edit_menu.addAction(center_france)
        
        center_intersection = QAction("Centrer sur &zone d'intersection", self)
        center_intersection.triggered.connect(self.center_on_intersection)
        edit_menu.addAction(center_intersection)
        
        edit_menu.addSeparator()
        
        set_beacon_action = QAction("📍 Définir position &balise", self)
        set_beacon_action.triggered.connect(self.set_beacon_position)
        edit_menu.addAction(set_beacon_action)
        
        clear_beacon_action = QAction("🗑️ Effacer position balise", self)
        clear_beacon_action.triggered.connect(self.clear_beacon_position)
        edit_menu.addAction(clear_beacon_action)
        
        edit_menu.addSeparator()
        download_tiles_action = QAction("📥 &Télécharger les tuiles visibles", self)
        download_tiles_action.triggered.connect(self.download_visible_tiles)
        edit_menu.addAction(download_tiles_action)

        help_menu = menubar.addMenu("&Aide")
        about_action = QAction("À &propos", self)
        about_action.triggered.connect(self.show_about)
        help_menu.addAction(about_action)

    def _setup_map(self):
        settings = self.map_view.page().settings()
        settings.setAttribute(QWebEngineSettings.WebAttribute.LocalContentCanAccessRemoteUrls, True)
        settings.setAttribute(QWebEngineSettings.WebAttribute.LocalContentCanAccessFileUrls, True)
        settings.setAttribute(QWebEngineSettings.WebAttribute.JavascriptEnabled, True)

        self.map_view.loadFinished.connect(self._on_load_finished)

        self.temp_dir = tempfile.mkdtemp()
        self._reload_map()

    def _reload_map(self):
        map_file = os.path.join(self.temp_dir, "map.html")
        
        with open(map_file, 'w', encoding='utf-8') as f:
            f.write(self._generate_map_html())
        
        self.map_ready = False
        self.map_view.load(QUrl.fromLocalFile(map_file))

    def _on_load_finished(self, ok):
        if ok:
            QTimer.singleShot(500, self._check_map_ready)

    def _check_map_ready(self):
        def callback(result):
            if result:
                self.map_ready = True
                provider_name = TILE_PROVIDERS[self.current_provider]['name']
                mode = "hors-ligne" if self.offline_mode else "en ligne"
                self.status_bar.showMessage(f"Carte chargée - {provider_name} ({mode})")
                self._do_update_map()
            else:
                QTimer.singleShot(500, self._check_map_ready)
        
        self.map_view.page().runJavaScript("typeof map !== 'undefined' && map !== null", callback)

    def _generate_map_html(self) -> str:
        provider = TILE_PROVIDERS[self.current_provider]
        
        if self.offline_mode:
            tile_url = f"file://{self.tiles_dir}/{self.current_provider}/{{z}}/{{x}}/{{y}}.png"
            subdomains_js = ""
        else:
            tile_url = provider['url']
            if provider['subdomains']:
                subdomains_js = f"subdomains: {json.dumps(provider['subdomains'])},"
            else:
                subdomains_js = ""
        
        return f'''<!DOCTYPE html>
<html>
<head>
    <meta charset="utf-8">
    <link rel="stylesheet" href="https://unpkg.com/leaflet@1.9.4/dist/leaflet.css"/>
    <script src="https://unpkg.com/leaflet@1.9.4/dist/leaflet.js"></script>
    <style>
        html, body, #map {{ height: 100%; margin: 0; padding: 0; }}
        .station-marker {{ cursor: move !important; white-space: nowrap; }}
        .marker-label {{
            background: rgba(255,255,255,0.95);
            border: 2px solid #333;
            border-radius: 4px;
            padding: 3px 8px;
            font-size: 12px;
            font-weight: bold;
            box-shadow: 0 2px 5px rgba(0,0,0,0.3);
            cursor: move;
        }}
        .utm-grid-label {{
            font-family: monospace;
            font-size: 11px;
            padding: 2px 5px;
            border-radius: 3px;
            white-space: nowrap;
            text-shadow: 1px 1px 2px rgba(0,0,0,0.8);
        }}
        .leaflet-popup-content {{ font-size: 12px; }}
        .popup-coords {{ font-family: monospace; font-size: 11px; margin: 3px 0; }}
        
        /* Animation pulsante pour la balise de détresse */
        @keyframes beacon-pulse {{
            0% {{
                box-shadow: 0 0 5px #ff0000, 0 0 10px #ff0000;
                transform: scale(1);
            }}
            50% {{
                box-shadow: 0 0 20px #ff0000, 0 0 40px #ff0000, 0 0 60px #ff3333;
                transform: scale(1.1);
            }}
            100% {{
                box-shadow: 0 0 5px #ff0000, 0 0 10px #ff0000;
                transform: scale(1);
            }}
        }}
        .beacon-pulse {{
            animation: beacon-pulse 1.5s ease-in-out infinite;
        }}
    </style>
</head>
<body>
<div id="map"></div>
<script>
var map = null;
var stationMarkers = {{}};
var azimuthLines = {{}};
var azimuthCones = {{}};
var intersectionCircle = null;
var intersectionPoints = [];
var utmGridLayer = null;
var azimuthLength = 100;
var showUtmGrid = false;
var gridColor = '{self.grid_color}';

// Fonction de conversion DD vers DMS
function ddToDms(dd, isLat) {{
    var dir = isLat ? (dd >= 0 ? 'N' : 'S') : (dd >= 0 ? 'E' : 'W');
    dd = Math.abs(dd);
    var d = Math.floor(dd);
    var m = Math.floor((dd - d) * 60);
    var s = ((dd - d - m/60) * 3600).toFixed(1);
    return d + '°' + m + "'" + s + '"' + dir;
}}

map = L.map('map', {{
    center: [{self.map_center_lat}, {self.map_center_lon}],
    zoom: {self.map_zoom}
}});

L.tileLayer('{tile_url}', {{
    attribution: '{provider["attribution"]}',
    maxZoom: {provider["max_zoom"]},
    {subdomains_js}
    errorTileUrl: 'data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVR42mNk+M9QDwADhgGAWjR9awAAAABJRU5ErkJggg=='
}}).addTo(map);

map.on('contextmenu', function(e) {{
    var lat = e.latlng.lat;
    var lon = e.latlng.lng;
    var zone = Math.floor((lon + 180) / 6) + 1;
    var letters = "CDEFGHJKLMNPQRSTUVWXX";
    var letter = letters[Math.floor((lat + 80) / 8)] || '?';
    L.popup().setLatLng(e.latlng)
        .setContent('<b>Coordonnées</b><br>' +
            '<div class="popup-coords"><b>DD:</b> ' + lat.toFixed(6) + ', ' + lon.toFixed(6) + '</div>' +
            '<div class="popup-coords"><b>DMS:</b> ' + ddToDms(lat, true) + ' ' + ddToDms(lon, false) + '</div>' +
            '<div class="popup-coords"><b>UTM:</b> ' + zone + letter + '</div>')
        .openOn(map);
}});

function getMapBounds() {{
    var b = map.getBounds();
    var z = map.getZoom();
    return JSON.stringify({{
        minLat: b.getSouth(),
        maxLat: b.getNorth(),
        minLon: b.getWest(),
        maxLon: b.getEast(),
        zoom: z
    }});
}}

function calcEndpoint(lat, lon, azimuth, distKm) {{
    var R = 6371.0;
    var lat1 = lat * Math.PI / 180;
    var lon1 = lon * Math.PI / 180;
    var brng = azimuth * Math.PI / 180;
    
    var lat2 = Math.asin(
        Math.sin(lat1) * Math.cos(distKm / R) +
        Math.cos(lat1) * Math.sin(distKm / R) * Math.cos(brng)
    );
    
    var lon2 = lon1 + Math.atan2(
        Math.sin(brng) * Math.sin(distKm / R) * Math.cos(lat1),
        Math.cos(distKm / R) - Math.sin(lat1) * Math.sin(lat2)
    );
    
    return [lat2 * 180 / Math.PI, lon2 * 180 / Math.PI];
}}

function updateStations(stationsJson) {{
    var stations = JSON.parse(stationsJson);
    
    // Nettoyage complet de tous les objets Leaflet existants
    for (var key in stationMarkers) {{
        if (stationMarkers.hasOwnProperty(key)) {{
            map.removeLayer(stationMarkers[key]);
        }}
    }}
    for (var key in azimuthLines) {{
        if (azimuthLines.hasOwnProperty(key)) {{
            map.removeLayer(azimuthLines[key]);
        }}
    }}
    for (var key in azimuthCones) {{
        if (azimuthCones.hasOwnProperty(key)) {{
            map.removeLayer(azimuthCones[key]);
        }}
    }}
    // Réinitialisation des dictionnaires
    stationMarkers = {{}};
    azimuthLines = {{}};
    azimuthCones = {{}};
    
    for (var i = 0; i < stations.length; i++) {{
        var s = stations[i];
        if (!s.visible) continue;
        
        // Utiliser station_id comme clé unique (jamais callsign)
        var stationId = s.station_id || ('station_' + i);
        
        var icon = L.divIcon({{
            className: 'station-marker',
            html: '<div class="marker-label" style="border-color:' + s.color + '; color:' + s.color + ';">' + s.callsign + '</div>',
            iconSize: null,
            iconAnchor: [0, 12]
        }});
        
        var marker = L.marker([s.lat, s.lon], {{
            icon: icon,
            draggable: true,
            autoPan: true
        }});
        
        // Popup avec DD et DMS
        var popupContent = '<b>' + s.callsign + '</b> <span style="color:#888">(' + (s.signal || 'S5') + ')</span><br>' +
            '<div class="popup-coords"><b>DD:</b> ' + s.lat.toFixed(6) + ', ' + s.lon.toFixed(6) + '</div>' +
            '<div class="popup-coords"><b>DMS:</b> ' + ddToDms(s.lat, true) + ' ' + ddToDms(s.lon, false) + '</div>' +
            '<div class="popup-coords"><b>Azimut:</b> ' + s.azimuth + '° ±' + s.uncertainty + '°</div>';
        marker.bindPopup(popupContent);
        
        // Stocker l'ID technique et les propriétés sur le marker
        marker._stationId = stationId;
        marker._callsign = s.callsign;
        marker._azimuth = s.azimuth;
        marker._uncertainty = s.uncertainty;
        marker._color = s.color;
        marker._signal = s.signal || 'S5';
        
        marker.on('drag', function(e) {{
            var pos = e.target.getLatLng();
            var sid = e.target._stationId;
            var az = e.target._azimuth;
            var unc = e.target._uncertainty;
            
            // Mettre à jour la ligne d'azimut pendant le drag
            if (azimuthLines[sid]) {{
                var endPoint = calcEndpoint(pos.lat, pos.lng, az, azimuthLength);
                azimuthLines[sid].setLatLngs([[pos.lat, pos.lng], endPoint]);
            }}
            // Mettre à jour le cône d'incertitude pendant le drag
            if (azimuthCones[sid]) {{
                var endLeft = calcEndpoint(pos.lat, pos.lng, az - unc, azimuthLength);
                var endRight = calcEndpoint(pos.lat, pos.lng, az + unc, azimuthLength);
                azimuthCones[sid].setLatLngs([[pos.lat, pos.lng], endLeft, endRight]);
            }}
        }});
        
        marker.on('dragend', function(e) {{
            var pos = e.target.getLatLng();
            var sid = e.target._stationId;
            // Retourner station_id pour identification côté Python
            window.lastDraggedStation = {{
                station_id: sid,
                lat: pos.lat,
                lon: pos.lng
            }};
        }});
        
        marker.addTo(map);
        stationMarkers[stationId] = marker;
        
        // Ne pas tracer l'azimut si signal S0 (station présente mais pas de réception)
        var signalLevel = s.signal || 'S5';
        if (signalLevel !== 'S0') {{
            var endPoint = calcEndpoint(s.lat, s.lon, s.azimuth, azimuthLength);
            var line = L.polyline([[s.lat, s.lon], endPoint], {{
                color: s.color,
                weight: 3,
                opacity: 0.9,
                dashArray: '10, 5'
            }});
            line.addTo(map);
            azimuthLines[stationId] = line;
            
            if (s.uncertainty > 0) {{
                var endLeft = calcEndpoint(s.lat, s.lon, s.azimuth - s.uncertainty, azimuthLength);
                var endRight = calcEndpoint(s.lat, s.lon, s.azimuth + s.uncertainty, azimuthLength);
                var cone = L.polygon([[s.lat, s.lon], endLeft, endRight], {{
                    color: s.color,
                    fillColor: s.color,
                    fillOpacity: 0.15,
                    weight: 1,
                    opacity: 0.5
                }});
                cone.addTo(map);
                azimuthCones[stationId] = cone;
            }}
        }}
    }}
    
    return stations.length;
}}

function getLastDraggedStation() {{
    var result = window.lastDraggedStation;
    window.lastDraggedStation = null;
    return result ? JSON.stringify(result) : null;
}}

function setAzimuthLength(len) {{
    azimuthLength = len;
}}

function showIntersectionZone(centerLat, centerLon, radiusKm, pointsJson, borderColor, fillColor, fillOpacity) {{
    clearIntersectionZone();
    
    var points = JSON.parse(pointsJson);
    
    intersectionCircle = L.circle([centerLat, centerLon], {{
        radius: radiusKm * 1000,
        color: borderColor,
        fillColor: fillColor,
        fillOpacity: fillOpacity,
        weight: 2,
        dashArray: '5, 5'
    }});
    
    var surface = (Math.PI * radiusKm * radiusKm).toFixed(2);
    intersectionCircle.bindPopup(
        '<b>📍 Zone d\\'intersection</b><br>' +
        '<div class="popup-coords"><b>Centre DD:</b> ' + centerLat.toFixed(6) + ', ' + centerLon.toFixed(6) + '</div>' +
        '<div class="popup-coords"><b>Centre DMS:</b> ' + ddToDms(centerLat, true) + ' ' + ddToDms(centerLon, false) + '</div>' +
        '<div class="popup-coords"><b>Rayon:</b> ' + radiusKm.toFixed(2) + ' km</div>' +
        '<div class="popup-coords"><b>Surface:</b> ' + surface + ' km²</div>' +
        '<div class="popup-coords"><b>Points:</b> ' + points.length + '</div>'
    );
    intersectionCircle.addTo(map);
    
    intersectionPoints = [];
    for (var i = 0; i < points.length; i++) {{
        var p = points[i];
        var pointMarker = L.circleMarker([p[0], p[1]], {{
            radius: 6,
            color: borderColor,
            fillColor: borderColor,
            fillOpacity: 1,
            weight: 2
        }});
        pointMarker.bindPopup(
            '<b>Intersection ' + (i+1) + '</b><br>' +
            '<div class="popup-coords"><b>DD:</b> ' + p[0].toFixed(6) + ', ' + p[1].toFixed(6) + '</div>' +
            '<div class="popup-coords"><b>DMS:</b> ' + ddToDms(p[0], true) + ' ' + ddToDms(p[1], false) + '</div>'
        );
        pointMarker.addTo(map);
        intersectionPoints.push(pointMarker);
    }}
}}

function clearIntersectionZone() {{
    if (intersectionCircle) {{
        map.removeLayer(intersectionCircle);
        intersectionCircle = null;
    }}
    for (var i = 0; i < intersectionPoints.length; i++) {{
        map.removeLayer(intersectionPoints[i]);
    }}
    intersectionPoints = [];
}}

var beaconMarker = null;

function showBeacon(lat, lon) {{
    // Toujours supprimer l'ancien marqueur d'abord
    if (beaconMarker !== null) {{
        try {{
            map.removeLayer(beaconMarker);
        }} catch(e) {{}}
        beaconMarker = null;
    }}
    
    var beaconIcon = L.divIcon({{
        className: 'beacon-marker',
        html: '<div class="beacon-pulse" style="background-color: #ff0000; border: 3px solid #fff; border-radius: 50%; width: 20px; height: 20px; cursor: move;"></div>',
        iconSize: [20, 20],
        iconAnchor: [10, 10]
    }});
    
    beaconMarker = L.marker([lat, lon], {{
        icon: beaconIcon,
        zIndexOffset: 1000,
        draggable: true
    }});
    
    function updateBeaconPopup(lat, lon) {{
        if (beaconMarker) {{
            beaconMarker.setPopupContent(
                '<b style="color: #ff0000;">🎯 BALISE DE DÉTRESSE</b><br>' +
                '<div class="popup-coords"><b>DD:</b> ' + lat.toFixed(6) + ', ' + lon.toFixed(6) + '</div>' +
                '<div class="popup-coords"><b>DMS:</b> ' + ddToDms(lat, true) + ' ' + ddToDms(lon, false) + '</div>' +
                '<div style="font-size: 10px; color: #666; margin-top: 5px;">Glisser pour déplacer</div>'
            );
        }}
    }}
    
    beaconMarker.bindPopup('');
    updateBeaconPopup(lat, lon);
    
    beaconMarker.on('dragend', function(e) {{
        var pos = e.target.getLatLng();
        updateBeaconPopup(pos.lat, pos.lng);
    }});
    
    beaconMarker.addTo(map);
}}

function getBeaconPosition() {{
    if (beaconMarker !== null) {{
        try {{
            var pos = beaconMarker.getLatLng();
            return JSON.stringify({{lat: pos.lat, lon: pos.lng}});
        }} catch(e) {{}}
    }}
    return null;
}}

function clearBeacon() {{
    if (beaconMarker !== null) {{
        try {{
            map.removeLayer(beaconMarker);
        }} catch(e) {{}}
        beaconMarker = null;
    }}
}}

function setGridColor(color) {{
    gridColor = color;
    if (showUtmGrid) {{
        drawUtmGrid();
    }}
}}

// Fonction pour calculer le fond contrasté du label
function getGridLabelBackground(hexColor) {{
    // Convertir hex en RGB
    var r = parseInt(hexColor.slice(1, 3), 16);
    var g = parseInt(hexColor.slice(3, 5), 16);
    var b = parseInt(hexColor.slice(5, 7), 16);
    // Calculer la luminosité
    var luminance = (0.299 * r + 0.587 * g + 0.114 * b) / 255;
    // Fond sombre si couleur claire, fond clair si couleur sombre
    if (luminance > 0.5) {{
        return 'rgba(0, 0, 0, 0.75)';
    }} else {{
        return 'rgba(40, 40, 40, 0.85)';
    }}
}}

function toggleUtmGrid(show) {{
    showUtmGrid = show;
    if (show) {{
        drawUtmGrid();
    }} else {{
        clearUtmGrid();
    }}
}}

function clearUtmGrid() {{
    if (utmGridLayer) {{
        map.removeLayer(utmGridLayer);
        utmGridLayer = null;
    }}
}}

function drawUtmGrid() {{
    clearUtmGrid();
    
    var bounds = map.getBounds();
    var zoom = map.getZoom();
    
    // Calculer le fond des labels en fonction de la couleur de la grille
    var labelBg = getGridLabelBackground(gridColor);
    
    var spacing;
    if (zoom < 8) spacing = 6;
    else if (zoom < 10) spacing = 1;
    else if (zoom < 12) spacing = 0.5;
    else if (zoom < 14) spacing = 0.1;
    else spacing = 0.05;
    
    var minLat = Math.floor(bounds.getSouth() / spacing) * spacing;
    var maxLat = Math.ceil(bounds.getNorth() / spacing) * spacing;
    var minLon = Math.floor(bounds.getWest() / spacing) * spacing;
    var maxLon = Math.ceil(bounds.getEast() / spacing) * spacing;
    
    var layers = [];
    var letters = "CDEFGHJKLMNPQRSTUVWXX";
    
    // Lignes verticales
    for (var lon = minLon; lon <= maxLon; lon += spacing) {{
        var line = L.polyline([[minLat, lon], [maxLat, lon]], {{
            color: gridColor,
            weight: 1,
            opacity: 0.6
        }});
        layers.push(line);
        
        var zone = Math.floor((lon + 180) / 6) + 1;
        var labelLat = bounds.getNorth() - (bounds.getNorth() - bounds.getSouth()) * 0.02;
        var label = L.marker([labelLat, lon], {{
            icon: L.divIcon({{
                className: 'utm-grid-label',
                html: '<span style="color:' + gridColor + '; background:' + labelBg + '; padding: 2px 5px; border-radius: 3px;">' + lon.toFixed(2) + '°</span>',
                iconSize: [70, 18],
                iconAnchor: [35, 0]
            }})
        }});
        layers.push(label);
    }}
    
    // Lignes horizontales
    for (var lat = minLat; lat <= maxLat; lat += spacing) {{
        var line = L.polyline([[lat, minLon], [lat, maxLon]], {{
            color: gridColor,
            weight: 1,
            opacity: 0.6
        }});
        layers.push(line);
        
        var letter = letters[Math.floor((lat + 80) / 8)] || '?';
        var zone = Math.floor((bounds.getCenter().lng + 180) / 6) + 1;
        var labelLon = bounds.getWest() + (bounds.getEast() - bounds.getWest()) * 0.02;
        var label = L.marker([lat, labelLon], {{
            icon: L.divIcon({{
                className: 'utm-grid-label',
                html: '<span style="color:' + gridColor + '; background:' + labelBg + '; padding: 2px 5px; border-radius: 3px;">' + zone + letter + ' ' + lat.toFixed(2) + '°</span>',
                iconSize: [90, 18],
                iconAnchor: [0, 9]
            }})
        }});
        layers.push(label);
    }}
    
    utmGridLayer = L.layerGroup(layers);
    utmGridLayer.addTo(map);
}}

map.on('moveend', function() {{
    if (showUtmGrid) {{
        drawUtmGrid();
    }}
}});

map.on('zoomend', function() {{
    if (showUtmGrid) {{
        drawUtmGrid();
    }}
}});

function centerMap(lat, lon, zoom) {{
    map.setView([lat, lon], zoom);
}}

console.log('SATER Map v2.7 loaded');
</script>
</body>
</html>'''

    def _connect_signals(self):
        self.add_btn.clicked.connect(self.add_station)
        self.clear_btn.clicked.connect(self.clear_all_stations)
        self.length_spin.valueChanged.connect(self._on_length_changed)
        self.intersection_check.stateChanged.connect(self._on_settings_changed)
        self.utm_grid_check.stateChanged.connect(self._on_utm_grid_changed)
        self.tiles_combo.currentIndexChanged.connect(self._on_tiles_changed)
        self.offline_check.stateChanged.connect(self._on_offline_changed)
        
        # Connexions du tableau des stations
        self.stations_table.stationDataChanged.connect(self._schedule_update)
        self.stations_table.recordingRequested.connect(self._on_recording_requested)

    def _run_js(self, code, callback=None):
        if self.map_view is None:
            return
        if callback:
            self.map_view.page().runJavaScript(code, callback)
        else:
            self.map_view.page().runJavaScript(code)

    def _on_recording_requested(self, callsign: str, azimuth: float, uncertainty: float, lat: float, lon: float, signal: str):
        record = AzimuthRecord(
            timestamp=datetime.now().strftime("%d/%m/%Y %H:%M:%S"),
            callsign=callsign,
            azimuth=azimuth,
            uncertainty=uncertainty,
            lat=lat,
            lon=lon,
            signal=signal
        )
        self.azimuth_history.append(record)
        self.history_count_label.setText(f"{len(self.azimuth_history)} relevés")

    def add_station(self):
        row = self.stations_table.add_station()
        self.status_bar.showMessage(f"Station ajoutée ({self.stations_table.rowCount()})")
        self._schedule_update()

    def remove_station(self, row: int):
        # Cette méthode n'est plus nécessaire, la suppression est gérée par StationsTableWidget
        pass

    def clear_all_stations(self):
        if self.stations_table.rowCount() == 0:
            return
        if QMessageBox.question(self, "Confirmation", "Supprimer toutes les stations ?") == QMessageBox.StandardButton.Yes:
            self.stations_table.clear_all()
            self._schedule_update()

    def _schedule_update(self):
        self.update_timer.start(150)

    def _check_drag(self):
        if not self.map_ready:
            return
            
        def on_result(result):
            if result:
                try:
                    data = json.loads(result)
                    # Utiliser station_id pour identifier la station (pas callsign)
                    station_id = data.get('station_id')
                    if station_id:
                        row = self.stations_table.get_row_by_station_id(station_id)
                        if row >= 0:
                            self.stations_table.set_coordinates(row, data['lat'], data['lon'])
                except:
                    pass
        
        self._run_js("getLastDraggedStation()", on_result)
        
        # Vérifier si la balise a été déplacée
        def on_beacon_result(result):
            if result:
                try:
                    data = json.loads(result)
                    self.beacon_position = (data['lat'], data['lon'])
                except:
                    pass
        
        self._run_js("getBeaconPosition()", on_beacon_result)

    def _do_update_map(self):
        if not self.map_ready:
            return
            
        stations = self.stations_table.get_all_stations()
        
        stations_data = [s.to_dict() for s in stations]
        json_str = json.dumps(stations_data)
        js_code = f'updateStations(`{json_str}`);'
        self._run_js(js_code)
        
        # Afficher la balise si définie
        if self.beacon_position:
            lat, lon = self.beacon_position
            self._run_js(f"showBeacon({lat}, {lon});")
        else:
            self._run_js("clearBeacon();")
        
        visible = [s for s in stations if s.visible]
        if len(visible) >= 2 and self.show_intersection:
            intersections = calculate_all_intersections(visible, self.azimuth_length_km)
            
            if intersections:
                circle_data = calculate_uncertainty_circle(intersections)
                if circle_data:
                    center_lat, center_lon, radius_km, surface_km2 = circle_data
                    # Sauvegarder le centre pour pouvoir y centrer la carte
                    self.last_intersection_center = (center_lat, center_lon)
                    
                    points_json = json.dumps(intersections)
                    self._run_js(f"showIntersectionZone({center_lat}, {center_lon}, {radius_km}, `{points_json}`, '{self.zone_border_color}', '{self.zone_fill_color}', {self.zone_opacity});")
                    
                    mgrs = lat_lon_to_mgrs(center_lat, center_lon)
                    self.mgrs_label.setText(f"MGRS: {mgrs}")
                    
                    if len(intersections) == 1:
                        self.intersection_label.setText(
                            f"Intersection: {center_lat:.6f}, {center_lon:.6f}\n"
                            f"Surface: {surface_km2:.2f} km²"
                        )
                    else:
                        self.intersection_label.setText(
                            f"Centre: {center_lat:.6f}, {center_lon:.6f}\n"
                            f"Rayon: {radius_km:.2f} km | Surface: {surface_km2:.2f} km²\n"
                            f"({len(intersections)} points)"
                        )
                else:
                    self._run_js("clearIntersectionZone();")
                    self.intersection_label.setText("Calcul impossible")
                    self.mgrs_label.setText("MGRS: -")
                    self.last_intersection_center = None
            else:
                self._run_js("clearIntersectionZone();")
                self.intersection_label.setText("Pas d'intersection")
                self.mgrs_label.setText("MGRS: -")
                self.last_intersection_center = None
        else:
            self._run_js("clearIntersectionZone();")
            self.intersection_label.setText("Zone d'intersection: -")
            self.mgrs_label.setText("MGRS: -")
            self.last_intersection_center = None

    def _on_length_changed(self, value):
        self.azimuth_length_km = value
        if self.map_ready:
            self._run_js(f"setAzimuthLength({value});")
            self._schedule_update()

    def _on_settings_changed(self):
        self.show_intersection = self.intersection_check.isChecked()
        self._schedule_update()

    def _on_utm_grid_changed(self, state):
        self.show_utm_grid = (state == Qt.CheckState.Checked.value)
        if self.map_ready:
            if self.show_utm_grid:
                # Appliquer la grille ET la couleur
                self._run_js(f"toggleUtmGrid(true); setGridColor('{self.grid_color}');")
            else:
                self._run_js("toggleUtmGrid(false);")

    def _on_tiles_changed(self, index):
        self.current_provider = self.tiles_combo.currentData()
        
        # Sauvegarder la position actuelle avant de recharger
        if self.map_ready:
            def on_bounds(result):
                if result:
                    try:
                        data = json.loads(result)
                        self.map_center_lat = (data['minLat'] + data['maxLat']) / 2
                        self.map_center_lon = (data['minLon'] + data['maxLon']) / 2
                        self.map_zoom = data['zoom']
                    except:
                        pass
                # Recharger la carte avec la nouvelle position sauvegardée
                self._reload_map()
                # Rafraîchir la grille UTM/MGRS si elle est activée
                if self.utm_grid_check.isChecked():
                    QTimer.singleShot(500, lambda: self._run_js(f"toggleUtmGrid(true); setGridColor('{self.grid_color}');"))
            
            self._run_js("getMapBounds()", on_bounds)
        else:
            self._reload_map()

    def _on_offline_changed(self, state):
        self.offline_mode = (state == Qt.CheckState.Checked.value)
        
        if self.offline_mode:
            provider_dir = os.path.join(self.tiles_dir, self.current_provider)
            if not os.path.exists(provider_dir):
                QMessageBox.warning(self, "Attention", 
                    f"Aucune tuile locale pour '{TILE_PROVIDERS[self.current_provider]['name']}'.")
        
        # Sauvegarder la position actuelle avant de recharger
        if self.map_ready:
            def on_bounds(result):
                if result:
                    try:
                        data = json.loads(result)
                        self.map_center_lat = (data['minLat'] + data['maxLat']) / 2
                        self.map_center_lon = (data['minLon'] + data['maxLon']) / 2
                        self.map_zoom = data['zoom']
                    except:
                        pass
                self._reload_map()
            
            self._run_js("getMapBounds()", on_bounds)
        else:
            self._reload_map()

    def show_history(self):
        dialog = HistoryDialog(self.azimuth_history, self)
        dialog.exec()
        # Récupérer l'historique modifié (suppressions individuelles ou totales)
        self.azimuth_history = dialog.get_history()
        self.history_count_label.setText(f"{len(self.azimuth_history)} relevés")

    def show_kilometers(self):
        """Ouvre le dialogue de saisie des kilomètres parcourus"""
        # Récupérer les indicatifs des stations actuelles
        callsigns = []
        for row in range(self.stations_table.rowCount()):
            station = self.stations_table.get_station(row)
            if station and station.callsign:
                callsigns.append(station.callsign)
        
        dialog = KilometersDialog(callsigns, self.station_kilometers, self)
        if dialog.exec() == QDialog.DialogCode.Accepted:
            self.station_kilometers = dialog.get_kilometers()

    def center_map(self, lat, lon, zoom):
        if self.map_ready:
            self._run_js(f"centerMap({lat}, {lon}, {zoom});")

    def center_on_intersection(self):
        """Centre la carte sur la zone d'intersection"""
        if self.last_intersection_center:
            lat, lon = self.last_intersection_center
            self.center_map(lat, lon, 12)
        else:
            QMessageBox.information(self, "Information", 
                "Aucune zone d'intersection calculée.\n"
                "Ajoutez au moins 2 stations avec des azimuts pour créer une intersection.")

    def set_beacon_position(self):
        """Définit la position de la balise de détresse"""
        dialog = QDialog(self)
        dialog.setWindowTitle("Position de la balise de détresse")
        dialog.setMinimumWidth(400)
        layout = QVBoxLayout(dialog)
        
        layout.addWidget(QLabel("<b>Entrez les coordonnées de la balise:</b>"))
        
        # Onglets DD / DMS
        tabs = QTabWidget()
        
        # === Onglet Degrés Décimaux ===
        dd_tab = QWidget()
        dd_layout = QFormLayout(dd_tab)
        
        lat_spin = QDoubleSpinBox()
        lat_spin.setRange(-90, 90)
        lat_spin.setDecimals(6)
        lat_spin.setValue(self.beacon_position[0] if self.beacon_position else 43.7)
        dd_layout.addRow("Latitude (DD):", lat_spin)
        
        lon_spin = QDoubleSpinBox()
        lon_spin.setRange(-180, 180)
        lon_spin.setDecimals(6)
        lon_spin.setValue(self.beacon_position[1] if self.beacon_position else 7.25)
        dd_layout.addRow("Longitude (DD):", lon_spin)
        
        tabs.addTab(dd_tab, "Degrés Décimaux")
        
        # === Onglet DMS ===
        dms_tab = QWidget()
        dms_layout = QVBoxLayout(dms_tab)
        
        # Latitude DMS
        lat_group = QGroupBox("Latitude")
        lat_dms_layout = QHBoxLayout(lat_group)
        
        lat_deg = QSpinBox()
        lat_deg.setRange(0, 90)
        lat_dms_layout.addWidget(lat_deg)
        lat_dms_layout.addWidget(QLabel("°"))
        
        lat_min = QSpinBox()
        lat_min.setRange(0, 59)
        lat_dms_layout.addWidget(lat_min)
        lat_dms_layout.addWidget(QLabel("'"))
        
        lat_sec = QDoubleSpinBox()
        lat_sec.setRange(0, 59.99)
        lat_sec.setDecimals(2)
        lat_dms_layout.addWidget(lat_sec)
        lat_dms_layout.addWidget(QLabel('"'))
        
        lat_dir = QComboBox()
        lat_dir.addItems(["N", "S"])
        lat_dms_layout.addWidget(lat_dir)
        
        dms_layout.addWidget(lat_group)
        
        # Longitude DMS
        lon_group = QGroupBox("Longitude")
        lon_dms_layout = QHBoxLayout(lon_group)
        
        lon_deg = QSpinBox()
        lon_deg.setRange(0, 180)
        lon_dms_layout.addWidget(lon_deg)
        lon_dms_layout.addWidget(QLabel("°"))
        
        lon_min = QSpinBox()
        lon_min.setRange(0, 59)
        lon_dms_layout.addWidget(lon_min)
        lon_dms_layout.addWidget(QLabel("'"))
        
        lon_sec = QDoubleSpinBox()
        lon_sec.setRange(0, 59.99)
        lon_sec.setDecimals(2)
        lon_dms_layout.addWidget(lon_sec)
        lon_dms_layout.addWidget(QLabel('"'))
        
        lon_dir = QComboBox()
        lon_dir.addItems(["E", "W"])
        lon_dms_layout.addWidget(lon_dir)
        
        dms_layout.addWidget(lon_group)
        
        tabs.addTab(dms_tab, "Degrés Minutes Secondes")
        
        layout.addWidget(tabs)
        
        # Initialiser les valeurs DMS depuis la position actuelle
        def update_dms_from_dd():
            lat = lat_spin.value()
            lon = lon_spin.value()
            
            lat_dir.setCurrentText("N" if lat >= 0 else "S")
            lat = abs(lat)
            lat_deg.setValue(int(lat))
            lat_min.setValue(int((lat - int(lat)) * 60))
            lat_sec.setValue(((lat - int(lat)) * 60 - int((lat - int(lat)) * 60)) * 60)
            
            lon_dir.setCurrentText("E" if lon >= 0 else "W")
            lon = abs(lon)
            lon_deg.setValue(int(lon))
            lon_min.setValue(int((lon - int(lon)) * 60))
            lon_sec.setValue(((lon - int(lon)) * 60 - int((lon - int(lon)) * 60)) * 60)
        
        def update_dd_from_dms():
            lat = lat_deg.value() + lat_min.value() / 60 + lat_sec.value() / 3600
            if lat_dir.currentText() == "S":
                lat = -lat
            lat_spin.setValue(lat)
            
            lon = lon_deg.value() + lon_min.value() / 60 + lon_sec.value() / 3600
            if lon_dir.currentText() == "W":
                lon = -lon
            lon_spin.setValue(lon)
        
        # Mettre à jour quand on change d'onglet
        tabs.currentChanged.connect(lambda idx: update_dms_from_dd() if idx == 1 else update_dd_from_dms())
        
        # Initialiser DMS
        update_dms_from_dd()
        
        # Option pour utiliser le centre de la zone d'intersection
        if self.last_intersection_center:
            use_intersection_btn = QPushButton("📍 Utiliser le centre de la zone d'intersection")
            use_intersection_btn.clicked.connect(lambda: (
                lat_spin.setValue(self.last_intersection_center[0]),
                lon_spin.setValue(self.last_intersection_center[1]),
                update_dms_from_dd()
            ))
            layout.addWidget(use_intersection_btn)
        
        layout.addWidget(QLabel("<i>💡 La balise peut aussi être déplacée directement sur la carte</i>"))
        
        buttons = QDialogButtonBox(QDialogButtonBox.StandardButton.Ok | QDialogButtonBox.StandardButton.Cancel)
        buttons.accepted.connect(dialog.accept)
        buttons.rejected.connect(dialog.reject)
        layout.addWidget(buttons)
        
        if dialog.exec() == QDialog.DialogCode.Accepted:
            # Si on est sur l'onglet DMS, mettre à jour DD d'abord
            if tabs.currentIndex() == 1:
                update_dd_from_dms()
            
            lat = lat_spin.value()
            lon = lon_spin.value()
            self.beacon_position = (lat, lon)
            
            # Appeler directement showBeacon en JavaScript
            if self.map_ready:
                self._run_js(f"showBeacon({lat}, {lon});")
            
            self.status_bar.showMessage(f"Position balise: {lat:.6f}, {lon:.6f}")

    def clear_beacon_position(self):
        """Efface la position de la balise"""
        self.beacon_position = None
        # Appeler directement clearBeacon en JavaScript
        if self.map_ready:
            self._run_js("clearBeacon();")
        self.status_bar.showMessage("Position de la balise effacée")

    def download_visible_tiles(self):
        if not self.map_ready:
            QMessageBox.warning(self, "Erreur", "La carte n'est pas encore chargée.")
            return
        
        def on_bounds(result):
            if not result:
                return
            try:
                bounds = json.loads(result)
                self._do_download_tiles(bounds)
            except:
                pass
        
        self._run_js("getMapBounds()", on_bounds)

    def _do_download_tiles(self, bounds):
        min_lat, max_lat = bounds['minLat'], bounds['maxLat']
        min_lon, max_lon = bounds['minLon'], bounds['maxLon']
        current_zoom = bounds['zoom']
        
        provider = TILE_PROVIDERS[self.current_provider]
        min_zoom = max(1, current_zoom - 2)
        max_zoom = min(provider['max_zoom'], current_zoom + 2)
        
        tiles = get_tiles_for_bounds(min_lat, min_lon, max_lat, max_lon, min_zoom, max_zoom)
        
        if not tiles:
            return
        
        reply = QMessageBox.question(self, "Télécharger les tuiles",
            f"Fond: {provider['name']}\nTuiles: {len(tiles)}\nZoom: {min_zoom}-{max_zoom}\n\nContinuer ?",
            QMessageBox.StandardButton.Yes | QMessageBox.StandardButton.No)
        
        if reply != QMessageBox.StandardButton.Yes:
            return
        
        os.makedirs(self.tiles_dir, exist_ok=True)
        
        progress = QProgressDialog("Téléchargement...", "Annuler", 0, len(tiles), self)
        progress.setWindowModality(Qt.WindowModality.WindowModal)
        
        self.downloader = TileDownloader(tiles, self.tiles_dir, self.current_provider)
        
        def on_progress(current, total, msg):
            progress.setValue(current)
            if progress.wasCanceled():
                self.downloader.cancel()
        
        def on_finished(downloaded, failed):
            progress.close()
            self._update_tiles_info()
            QMessageBox.information(self, "Terminé", f"Téléchargées: {downloaded}\nÉchouées: {failed}")
        
        self.downloader.progress.connect(on_progress)
        self.downloader.finished.connect(on_finished)
        
        thread = threading.Thread(target=self.downloader.download)
        thread.start()

    def save_map_html(self):
        fn, _ = QFileDialog.getSaveFileName(self, "Sauvegarder", "sater_map.html", "HTML (*.html)")
        if not fn:
            return
        
        stations = self.stations_table.get_all_stations()
        stations_data = [s.to_dict() for s in stations if s]
        
        # Récupérer la vue actuelle de la carte
        def on_bounds(result):
            view_data = None
            if result:
                try:
                    view_data = json.loads(result)
                except:
                    pass
            
            html = self._standalone_html(stations_data, view_data)
            with open(fn, 'w', encoding='utf-8') as f:
                f.write(html)
            self.status_bar.showMessage(f"Sauvegardé: {fn}")
        
        if self.map_ready:
            self._run_js("getMapBounds()", on_bounds)
        else:
            html = self._standalone_html(stations_data, None)
            with open(fn, 'w', encoding='utf-8') as f:
                f.write(html)
            self.status_bar.showMessage(f"Sauvegardé: {fn}")

    def export_map_png(self):
        """Exporte la carte actuelle en PNG"""
        if self.map_view is None:
            QMessageBox.warning(self, "Erreur", "La carte n'est pas encore chargée.")
            return
            
        fn, _ = QFileDialog.getSaveFileName(self, "Exporter PNG", 
            f"sater_map_{datetime.now().strftime('%Y%m%d_%H%M')}.png", 
            "PNG (*.png)")
        if not fn:
            return
        
        # Capturer la vue web
        self.map_view.grab().save(fn, "PNG")
        self.status_bar.showMessage(f"PNG exporté: {fn}")

    def _standalone_html(self, stations, view_data=None):
        mission_time = self.mission_timer.get_elapsed_str()
        start_time = self.mission_timer.get_start_time()
        start_str = start_time.strftime("%d/%m/%Y %H:%M") if start_time else "Non démarrée"
        
        # Préparer les données de la balise
        beacon_js = "null"
        if self.beacon_position:
            beacon_js = f"{{lat: {self.beacon_position[0]}, lon: {self.beacon_position[1]}}}"
        
        # Utiliser la vue actuelle ou les valeurs par défaut
        if view_data:
            center_lat = (view_data['minLat'] + view_data['maxLat']) / 2
            center_lon = (view_data['minLon'] + view_data['maxLon']) / 2
            zoom = view_data['zoom']
        else:
            center_lat, center_lon, zoom = 43.7, 7.25, 10
        
        # Utiliser le provider actuel
        provider = TILE_PROVIDERS.get(self.current_provider, TILE_PROVIDERS['osm'])
        tile_url = provider['url']
        attribution = provider['attribution'].replace("'", "\\'")  # Échapper les apostrophes
        max_zoom = provider['max_zoom']
        
        # Gérer les subdomains - convertir la liste en chaîne
        if 'subdomains' in provider:
            subdomains_str = ''.join(provider['subdomains'])  # ['a','b','c'] -> 'abc'
            subdomains_js = f"subdomains: '{subdomains_str}',"
        else:
            subdomains_js = ""
        
        return f'''<!DOCTYPE html>
<html><head><meta charset="utf-8">
<link rel="stylesheet" href="https://unpkg.com/leaflet@1.9.4/dist/leaflet.css"/>
<script src="https://unpkg.com/leaflet@1.9.4/dist/leaflet.js"></script>
<style>
html,body,#map{{height:100%;margin:0}}
.info{{position:absolute;top:10px;right:10px;z-index:1000;background:#fff;padding:10px;border-radius:5px;box-shadow:0 2px 10px rgba(0,0,0,.2);max-width:300px}}
.marker-label{{
    display:inline-block;
    background:rgba(255,255,255,0.95);
    border:2px solid;
    border-radius:4px;
    padding:4px 8px;
    font-size:12px;
    font-weight:bold;
    white-space:nowrap;
    line-height:1.2;
    box-shadow: 0 2px 5px rgba(0,0,0,0.3);
}}
@keyframes beacon-pulse {{
    0% {{ box-shadow: 0 0 5px #ff0000, 0 0 10px #ff0000; transform: scale(1); }}
    50% {{ box-shadow: 0 0 20px #ff0000, 0 0 40px #ff0000, 0 0 60px #ff3333; transform: scale(1.1); }}
    100% {{ box-shadow: 0 0 5px #ff0000, 0 0 10px #ff0000; transform: scale(1); }}
}}
.beacon-pulse {{
    animation: beacon-pulse 1.5s ease-in-out infinite;
}}
</style>
</head><body><div id="map"></div>
<div class="info">
<h3>🛰️ SATER Map</h3>
<p><b>Mission:</b> {start_str}<br><b>Durée:</b> {mission_time}</p>
<div id="list"></div>
<p><b>Relevés:</b> {len(self.azimuth_history)}</p>
</div>
<script>
var stations={json.dumps(stations)};
var len={self.azimuth_length_km};
var beacon={beacon_js};
var map=L.map('map').setView([{center_lat}, {center_lon}], {zoom});
L.tileLayer('{tile_url}', {{
    attribution: '{attribution}',
    maxZoom: {max_zoom},
    {subdomains_js}
}}).addTo(map);

function calcEnd(lat,lon,az,d){{
    var R=6371,lat1=lat*Math.PI/180,lon1=lon*Math.PI/180,b=az*Math.PI/180;
    var lat2=Math.asin(Math.sin(lat1)*Math.cos(d/R)+Math.cos(lat1)*Math.sin(d/R)*Math.cos(b));
    var lon2=lon1+Math.atan2(Math.sin(b)*Math.sin(d/R)*Math.cos(lat1),Math.cos(d/R)-Math.sin(lat1)*Math.sin(lat2));
    return[lat2*180/Math.PI,lon2*180/Math.PI];
}}

var html=[];
stations.forEach(function(s){{
    if(!s.visible) return;
    
    // Créer le label avec le nom de la station
    var labelHtml = '<div class="marker-label" style="border-color:'+s.color+';color:'+s.color+';">'+s.callsign+'</div>';
    var icon = L.divIcon({{
        className: '',
        html: labelHtml,
        iconSize: null,
        iconAnchor: [-8, 14]
    }});
    
    var signalStr = s.signal || 'S5';
    L.marker([s.lat,s.lon],{{icon:icon}})
        .bindPopup('<b>'+s.callsign+'</b><br>Signal: '+signalStr+'<br>Az: '+s.azimuth+'° ±'+s.uncertainty+'°')
        .addTo(map);
    
    var e=calcEnd(s.lat,s.lon,s.azimuth,len);
    L.polyline([[s.lat,s.lon],e],{{color:s.color,weight:3,dashArray:'10,5'}}).addTo(map);
    
    if(s.uncertainty>0){{
        var eL=calcEnd(s.lat,s.lon,s.azimuth-s.uncertainty,len);
        var eR=calcEnd(s.lat,s.lon,s.azimuth+s.uncertainty,len);
        L.polygon([[s.lat,s.lon],eL,eR],{{color:s.color,fillColor:s.color,fillOpacity:0.15,weight:1}}).addTo(map);
    }}
    
    html.push('<div><span style="color:'+s.color+'">●</span> '+s.callsign+' - '+s.azimuth+'° ±'+s.uncertainty+'° ('+signalStr+')</div>');
}});

// Ajouter la balise si elle existe
if(beacon) {{
    var beaconIcon = L.divIcon({{
        className: 'beacon-marker',
        html: '<div class="beacon-pulse" style="background-color: #ff0000; border: 3px solid #fff; border-radius: 50%; width: 20px; height: 20px;"></div>',
        iconSize: [20, 20],
        iconAnchor: [10, 10]
    }});
    L.marker([beacon.lat, beacon.lon], {{icon: beaconIcon, zIndexOffset: 1000}})
        .bindPopup('<b style="color: #ff0000;">🎯 BALISE DE DÉTRESSE</b><br>DD: '+beacon.lat.toFixed(6)+', '+beacon.lon.toFixed(6))
        .addTo(map);
}}

document.getElementById('list').innerHTML=html.join('');
</script></body></html>'''

    def export_kml(self):
        fn, _ = QFileDialog.getSaveFileName(self, "Export KML", "sater.kml", "KML (*.kml)")
        if not fn: return
        stations = self.stations_table.get_all_stations()
        kml = '<?xml version="1.0" encoding="UTF-8"?>\n<kml xmlns="http://www.opengis.net/kml/2.2"><Document><n>SATER</n>\n'
        for s in stations:
            end_lat, end_lon = calc_endpoint_haversine(s.lat, s.lon, s.azimuth, self.azimuth_length_km)
            kml += f'<Placemark><n>{s.callsign}</n><Point><coordinates>{s.lon},{s.lat},0</coordinates></Point></Placemark>\n'
            kml += f'<Placemark><n>{s.callsign} Az {s.azimuth}° ±{s.uncertainty}°</n><LineString><coordinates>{s.lon},{s.lat},0 {end_lon},{end_lat},0</coordinates></LineString></Placemark>\n'
        kml += '</Document></kml>'
        with open(fn, 'w') as f: f.write(kml)
        self.status_bar.showMessage(f"KML: {fn}")

    def export_json(self):
        fn, _ = QFileDialog.getSaveFileName(self, "Export JSON", "sater.json", "JSON (*.json)")
        if not fn: return
        stations = self.stations_table.get_all_stations()
        data = {
            'version': APP_VERSION, 
            'azimuth_length_km': self.azimuth_length_km, 
            'stations': [s.to_dict() for s in stations],
            'history': [h.to_dict() for h in self.azimuth_history],
            'kilometers': self.station_kilometers,
            'beacon': {'lat': self.beacon_position[0], 'lon': self.beacon_position[1]} if self.beacon_position else None,
            'mission_start': self.mission_timer.get_start_time().isoformat() if self.mission_timer.get_start_time() else None,
            'mission_duration': self.mission_timer.get_elapsed_str(),
            'settings': {
                'grid_color': self.grid_color,
                'zone_border_color': self.zone_border_color,
                'zone_fill_color': self.zone_fill_color,
                'zone_opacity': self.zone_opacity
            }
        }
        with open(fn, 'w') as f: json.dump(data, f, indent=2)
        self.status_bar.showMessage(f"JSON: {fn}")

    def import_json(self):
        fn, _ = QFileDialog.getOpenFileName(self, "Import JSON", "", "JSON (*.json)")
        if not fn: return
        try:
            with open(fn) as f: data = json.load(f)
            self.stations_table.clear_all()
            if 'azimuth_length_km' in data: self.length_spin.setValue(data['azimuth_length_km'])
            for sd in data.get('stations', []):
                row = self.stations_table.add_station()
                self.stations_table.set_station(row, Station(**sd))
            if 'history' in data:
                self.azimuth_history = [AzimuthRecord(**h) for h in data['history']]
                self.history_count_label.setText(f"{len(self.azimuth_history)} relevés")
            if 'kilometers' in data:
                self.station_kilometers = data['kilometers']
            if 'beacon' in data and data['beacon']:
                self.beacon_position = (data['beacon']['lat'], data['beacon']['lon'])
            else:
                self.beacon_position = None
            if 'settings' in data:
                s = data['settings']
                if 'grid_color' in s:
                    self.grid_color = s['grid_color']
                    self.grid_color_btn.setStyleSheet(f"background-color: {self.grid_color};")
                if 'zone_border_color' in s:
                    self.zone_border_color = s['zone_border_color']
                    self.zone_border_btn.setStyleSheet(f"background-color: {self.zone_border_color};")
                if 'zone_fill_color' in s:
                    self.zone_fill_color = s['zone_fill_color']
                    self.zone_fill_btn.setStyleSheet(f"background-color: {self.zone_fill_color};")
                if 'zone_opacity' in s:
                    self.zone_opacity = s['zone_opacity']
                    self.zone_opacity_slider.setValue(int(s['zone_opacity'] * 100))
            self._schedule_update()
            self.status_bar.showMessage(f"Importé: {self.stations_table.rowCount()} stations")
        except Exception as e:
            QMessageBox.warning(self, "Erreur", str(e))

    def show_about(self):
        QMessageBox.about(self, "À propos", 
            f"<h2>{APP_NAME}</h2>"
            f"<p>Version {APP_VERSION}</p>"
            f"<p><b>ADRASEC 06</b></p>"
            f"<p>Application de radiogoniométrie pour la recherche<br>"
            f"de balises de détresse (missions SATER).</p>"
            f"<hr>"
            f"<p><small>© 2024-2025 - Licence libre</small></p>")

    def generate_pdf_report(self):
        """Génère un rapport PDF de la mission"""
        if not HAS_REPORTLAB:
            QMessageBox.warning(self, "Module manquant",
                "Le module 'reportlab' n'est pas installé.\n\n"
                "Installez-le avec:\npip install reportlab")
            return
        
        # Dialogue pour le titre et commentaire
        dialog = QDialog(self)
        dialog.setWindowTitle("Informations du rapport PDF")
        dialog.setMinimumWidth(500)
        layout = QVBoxLayout(dialog)
        
        form = QFormLayout()
        
        title_edit = QLineEdit()
        title_edit.setText("Rapport de mission SATER")
        title_edit.setPlaceholderText("Titre du document")
        form.addRow("Titre:", title_edit)
        
        subject_edit = QLineEdit()
        subject_edit.setText("Radiogoniométrie - Recherche balise de détresse")
        subject_edit.setPlaceholderText("Sujet du document")
        form.addRow("Sujet:", subject_edit)
        
        keywords_edit = QLineEdit()
        keywords_edit.setText("SATER, ADRASEC, radiogoniométrie, balise, détresse, 406MHz")
        keywords_edit.setPlaceholderText("Mots-clés séparés par des virgules")
        form.addRow("Mots-clés:", keywords_edit)
        
        layout.addLayout(form)
        
        layout.addWidget(QLabel("<b>Commentaire / Notes:</b>"))
        comment_edit = QTextEdit()
        comment_edit.setPlaceholderText("Ajoutez ici vos commentaires, observations ou notes sur la mission...")
        comment_edit.setMinimumHeight(100)
        layout.addWidget(comment_edit)
        
        buttons = QDialogButtonBox(QDialogButtonBox.StandardButton.Ok | QDialogButtonBox.StandardButton.Cancel)
        buttons.accepted.connect(dialog.accept)
        buttons.rejected.connect(dialog.reject)
        layout.addWidget(buttons)
        
        if dialog.exec() != QDialog.DialogCode.Accepted:
            return
        
        fn, _ = QFileDialog.getSaveFileName(self, "Sauvegarder rapport PDF", 
            f"rapport_mission_{datetime.now().strftime('%Y%m%d_%H%M')}.pdf", 
            "PDF (*.pdf)")
        if not fn:
            return
        
        # Capturer la carte en PNG temporaire
        map_image_path = None
        if self.map_view:
            try:
                map_image_path = os.path.join(self.temp_dir, "map_capture.png")
                self.map_view.grab().save(map_image_path, "PNG")
            except Exception as e:
                print(f"Erreur capture carte: {e}")
                map_image_path = None
        
        # Collecter les données de mission
        stations = self.stations_table.get_all_stations()
        
        mission_data = {
            'title': title_edit.text().strip() or "Rapport de mission SATER",
            'subject': subject_edit.text().strip(),
            'keywords': keywords_edit.text().strip(),
            'comment': comment_edit.toPlainText().strip(),
            'start_date': self.mission_timer.get_start_time().strftime('%d/%m/%Y') if self.mission_timer.get_start_time() else 'N/A',
            'start_time': self.mission_timer.get_start_time().strftime('%H:%M:%S') if self.mission_timer.get_start_time() else 'N/A',
            'duration': self.mission_timer.get_elapsed_str(),
            'station_count': len(stations),
            'record_count': len(self.azimuth_history),
            'stations': [s.to_dict() for s in stations],
            'history': [h.to_dict() for h in self.azimuth_history],
            'kilometers': self.station_kilometers,
            'map_image': map_image_path,
        }
        
        # Ajouter la position de la balise si définie
        if self.beacon_position:
            mission_data['beacon'] = {
                'lat': self.beacon_position[0],
                'lon': self.beacon_position[1],
                'mgrs': lat_lon_to_mgrs(self.beacon_position[0], self.beacon_position[1])
            }
        
        # Ajouter les infos d'intersection si disponibles
        visible = [s for s in stations if s.visible]
        if len(visible) >= 2:
            intersections = calculate_all_intersections(visible, self.azimuth_length_km)
            if intersections:
                circle_data = calculate_uncertainty_circle(intersections)
                if circle_data:
                    center_lat, center_lon, radius_km, surface_km2 = circle_data
                    mission_data['intersection'] = {
                        'center_lat': center_lat,
                        'center_lon': center_lon,
                        'radius_km': radius_km,
                        'surface_km2': surface_km2,
                        'mgrs': lat_lon_to_mgrs(center_lat, center_lon),
                        'point_count': len(intersections)
                    }
        
        # Déterminer le chemin des images
        img_dir = os.path.join(os.path.dirname(os.path.abspath(__file__)), "img")
        if not os.path.exists(img_dir):
            img_dir = "./img"
        
        if generate_pdf_report(fn, mission_data, img_dir):
            QMessageBox.information(self, "Succès", f"Rapport PDF généré:\n{fn}")
            self.status_bar.showMessage(f"PDF: {fn}")
        else:
            QMessageBox.warning(self, "Erreur", "Erreur lors de la génération du PDF")

    def manage_presets(self):
        """Ouvre le gestionnaire de présets"""
        presets = load_presets()
        dialog = PresetManagerDialog(presets, self)
        if dialog.exec() == QDialog.DialogCode.Accepted:
            # Sauvegarder les présets modifiés
            save_presets(dialog.get_presets())
            
            # Charger les présets sélectionnés
            selected = dialog.get_selected_presets()
            if selected:
                self._load_presets_to_stations(selected)

    def load_presets_dialog(self):
        """Charge des présets dans les stations"""
        presets = load_presets()
        if not presets:
            QMessageBox.information(self, "Aucun préset", 
                "Aucun préset de station enregistré.\n\n"
                "Utilisez 'Stations > Gérer les présets' pour en créer.")
            return
        
        dialog = PresetManagerDialog(presets, self)
        dialog.setWindowTitle("Charger des présets")
        if dialog.exec() == QDialog.DialogCode.Accepted:
            selected = dialog.get_selected_presets()
            if selected:
                self._load_presets_to_stations(selected)
                save_presets(dialog.get_presets())

    def _load_presets_to_stations(self, presets: List[StationPreset]):
        """Charge les présets sélectionnés comme stations"""
        for preset in presets:
            row = self.stations_table.add_station()
            station = Station(
                callsign=preset.callsign,
                lat=preset.lat,
                lon=preset.lon,
                azimuth=0,
                uncertainty=preset.default_uncertainty,
                color=preset.color,
                visible=True,
                signal="S5"
            )
            self.stations_table.set_station(row, station)
        
        self.status_bar.showMessage(f"{len(presets)} préset(s) chargé(s)")
        self._schedule_update()

    def save_station_as_preset(self):
        """Sauvegarde la première station sélectionnée comme préset"""
        if self.stations_table.rowCount() == 0:
            QMessageBox.information(self, "Aucune station", 
                "Ajoutez d'abord une station à sauvegarder comme préset.")
            return
        
        # Trouver les stations avec un indicatif
        stations_with_data = []
        for row in range(self.stations_table.rowCount()):
            s = self.stations_table.get_station(row)
            if s:
                stations_with_data.append((row, s))
        
        if not stations_with_data:
            QMessageBox.information(self, "Aucune station valide", 
                "Remplissez au moins l'indicatif d'une station.")
            return
        
        # Sélectionner la station
        if len(stations_with_data) == 1:
            row, station = stations_with_data[0]
        else:
            items = [f"{s.callsign} ({dd_to_dms_str(s.lat, 'lat')} {dd_to_dms_str(s.lon, 'lon')})" 
                    for row, s in stations_with_data]
            item, ok = QInputDialog.getItem(self, "Sélectionner station", 
                "Station à sauvegarder:", items, 0, False)
            if not ok:
                return
            idx = items.index(item)
            row, station = stations_with_data[idx]
        
        # Demander le nom du préset
        name, ok = QInputDialog.getText(self, "Nom du préset", 
            "Nom pour ce préset:", text=f"Préset {station.callsign}")
        if not ok or not name:
            return
        
        # Créer et sauvegarder le préset
        preset = StationPreset(
            name=name,
            callsign=station.callsign,
            lat=station.lat,
            lon=station.lon,
            color=station.color,
            default_uncertainty=station.uncertainty
        )
        
        presets = load_presets()
        presets.append(preset)
        save_presets(presets)
        
        QMessageBox.information(self, "Préset sauvegardé", 
            f"Préset '{name}' sauvegardé avec succès!")


def main():
    app = QApplication(sys.argv)
    app.setStyle("Fusion")
    win = MainWindow()
    win.show()
    sys.exit(app.exec())

if __name__ == '__main__':
    main()
