# Changelog

All notable changes to SATER Map. Versions follow `bump_version.sh`.

## 3.0.0 - 2026-10-10

- C++ / Qt 6 Quick rewrite of SATER Map 2 (Python): one code base for
  Windows, Linux (Ubuntu 24.04, .deb package) and Android (APK), one
  interface laid out for a desktop window or a phone.
- Same geometry (bearings, intersections, enclosing circle), checked against
  the Python code; same JSON mission and preset files, same tiles folder
  layout. The Windows installer upgrades version 2 in place.
- MGRS fixed in the even-numbered UTM zones (zone 32 among them): the row
  letter was 500 km off.
- DMS rounding carry fixed (59.999" no longer shows as 60.0").
- The map is drawn natively (no web engine): the same backgrounds, cones,
  dashed bearing lines, zone, points and grid as before.
- New: long press / right click on the map for the coordinates of a point
  (DD, DMS, UTM, MGRS), and to place a station or the beacon there; station
  editor; scale bar; tiles seen online kept in a cache for offline use.
- CARTO Dark, which now needs an API key, replaced by Esri Dark Gray (with its
  place names); a map left on CARTO Dark opens on it.
- New: best estimate of the beacon's position, the least-squares solution of
  all the bearings with its 50 % and 95 % confidence ellipses, and a check
  that the bearings agree within their uncertainty (from four bearings, the
  one that does not is named).
- New: bearings read from magnetic north (a compass), station by station,
  corrected by the WMM2025 declination at the station.
- New: APRS. Field teams send their bearing as a standard APRS DF report;
  the command post receives it over APRS-IS or a KISS TNC on TCP (Dire Wolf)
  and puts it on the map, from the operation's callsigns only. Packet log.
- New: the Android application has a modem built in (Dire Wolf 1.8, through
  the embedding layer of AX25Chess): the bearing goes out by radio through a
  USB sound card or the headset jack, the transmitter keyed by VOX, a CM108
  GPIO or the RTS/DTR line of a USB serial adapter.
- APRS: each SSID of a callsign is a station of its own (a row named without
  SSID becomes the first one heard); this station's own reports heard back
  are left out; a DF report needs the DF symbol; N = 0 reports accepted;
  "DDHHMM/" timestamps read as local time; an APRS-IS server that refuses
  the login says why; a dead KISS connection is noticed (TCP keepalive); a
  new mission starts with no operation callsigns.
- Button texts stay inside their buttons with Qt 6.5 and later (Android,
  Windows): Material 3 sets its own side padding, which the record button's
  label and the N/S, E/W and T/M buttons did not account for. Short tab
  names for the coordinate formats on narrow phones.
- Android: APRS (APRS-IS, KISS, the built-in modem) keeps working with the
  application minimised or the phone locked (foreground service, wake and
  Wi-Fi locks, exemption from battery saving asked); bearings received in
  the background are notified. White splash screens, the system's included.
- PDF report: each section (a heading and its table, the map) whole on one
  page, tables inside the margins on every platform (their widths were in
  pixels, scaled by 4/3 on Android), page numbers; only a table taller than
  a page is cut between rows, its header repeated.
- Fixed with Qt 6.5 and later (Android, Windows): a distance typed in the
  kilometres dialog showed 0 again once the field lost the focus (the value
  was saved); the history and preset selections did not show; the comment
  box of the report dialog drew its text and example over its frame; long
  texts in fields showed their end; the buttons of the map's station popup
  ran past its edge.
- The emblem redrawn from its vector original (window, menus, Android
  launcher and splash screen, Windows icon, PDF report).
- Standard dialog buttons (OK, Cancel, Close, Yes, No) in French too.
- New: themes (light, dark, red for the night with a red-tinted map, amber),
  English and French interface, the organisation printed in the PDF footer
  set in the preferences.
- New: the mission is saved as it changes and restored at start.
