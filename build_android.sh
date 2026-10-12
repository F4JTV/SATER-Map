#!/usr/bin/env bash
# ============================================================================
#  SATER Map - Android build from Ubuntu 24.04
#
#  Builds the APK, signs it and installs it on the phone connected by USB.
#  The first time, --setup installs everything needed: the JDK, adb, Qt for
#  Android and the desktop Qt it needs as host tools (aqtinstall in a Python
#  virtual environment), the Android SDK platform, build tools and NDK
#  (sdkmanager), and creates the signing key.  What is already installed is
#  kept, so a Qt installed for another project (AX25Chat...) is reused.
#
#  Keep the signing key: an update must carry the same key as the version it
#  replaces, or Android refuses to install it.
#
#  HTTPS (the map tiles) needs OpenSSL inside the APK: CMake fetches KDAB's
#  prebuilt libraries (github.com/KDAB/android_openssl) at the first
#  configure, so the first build needs a network.  So does the modem built
#  into the application (APRS by radio through an audio interface): the
#  Dire Wolf sources are cloned into external/direwolf and patched by
#  scripts/fetch_direwolf.sh, and Google's Oboe audio library is fetched by
#  CMake.
#
#  Usage:
#    ./build_android.sh --setup --install   first time
#    ./build_android.sh --install           build, sign, install
#    ./build_android.sh --reinstall         ... removing the previous install first
#    ./build_android.sh --logcat            ... and follow the application's log
#    ./build_android.sh --no-sign           stop at the unsigned APK
#    ./build_android.sh --clean             start from an empty build directory
#    ./build_android.sh --help
#
#  Paths and versions are variables below, each overridable from the
#  environment (QT_VERSION=6.10.1 ./build_android.sh ...).
# ============================================================================
set -uo pipefail

SRC_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="$SRC_DIR/build-android"

QT_VERSION="${QT_VERSION:-6.11.2}"
QT_ROOT="${QT_ROOT:-$HOME/Qt}"
QT_ARCH="${QT_ARCH:-android_arm64_v8a}"
ANDROID_ABI="${ANDROID_ABI:-arm64-v8a}"
SDK_ROOT="${ANDROID_SDK_ROOT:-$HOME/Android/Sdk}"
NDK_VERSION="${NDK_VERSION:-27.2.12479018}"
PLATFORM="${ANDROID_PLATFORM:-36}"
BUILD_TOOLS="${ANDROID_BUILD_TOOLS:-35.0.0}"
CMDLINE_TOOLS_URL="${CMDLINE_TOOLS_URL:-https://dl.google.com/android/repository/commandlinetools-linux-11076708_latest.zip}"
KEYSTORE="${KEYSTORE:-$HOME/.satermap-android.keystore}"
KEY_ALIAS="${KEY_ALIAS:-satermap}"
KEY_PASS="${KEY_PASS:-satermap}"
VENV="$SRC_DIR/.venv-android"
PACKAGE="org.satermap.app"

DO_SETUP=0; DO_INSTALL=0; DO_REINSTALL=0; DO_LOGCAT=0; DO_SIGN=1; DO_CLEAN=0

say()  { printf '  %s\n' "$*"; }
step() { printf '\n== %s\n' "$*"; }
die()  { printf '\n[X] %s\n' "$*" >&2; exit 1; }

while [ $# -gt 0 ]; do
    case "$1" in
        --setup)     DO_SETUP=1 ;;
        --install)   DO_INSTALL=1 ;;
        --reinstall) DO_INSTALL=1; DO_REINSTALL=1 ;;
        --logcat)    DO_INSTALL=1; DO_LOGCAT=1 ;;
        --no-sign)   DO_SIGN=0 ;;
        --clean)     DO_CLEAN=1 ;;
        -h|--help)   sed -n '2,30p' "${BASH_SOURCE[0]}" | sed 's/^# \{0,1\}//'; exit 0 ;;
        *) die "Unknown option: $1  (try --help)" ;;
    esac
    shift
done

QT_ANDROID="$QT_ROOT/$QT_VERSION/$QT_ARCH"
QT_HOST="$QT_ROOT/$QT_VERSION/gcc_64"
NDK_ROOT="$SDK_ROOT/ndk/$NDK_VERSION"

# ------------------------------------------------------------------- setup
if [ "$DO_SETUP" -eq 1 ]; then
    step "System packages"
    sudo apt-get update
    sudo apt-get install -y openjdk-17-jdk-headless git patch unzip curl python3-venv adb ninja-build cmake

    step "Qt $QT_VERSION for Android and the host tools (aqtinstall)"
    if [ ! -d "$VENV" ]; then python3 -m venv "$VENV"; fi
    # shellcheck disable=SC1091
    . "$VENV/bin/activate"
    pip install --quiet --upgrade pip aqtinstall
    # Qt Quick, Quick Controls and Qt Network are all in the base package:
    # no -m modules needed.
    if [ ! -x "$QT_HOST/bin/androiddeployqt" ]; then
        aqt install-qt linux desktop "$QT_VERSION" linux_gcc_64 -O "$QT_ROOT"
    fi
    if [ ! -d "$QT_ANDROID/lib" ]; then
        aqt install-qt linux android "$QT_VERSION" "$QT_ARCH" -O "$QT_ROOT"
    fi
    deactivate

    step "Android SDK, build tools and NDK"
    if [ ! -x "$SDK_ROOT/cmdline-tools/latest/bin/sdkmanager" ]; then
        mkdir -p "$SDK_ROOT/cmdline-tools"
        tmp="$(mktemp -d)"
        curl -fsSL -o "$tmp/tools.zip" "$CMDLINE_TOOLS_URL"
        unzip -q "$tmp/tools.zip" -d "$tmp"
        rm -rf "$SDK_ROOT/cmdline-tools/latest"
        mv "$tmp/cmdline-tools" "$SDK_ROOT/cmdline-tools/latest"
        rm -rf "$tmp"
    fi
    yes | "$SDK_ROOT/cmdline-tools/latest/bin/sdkmanager" --sdk_root="$SDK_ROOT" --licenses >/dev/null || true
    "$SDK_ROOT/cmdline-tools/latest/bin/sdkmanager" --sdk_root="$SDK_ROOT" \
        "platform-tools" "platforms;android-$PLATFORM" "build-tools;$BUILD_TOOLS" "ndk;$NDK_VERSION" \
        || die "sdkmanager failed."

    step "Signing key"
    if [ ! -f "$KEYSTORE" ]; then
        keytool -genkeypair -v -keystore "$KEYSTORE" -alias "$KEY_ALIAS" -keyalg RSA -keysize 2048 \
            -validity 10000 -storepass "$KEY_PASS" -keypass "$KEY_PASS" -dname "CN=SATER Map" \
            || die "keytool failed."
        say "Created $KEYSTORE - keep it, updates must be signed with the same key."
    else
        say "Using $KEYSTORE"
    fi
fi

# ------------------------------------------------------------- prerequisites
step "Checking prerequisites"
[ -x "$QT_ANDROID/bin/qt-cmake" ] || die "Qt for Android not found at $QT_ANDROID (run --setup, or set QT_ROOT/QT_VERSION/QT_ARCH)."
[ -d "$QT_HOST/bin" ] || die "Host Qt not found at $QT_HOST (run --setup)."
[ -d "$QT_ANDROID/lib/cmake/Qt6Quick" ] || die "Qt Quick missing from $QT_ANDROID: reinstall the base package (--setup)."
# API 36 is not a preference: Qt 6.11 drives Android Gradle Plugin 9, which
# pulls androidx.core 1.17, which refuses anything older (RemoteRig).
[ -d "$SDK_ROOT/platforms/android-$PLATFORM" ] || die "Android platform $PLATFORM not found (run --setup)."
# A mismatched NDK gives no readable error, only undefined symbols at link.
[ -d "$NDK_ROOT" ] || die "NDK $NDK_VERSION not found in $SDK_ROOT/ndk (run --setup). It must be the one Qt $QT_VERSION was built with."
command -v java >/dev/null 2>&1 || die "java missing: sudo apt install openjdk-17-jdk-headless"
command -v python3 >/dev/null 2>&1 || die "python3 missing: sudo apt install python3"
command -v git >/dev/null 2>&1 || die "git missing: sudo apt install git"
command -v patch >/dev/null 2>&1 || die "patch missing: sudo apt install patch"
# Every time: it clones only when the tree is missing, and applies any patch
# a newer release added to a tree patched by an older one.
"$SRC_DIR/scripts/fetch_direwolf.sh" >/dev/null || die "Could not fetch or patch the Dire Wolf sources (scripts/fetch_direwolf.sh)."
VERSION="$(sed -n 's/^project(SaterMap VERSION \([0-9.]*\).*/\1/p' "$SRC_DIR/CMakeLists.txt" | head -1)"
[ -n "$VERSION" ] || die "No version found in CMakeLists.txt."
say "Version     $VERSION"
say "Qt          $QT_ANDROID"
say "NDK         $NDK_ROOT"
say "ABI         $ANDROID_ABI"

# ------------------------------------------------------------------ manifest
# Qt's own template for this Qt version, with the splash screen and the
# icon added (Qt reads android.app.splash_screen_drawable from the
# activity's meta-data; there is no CMake property for it).  Generated on
# every run so it never lags behind the Qt installed.
step "Manifest"
TEMPLATE="$QT_ANDROID/src/android/templates/AndroidManifest.xml"
GENERATED="$SRC_DIR/android/AndroidManifest.xml"
rm -f "$GENERATED"
[ -f "$TEMPLATE" ] || die "No manifest template in $QT_ANDROID/src/android/templates."
python3 - "$TEMPLATE" "$GENERATED" <<'PYEOF'
import re, sys
import xml.etree.ElementTree as ET
template, target = sys.argv[1], sys.argv[2]
text = open(template, encoding="utf-8").read()
ANDROID = "{http://schemas.android.com/apk/res/android}"

def set_attr(tag, name, value):
    pattern = re.compile(r'(\s)' + re.escape(name) + r'="[^"]*"')
    if pattern.search(tag):
        return pattern.sub(lambda m: m.group(1) + name + '="' + value + '"', tag, count=1)
    head = re.match(r"<[\w.]+", tag)
    return tag[:head.end()] + " " + name + '="' + value + '"' + tag[head.end():]

# The launcher icon, whatever the Qt version does with QT_ANDROID_APP_ICON.
app = re.search(r'<application\b[^>]*>', text, re.S)
if app is None:
    sys.exit("no <application> in the template")
app_tag = set_attr(app.group(0), "android:icon", "@drawable/icon")
# Back has to reach Qt as a key press (Main.qml's onClosing closes the
# dialogs and goes back through the pages).  Android 16 no longer sends it
# to an application targeting SDK 36 unless the application opts out of the
# predictive back callback; the templates of Qt 6.10 and later do, older
# ones say nothing.  A value the template sets is left alone.
if "android:enableOnBackInvokedCallback" not in app_tag:
    app_tag = set_attr(app_tag, "android:enableOnBackInvokedCallback", "false")
text = text[:app.start()] + app_tag + text[app.end():]

# The activity's theme: a white window behind the splash screens (Android
# 12 and later draw one of their own first, in the device's colours unless
# told otherwise) - android/res/values*/styles.xml.
activity = re.search(r'<activity\b[^>]*>', text, re.S)
if activity is None:
    sys.exit("no <activity> in the template")
text = text[:activity.start()] + set_attr(activity.group(0), "android:theme", "@style/SaterTheme") + text[activity.end():]

# The splash meta-data belongs to the activity: just before its closing
# tag, whatever the layout of the template (Qt 6.11 spreads its meta-data
# over several lines).
marker = re.search(r'^([ \t]*)</activity>', text, re.M)
if marker is None:
    sys.exit("no </activity> in the template")
indent = marker.group(1) + "    "
insert = (f"{indent}<!-- Added by build_android.sh: the splash screen, android/res/drawable/splash.xml. -->\n"
          f'{indent}<meta-data android:name="android.app.splash_screen_drawable" android:resource="@drawable/splash"/>\n'
          f'{indent}<meta-data android:name="android.app.splash_screen_sticky" android:value="false"/>\n')
# Qt suspends an application's event loop when its activity leaves the
# screen unless background_running is true: every timer and socket on the
# Qt thread would stop the moment the phone locks - APRS-IS and the modem's
# frames among them (learned in AX25Chat).  True, whether the template
# carries the entry or not.
bg = re.compile(r'(<meta-data\s+android:name="android.app.background_running"\s+android:value=")false(")', re.S)
if bg.search(text):
    text = bg.sub(r'\1true\2', text)
    marker = re.search(r'^([ \t]*)</activity>', text, re.M)
elif 'android.app.background_running' not in text:
    insert += (f"{indent}<!-- Added by build_android.sh: keep the Qt event loop running off screen. -->\n"
               f'{indent}<meta-data android:name="android.app.background_running" android:value="true"/>\n')
text = text[:marker.start()] + insert + text[marker.start():]

# The foreground service that keeps APRS going in the background
# (android/src/org/satermap/app/KeepAliveService.java), with every type it
# may use; it picks at run time those whose permissions were granted.
app_end = re.search(r'^([ \t]*)</application>', text, re.M)
if app_end is None:
    sys.exit("no </application> in the template")
indent = app_end.group(1) + "    "
service = (f"{indent}<!-- Added by build_android.sh: android/src/org/satermap/app/KeepAliveService.java. -->\n"
           f'{indent}<service android:name="org.satermap.app.KeepAliveService" android:exported="false"\n'
           f'{indent}         android:foregroundServiceType="microphone|specialUse|dataSync">\n'
           f'{indent}    <property android:name="android.app.PROPERTY_SPECIAL_USE_FGS_SUBTYPE"\n'
           f'{indent}              android:value="Amateur radio APRS link kept open for a distress beacon search (bearings sent and received)"/>\n'
           f'{indent}</service>\n')
text = text[:app_end.start()] + service + text[app_end.start():]

# Refuse to hand androiddeployqt something it cannot parse.
root = ET.fromstring(text.encode("utf-8"))
application = root.find("application")
if application.get(ANDROID + "icon") != "@drawable/icon":
    sys.exit("the icon did not make it into the manifest")
if application.get(ANDROID + "enableOnBackInvokedCallback") is None:
    sys.exit("enableOnBackInvokedCallback did not make it into the manifest")
if "%%INSERT_PERMISSIONS" not in text:
    sys.exit("the permissions marker androiddeployqt fills in is gone")
if 'android.app.background_running" android:value="true"' not in text:
    sys.exit("background_running did not make it into the manifest")
if root.find("application/service") is None:
    sys.exit("the foreground service did not make it into the manifest")
open(target, "w", encoding="utf-8").write(text)
PYEOF
[ -f "$GENERATED" ] || die "Could not derive the manifest from $TEMPLATE."
say "android/AndroidManifest.xml from Qt $QT_VERSION's template"

if [ "$DO_CLEAN" -eq 1 ] && [ -d "$BUILD_DIR" ]; then
    step "Cleaning"
    rm -rf "$BUILD_DIR"
fi

# --------------------------------------------------------------------- build
step "Configuring"
SIGN_ARGS=()
if [ "$DO_SIGN" -eq 1 ]; then
    [ -f "$KEYSTORE" ] || die "Signing key $KEYSTORE not found (run --setup, or --no-sign)."
    export QT_ANDROID_KEYSTORE_PATH="$KEYSTORE"
    export QT_ANDROID_KEYSTORE_ALIAS="$KEY_ALIAS"
    export QT_ANDROID_KEYSTORE_STORE_PASS="$KEY_PASS"
    export QT_ANDROID_KEYSTORE_KEY_PASS="$KEY_PASS"
    SIGN_ARGS=(-DQT_ANDROID_SIGN_APK=ON)
fi
"$QT_ANDROID/bin/qt-cmake" -S "$SRC_DIR" -B "$BUILD_DIR" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DANDROID_SDK_ROOT="$SDK_ROOT" \
    -DANDROID_NDK_ROOT="$NDK_ROOT" \
    -DANDROID_ABI="$ANDROID_ABI" \
    -DQT_HOST_PATH="$QT_HOST" \
    -DSATER_BUILD_TESTS=OFF \
    "${SIGN_ARGS[@]}" || die "Configuration failed."

step "Compiling and packaging"
cmake --build "$BUILD_DIR" --target apk || die "Build failed."

# Careful with the pattern: "unsigned" contains "signed" (RemoteRig).
if [ "$DO_SIGN" -eq 1 ]; then
    APK="$(find "$BUILD_DIR/android-build" -name '*-signed.apk' ! -name '*-unsigned.apk' -printf '%T@ %p\n' 2>/dev/null | sort -rn | head -1 | cut -d' ' -f2-)"
    [ -n "$APK" ] || die "No signed APK produced (is the keystore password right?)."
else
    APK="$(find "$BUILD_DIR/android-build" -name '*.apk' -printf '%T@ %p\n' 2>/dev/null | sort -rn | head -1 | cut -d' ' -f2-)"
    [ -n "$APK" ] || die "No APK produced."
fi
# Named like the other installers: SATER_Map-<version>-<abi>.apk, next to
# SATER_Map-<version>-setup.exe and sater-map_<version>_<arch>.deb.
OUT="$SRC_DIR/SATER_Map-$VERSION-$ANDROID_ABI.apk"
cp -f "$APK" "$OUT"
step "Result"
say "$OUT ($(du -h "$OUT" | cut -f1))"

# ------------------------------------------------------------------- install
if [ "$DO_INSTALL" -eq 1 ]; then
    step "Installing on the phone"
    command -v adb >/dev/null 2>&1 || die "adb missing: sudo apt install adb"
    adb devices | grep -q -w device || die "No phone connected (USB debugging on, and authorised?)."
    if [ "$DO_REINSTALL" -eq 1 ]; then adb uninstall "$PACKAGE" >/dev/null 2>&1 || true; fi
    adb install -r "$OUT" || die "adb install failed."
    adb shell monkey -p "$PACKAGE" -c android.intent.category.LAUNCHER 1 >/dev/null 2>&1 || true
    say "Installed and started."
    if [ "$DO_LOGCAT" -eq 1 ]; then
        step "Log (Ctrl-C to stop)"
        adb logcat -c
        adb logcat | grep -E "sater|SaterMap|SATER|qml|AndroidRuntime|libc"
    fi
fi
