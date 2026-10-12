# SATER Map

Direction finding for SATER missions (search for distress beacons). Several
stations take bearings on a beacon; SATER Map draws each bearing with its
uncertainty cone on a map, computes the zone where they cross, and gives its
centre in decimal degrees, degrees-minutes-seconds and MGRS.

Version 3 is a C++ / Qt 6 Quick rewrite of SATER Map 2 (Python, PyQt6 and
Leaflet). One code base builds for **Windows**, **Linux (Ubuntu 24.04)** and
**Android**; the interface lays itself out as a desktop window (control panel
beside the map, menu bar) or as a phone application (full-screen map, pages
under a tab bar).

## Features

- Stations with callsign, position (DMS or decimal degrees), S-meter signal,
  azimuth and uncertainty, colour and visibility. A station at S0 stays on the
  map without a bearing and takes no part in the zone.
- Bearing lines and uncertainty cones; the intersection points and the
  enclosing circle (radius, surface), its MGRS reference.
- Best estimate: the least-squares position of all the bearings, each
  weighted by its uncertainty, with its 50 % and 95 % confidence ellipses.
  A chi-square test says whether the bearings agree within their cones and,
  from four bearings on, which one does not (a reflection, a misread).
- Magnetic or true north, station by station: a bearing read on a compass
  is corrected by the magnetic declination at the station on the mission's
  date (World Magnetic Model WMM2025, about 3° east in the south of France).
- Distress beacon position: typed in, taken from the zone's centre, or dragged
  on the map.
- Stations and beacon dragged on the map; a right click (desktop) or a long
  press (phone) gives the coordinates of a point and places a station or the
  beacon there.
- APRS: the field teams' bearings come in by themselves. A team sends its
  bearing as a standard APRS DF report (position, true bearing, accuracy,
  plus the S-meter reading and the exact uncertainty in the comment) from
  its station card; the command post receives it over APRS-IS or a KISS TNC
  on the network (Dire Wolf, UZ7HO Soundmodem) and puts it on the map. On
  the phone the modem is built in (Dire Wolf's): an audio interface between
  the phone and the radio is all it takes to send by radio. Only the
  operation's callsigns are taken in (a callsign without SSID stands for
  all its SSIDs, each SSID being a station of its own); a packet log shows
  what came in and what was left out. Other APRS programs (Xastir,
  APRSIS32) plot the same reports.
- Mission timer, history of the recorded bearings (CSV export), kilometres
  driven per station, station presets.
- Map backgrounds: OpenStreetMap, OpenStreetMap France, OpenTopoMap, Esri
  Dark Gray, IGN Plan, IGN Orthophotos, Esri World Imagery; none needs an API
  key. A latitude/longitude grid labelled with the UTM zone.
- Offline use: "Download visible tiles" stores the view and two zoom levels
  above and below; tiles seen once online are also kept in a cache.
- PDF mission report, KML, stand-alone HTML map (Leaflet), PNG, JSON.
- Four themes: light, dark, red (night: the map is tinted red too) and amber.
- English and French interface (French follows the system language, or is
  chosen in the preferences).
- The mission is saved as it changes and restored at the next start.

## Compatibility with SATER Map 2

- **JSON missions** are read and written in the same format, both ways.
- **Presets** are read from the same file: `%LOCALAPPDATA%\SATER_Map\station_presets.json`
  on Windows, `~/.config/SATER_Map/station_presets.json` on Linux.
- **Tiles** use the same layout, `tiles/<provider>/<z>/<x>/<y>.png`. On Windows
  the installer upgrades version 2 in place, and the tiles it downloaded next
  to the program are used as they are. Another folder can be chosen in the
  preferences. The CARTO dark background of version 2, which now needs an API
  key, gives way to Esri Dark Gray (tiles stored under `esri-dark`).
- **Magnetic bearings**: the JSON keeps version 2's keys with the true
  azimuth, so version 2 draws them in the right place; the compass reading
  and its declination are in a `sater_map_3` block version 2 ignores.
- **MGRS**: version 2 left out the row-letter offset of the even-numbered UTM
  zones (zone 32 among them), which put those references 500 km off. Version 3
  gives the correct reference; the geometry is otherwise identical (the tests
  compare it with the Python code).

## APRS in short

| | Command post (PC) | Field team (phone) |
|---|---|---|
| Settings | menu APRS > Operation and connections | Settings page, APRS |
| Callsign | the post's own (receive only needs no passcode) | the team's, with SSID (e.g. F4ABC-7) |
| Operation callsigns | the teams to take bearings from | the same list, to see the other teams |
| Radio | KISS TNC on TCP: Dire Wolf on port 8001 | the built-in modem (USB sound card or headset jack; VOX, CM108 GPIO or serial RTS/DTR for the PTT), or a KISS TNC on TCP |
| Internet | APRS-IS (rotate.aprs2.net:14580) | APRS-IS, with the APRS-IS passcode to send |
| Send a bearing | station editor: "OK and send by APRS" | the "Send" button on the team's own card |

A report reads, for example,
`F4ABC-7>APZSAT,WIDE1-1,WIDE2-1:/111023z4345.48N/00725.08E\000/000/251/965 SATER S7 U5.0`:
taken at 10:23 UTC on the 11th, bearing 251° from true north (a compass
reading is corrected by the declination first), N = 9 (manual), range code
6 (64 miles, from the bearing length), Q = 5 (cone under 16° wide), S7,
±5.0°.

The destination APZSAT is in the experimental range of APRS "tocalls".

On the phone APRS keeps going when SATER Map is minimised or the phone
locked, as long as a connection is on: a foreground service (its
notification says so) keeps the application, the APRS-IS and KISS
connections and the built-in modem's microphone alive; a bearing received
meanwhile shows as a notification. The first time a connection is switched
on, Android asks to allow notifications and to exempt SATER Map from
battery saving, which would otherwise cut the network with the screen off
(the APRS settings show it, with a button to ask again). Swiping SATER Map
away from the recent applications quits it.

A new mission starts with an empty list of operation callsigns; an imported
mission brings its own, if it has one. This station's own reports, heard
back from a digipeater or from APRS-IS, are left out.

## Building

### Linux (Ubuntu 24.04)

```bash
./build_linux.sh --deps --test     # dependencies (sudo), build, tests
./build_linux.sh --run             # build and start
./make_deb.sh                      # sater-map_<version>_<arch>.deb
sudo apt install ./sater-map_*.deb
```

Qt 6.4 from Ubuntu is enough. The package lists the QML modules it needs (no
tool can find them on its own).

### Windows

From a "x64 Native Tools Command Prompt for VS 2022" (or 2026), in the project
folder:

```bat
build_all.bat
```

Needs Visual Studio ("Desktop development with C++", which brings CMake and
Ninja), Qt 6 for MSVC 64-bit (set `QT_DIR` at the top of `build_all.bat`;
default `C:\Qt\6.11.2\msvc2022_64`) and Inno Setup 6 or 7. The result is
`installer\output\SATER_Map-<version>-setup.exe`, bilingual English/French,
with the Visual C++ runtime. Options: `/clean`, `/nobuild`, `/noinstaller`,
`/test`.

The `.bat`, `.iss`, `.rc.in` and `.ps1` files must keep Windows line endings;
`scripts/check_line_endings.sh` and `scripts/check_batch.py` check them.

### Android

From Ubuntu 24.04, the phone connected by USB with USB debugging on:

```bash
./build_android.sh --setup --install   # first time: JDK, SDK, NDK, Qt for Android, signing key
./build_android.sh --install           # then
```

Defaults: Qt 6.11.2, NDK 27.2.12479018, platform 36, all overridable from the
environment (`QT_VERSION=6.10.1 ./build_android.sh ...`). The first configure
fetches KDAB's OpenSSL libraries for Android (HTTPS map tiles) and Google's
Oboe (the modem's audio); the script clones Dire Wolf 1.8 into
`external/direwolf` and patches it (`scripts/fetch_direwolf.sh`, needs `git`
and `patch`). Keep the
signing key (`~/.satermap-android.keystore`): an update signed with another
key is refused. Build with this script rather than from Qt Creator: it
completes Qt's manifest (the background service, the event loop kept
running off screen, the splash screens). The application runs full screen; the system bars come back
with a swipe from the edge.

### The built-in modem elsewhere

The modem is built into the Android application only (`SATER_MODEM`, on by
default for Android). A PC normally reaches its own Dire Wolf or Soundmodem
by KISS TCP; the modem can be built in there too:

```bash
sudo apt install libasound2-dev git patch
scripts/fetch_direwolf.sh
cmake -S . -B build -DSATER_MODEM=ON && cmake --build build
```

On Windows it needs clang-cl for the C sources (`-DCMAKE_C_COMPILER=clang-cl`,
the "C++ Clang tools for Windows" component of Visual Studio).
`SATER_MODEM_LOG=1` copies every line of the modem's log to the program's
output (logcat on Android).

### Versions

`./bump_version.sh x.y.z "change" ["change"...]` sets the version in
`CMakeLists.txt` and adds the entry to `CHANGELOG.md` and the Debian
changelog. The installer, the package and the APK (name and version code)
follow it.

## Tests

```bash
cd build && QT_QPA_PLATFORM=offscreen ctest --output-on-failure
```

- `tst_core` also covers APRS: AX.25 frames and KISS, the DF reports of the
  APRS specification, APRS-IS and a KISS TNC against local servers (a
  refused login, an over-long line), the team filter, duplicates and our
  own reports heard back. Built with `-DSATER_MODEM=ON` on Linux, it sends a
  report through the built-in modem into an audio file (ALSA's file
  plugin) and decodes it back from that audio.
- `tst_core`: the geometry against vectors produced by the Python code
  (`tests/data/gen_python_vectors.py`), MGRS against an independent library,
  the declination against pygeomag (`tests/data/gen_wmm_vectors.py`), the
  least-squares estimate (exact bearings, the minimum, the coverage of its
  ellipses over simulated missions, the wrong bearing found),
  the JSON, KML, HTML and PDF outputs, the mission, the map rendering.
- `tst_ui`: the real QML under pointer and touch events: both layouts, every
  control inside a 360 px wide phone in English and French, the dialogs, the
  map gestures.

`SATER_SCREENSHOT_DIR=<dir> ./build/sater-map` renders the interface with a
sample mission into PNG files (`SATER_SCREENSHOT_LANG=fr` for French).

## Files

| What | Windows | Linux | Android |
|---|---|---|---|
| Presets, saved mission | `%LOCALAPPDATA%\SATER_Map` | `~/.config/SATER_Map` | application storage |
| Built-in modem's `direwolf.conf` (written from the settings) | - | `~/.local/share/SATER_Map` (with `SATER_MODEM`) | application storage |
| Downloaded tiles | `<program>\tiles` | `~/.local/share/SATER_Map/tiles` | application storage |
| Settings | registry `HKCU\Software\SATER_Map` | `~/.config/SATER_Map/SATER_Map.conf` | application storage |

## Licence

GPL-3.0-or-later, see `LICENSE.txt`. The built-in modem is Dire Wolf
(GPL-2.0-or-later, by WB2OSZ) with the embedding layer of AX25Chess, and on
Android Google's Oboe (Apache-2.0). Map tiles belong to their providers
(OpenStreetMap contributors, OpenTopoMap, IGN, Esri), whose usage
policies apply; the attribution is shown on the map.
