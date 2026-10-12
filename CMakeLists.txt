# ============================================================================
#  SATER Map - direction finding for SATER missions, C++/Qt 6 Quick
#
#  Linux (Ubuntu 24.04, Qt 6.4+):  ./build_linux.sh  (a package: ./make_deb.sh)
#  Windows (Visual Studio, Qt 6):  build_all.bat     (x64 Native Tools prompt)
#  Android (Qt 6.8+):              ./build_android.sh --install
# ============================================================================
cmake_minimum_required(VERSION 3.21)

project(SaterMap VERSION 3.0.0 DESCRIPTION "Direction finding for SATER missions" LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

option(SATER_BUILD_TESTS "Build the tests (desktop only)" ON)
# The modem built into the program: Dire Wolf's core (external/direwolf,
# fetched by scripts/fetch_direwolf.sh), so that the phone sends its
# bearings by radio through an audio interface alone.  On a PC, Dire Wolf
# or Soundmodem of its own is reached by KISS TCP; the option builds there
# too (Linux: libasound2-dev; Windows: clang-cl for the C sources).
if(ANDROID)
    set(_sater_modem_default ON)
else()
    set(_sater_modem_default OFF)
endif()
option(SATER_MODEM "Build the Dire Wolf modem into the program" ${_sater_modem_default})

if(MSVC)
    # The sources are UTF-8 (degree signs, the French catalog): say so to
    # the compiler rather than let it guess from the code page.  M_PI needs
    # _USE_MATH_DEFINES with Microsoft's math.h.
    add_compile_options("$<$<COMPILE_LANGUAGE:CXX>:/utf-8>")
    add_compile_definitions(_USE_MATH_DEFINES _CRT_SECURE_NO_WARNINGS)
endif()

if(SATER_MODEM)
    enable_language(C)
    set(CMAKE_C_STANDARD 11)
    set(CMAKE_C_STANDARD_REQUIRED ON)
    include(cmake/DirewolfCore.cmake)
endif()

# Qt 6.4 is what Ubuntu 24.04 ships: nothing here, in C++ or in QML, needs
# more than that.  Android and Windows use a newer Qt from qt.io.
find_package(Qt6 6.4 REQUIRED COMPONENTS Core Gui Network Qml Quick QuickControls2)
qt_standard_project_setup()
# Android deployment paths are plain strings here; say so to Qt.
if(QT_KNOWN_POLICY_QTP0002)
    qt_policy(SET QTP0002 NEW)
endif()

# ---- the application code, shared by the program and the tests -------------
add_library(sater_core STATIC
    src/aprs/AprsIsClient.cpp src/aprs/AprsIsClient.h
    src/aprs/AprsPort.cpp src/aprs/AprsPort.h
    src/aprs/Ax25.cpp src/aprs/Ax25.h
    src/aprs/DfReport.cpp src/aprs/DfReport.h
    src/aprs/Kiss.cpp src/aprs/Kiss.h
    src/aprs/KissTcpClient.cpp src/aprs/KissTcpClient.h
    src/aprs/ModemSettings.cpp src/aprs/ModemSettings.h
    src/core/Analysis.h
    src/core/Exporters.cpp src/core/Exporters.h
    src/core/Fix.cpp src/core/Fix.h
    src/core/Geo.cpp src/core/Geo.h
    src/core/Geomag.cpp src/core/Geomag.h
    src/core/MissionData.h
    src/core/MissionIO.cpp src/core/MissionIO.h
    src/core/ReportWriter.cpp src/core/ReportWriter.h
    src/core/TileProviders.cpp src/core/TileProviders.h
    src/core/Tr.h
    src/map/MapView.cpp src/map/MapView.h
    src/map/TileManager.cpp src/map/TileManager.h
    src/app/AppController.cpp src/app/AppController.h
    src/app/AprsLink.cpp src/app/AprsLink.h
    src/app/HistoryModel.cpp src/app/HistoryModel.h
    src/app/Mission.cpp src/app/Mission.h
    src/app/PresetModel.cpp src/app/PresetModel.h
    src/app/StationModel.cpp src/app/StationModel.h
    src/i18n/Translator.cpp src/i18n/Translator.h src/i18n/catalog_fr.inc
    src/platform/AndroidUi.cpp src/platform/AndroidUi.h
    src/platform/Screenshots.cpp src/platform/Screenshots.h
)
target_include_directories(sater_core PUBLIC src)
target_compile_definitions(sater_core PUBLIC SATER_VERSION="${PROJECT_VERSION}")
target_link_libraries(sater_core PUBLIC
    Qt6::Core Qt6::Gui Qt6::Network Qt6::Qml Qt6::Quick Qt6::QuickControls2)
if(WIN32)
    # setsockopt() for the KISS connection's keepalive (KissTcpClient.cpp).
    target_link_libraries(sater_core PRIVATE ws2_32)
endif()
if(SATER_MODEM)
    target_sources(sater_core PRIVATE
        src/aprs/ModemPort.cpp src/aprs/ModemPort.h
        src/modem/DirewolfEngine.cpp src/modem/DirewolfEngine.h
        src/modem/AndroidPtt.cpp src/modem/AndroidPtt.h)
    target_compile_definitions(sater_core PUBLIC SATER_MODEM=1)
    target_link_libraries(sater_core PUBLIC direwolf_core)
    # Dire Wolf's tables, written to the data folder when the modem starts
    # (ModemPort.cpp): nothing to install beside the program.
    qt_add_resources(sater_core "direwolf_data" PREFIX "/direwolf-data" BASE ${DIREWOLF_DIR}/data
        FILES ${DIREWOLF_DIR}/data/tocalls.yaml ${DIREWOLF_DIR}/data/symbols-new.txt)
endif()
# The logo: window icon, PDF report, About box.
qt_add_resources(sater_core "sater_assets"
    PREFIX "/"
    FILES assets/logo.png
)

# ---- the program ---------------------------------------------------------------
set(SATER_RC "")
if(WIN32)
    # Icon and version details of SATER_Map.exe (Properties > Details).
    string(REPLACE "." "," SATER_VERSION_COMMAS "${PROJECT_VERSION}")
    configure_file(assets/sater_map.rc.in ${CMAKE_BINARY_DIR}/sater_map.rc @ONLY)
    set(SATER_RC ${CMAKE_BINARY_DIR}/sater_map.rc)
endif()

qt_add_executable(sater-map src/main.cpp ${SATER_RC})

set(SATER_QML_FILES
    qml/Main.qml
    qml/DesktopLayout.qml
    qml/PhoneLayout.qml
    qml/MapPane.qml
    qml/StationTable.qml
    qml/StationCard.qml
    qml/StationEditor.qml
    qml/TimerView.qml
    qml/InfoView.qml
    qml/MapSettingsView.qml
    qml/PreferencesView.qml
    qml/HistoryView.qml
    qml/KilometersView.qml
    qml/BeaconDialog.qml
    qml/PresetsDialog.qml
    qml/PresetEditDialog.qml
    qml/SavePresetDialog.qml
    qml/ReportDialog.qml
    qml/TileDownloadDialog.qml
    qml/AboutDialog.qml
    qml/AlertDialog.qml
    qml/AppButton.qml
    qml/AppField.qml
    qml/AppTextArea.qml
    qml/AprsLogDialog.qml
    qml/AprsStatus.qml
    qml/AprsView.qml
    qml/DirButton.qml
    qml/NorthButton.qml
    qml/AppComboBox.qml
    qml/AppMenu.qml
    qml/AppDialog.qml
    qml/ColorButton.qml
    qml/ColorPicker.qml
    qml/NumberField.qml
    qml/DmsEditor.qml
    qml/Icon.qml
    qml/IconButton.qml
    qml/Card.qml
    qml/SoftShadow.qml
    qml/Toast.qml
)
# The files sit in qml/ but must land at the module's root
# (qrc:/qt/qml/SaterMap/Main.qml).  Loading by URL works on every Qt 6;
# loadFromModule() only from 6.5, and Ubuntu 24.04 ships 6.4.
foreach(_qml ${SATER_QML_FILES})
    get_filename_component(_name ${_qml} NAME)
    set_source_files_properties(${_qml} PROPERTIES QT_RESOURCE_ALIAS ${_name})
endforeach()

qt_add_qml_module(sater-map
    URI SaterMap
    VERSION 1.0
    RESOURCE_PREFIX /qt/qml
    QML_FILES ${SATER_QML_FILES}
)
target_link_libraries(sater-map PRIVATE sater_core)

if(WIN32)
    set_target_properties(sater-map PROPERTIES WIN32_EXECUTABLE ON OUTPUT_NAME "SATER_Map")
elseif(ANDROID)
    # The version code is what Android compares from one install to the
    # next: 3.0.0 -> 30000, 3.1.12 -> 30112.
    math(EXPR SATER_VERSION_CODE
        "${PROJECT_VERSION_MAJOR} * 10000 + ${PROJECT_VERSION_MINOR} * 100 + ${PROJECT_VERSION_PATCH}")
    # android/ holds the launcher icon and the splash screen; the manifest
    # is Qt's own template, filled from these properties and completed by
    # build_android.sh.
    set_target_properties(sater-map PROPERTIES
        QT_ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_SOURCE_DIR}/android"
        QT_ANDROID_PACKAGE_NAME "org.satermap.app"
        QT_ANDROID_APP_NAME "SATER Map"
        QT_ANDROID_APP_ICON "@drawable/icon"
        QT_ANDROID_MIN_SDK_VERSION 28
        QT_ANDROID_TARGET_SDK_VERSION 36
        QT_ANDROID_VERSION_NAME "${PROJECT_VERSION}"
        QT_ANDROID_VERSION_CODE ${SATER_VERSION_CODE})
    if(COMMAND qt_add_android_permission)
        qt_add_android_permission(sater-map NAME android.permission.INTERNET)
        qt_add_android_permission(sater-map NAME android.permission.ACCESS_NETWORK_STATE)
        # APRS goes on minimised or with the phone locked: a foreground
        # service (KeepAliveService.java, declared by build_android.sh), a
        # wake lock, a Wi-Fi lock, its notification, and the exemption from
        # battery saving asked of the user.
        qt_add_android_permission(sater-map NAME android.permission.FOREGROUND_SERVICE)
        qt_add_android_permission(sater-map NAME android.permission.FOREGROUND_SERVICE_SPECIAL_USE)
        qt_add_android_permission(sater-map NAME android.permission.WAKE_LOCK)
        qt_add_android_permission(sater-map NAME android.permission.ACCESS_WIFI_STATE)
        qt_add_android_permission(sater-map NAME android.permission.POST_NOTIFICATIONS)
        qt_add_android_permission(sater-map NAME android.permission.REQUEST_IGNORE_BATTERY_OPTIMIZATIONS)
        if(SATER_MODEM)
            # The built-in modem listens to the radio, also in the
            # background; USB devices (the PTT) are granted per device at
            # run time, no permission here.
            qt_add_android_permission(sater-map NAME android.permission.RECORD_AUDIO)
            qt_add_android_permission(sater-map NAME android.permission.MODIFY_AUDIO_SETTINGS)
            qt_add_android_permission(sater-map NAME android.permission.FOREGROUND_SERVICE_MICROPHONE)
        endif()
    else()
        message(WARNING "This Qt has no qt_add_android_permission(): add INTERNET and RECORD_AUDIO to the manifest by hand.")
    endif()
    # HTTPS for the map tiles: Qt for Android has no TLS of its own, the
    # OpenSSL libraries go in the APK.  Without them every https request
    # fails silently - the map stays grey.  KDAB keeps prebuilt libraries
    # for every ABI (AX25Chat's way).
    include(FetchContent)
    FetchContent_Declare(android_openssl
        GIT_REPOSITORY https://github.com/KDAB/android_openssl.git
        GIT_TAG master
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(android_openssl)
    if(EXISTS ${android_openssl_SOURCE_DIR}/android_openssl.cmake)
        include(${android_openssl_SOURCE_DIR}/android_openssl.cmake)
        add_android_openssl_libraries(sater-map)
        message(STATUS "OpenSSL for Android bundled from ${android_openssl_SOURCE_DIR}")
    else()
        message(WARNING "android_openssl.cmake not found: HTTPS (map tiles) will not work on the phone")
    endif()
endif()

# ---- install, and the Debian package ---------------------------------------------
# Not on Android: there the program is a shared library that androiddeployqt
# puts in the APK, and install() would want a LIBRARY destination.
if(UNIX AND NOT APPLE AND NOT ANDROID)
    include(GNUInstallDirs)
    install(TARGETS sater-map RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
    set(BINDIR ${CMAKE_INSTALL_FULL_BINDIR})
    configure_file(packaging/desktop/sater-map.desktop.in ${CMAKE_BINARY_DIR}/sater-map.desktop @ONLY)
    install(FILES ${CMAKE_BINARY_DIR}/sater-map.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
    foreach(_size 48 64 128 256)
        install(FILES packaging/icons/${_size}x${_size}/sater-map.png
                DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/${_size}x${_size}/apps)
    endforeach()
    string(TIMESTAMP SATER_RELEASE_DATE "%Y-%m-%d" UTC)
    configure_file(packaging/appstream/io.github.satermap.SaterMap.metainfo.xml.in
                   ${CMAKE_BINARY_DIR}/io.github.satermap.SaterMap.metainfo.xml @ONLY)
    install(FILES ${CMAKE_BINARY_DIR}/io.github.satermap.SaterMap.metainfo.xml
            DESTINATION ${CMAKE_INSTALL_DATADIR}/metainfo)
    install(FILES README.md CHANGELOG.md DESTINATION ${CMAKE_INSTALL_DATADIR}/doc/sater-map)
    install(FILES packaging/deb/copyright DESTINATION ${CMAKE_INSTALL_DATADIR}/doc/sater-map)
    # Manual page and changelog, compressed as Debian policy asks (a native
    # package, so the file is changelog.gz, not changelog.Debian.gz).
    find_program(GZIP gzip)
    if(GZIP)
        add_custom_command(OUTPUT ${CMAKE_BINARY_DIR}/sater-map.1.gz
            COMMAND ${GZIP} -9 -n -c ${CMAKE_SOURCE_DIR}/packaging/man/sater-map.1 > ${CMAKE_BINARY_DIR}/sater-map.1.gz
            DEPENDS ${CMAKE_SOURCE_DIR}/packaging/man/sater-map.1 VERBATIM)
        add_custom_command(OUTPUT ${CMAKE_BINARY_DIR}/changelog.gz
            COMMAND ${GZIP} -9 -n -c ${CMAKE_SOURCE_DIR}/packaging/deb/changelog > ${CMAKE_BINARY_DIR}/changelog.gz
            DEPENDS ${CMAKE_SOURCE_DIR}/packaging/deb/changelog VERBATIM)
        add_custom_target(compressed_docs ALL
            DEPENDS ${CMAKE_BINARY_DIR}/sater-map.1.gz ${CMAKE_BINARY_DIR}/changelog.gz)
        install(FILES ${CMAKE_BINARY_DIR}/sater-map.1.gz DESTINATION ${CMAKE_INSTALL_MANDIR}/man1)
        install(FILES ${CMAKE_BINARY_DIR}/changelog.gz DESTINATION ${CMAKE_INSTALL_DATADIR}/doc/sater-map)
    endif()

    # Dependencies are worked out by dpkg-shlibdeps from the libraries the
    # program links.  The QML modules are loaded at run time and linked by
    # nothing, so no tool can see them: they are listed here, or the
    # installed program opens an empty window and says nothing.
    set(CPACK_PACKAGE_NAME "sater-map")
    set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
    set(CPACK_PACKAGE_CONTACT "SATER Map contributors <noreply@satermap.invalid>")
    set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Direction finding map for distress beacon searches (SATER)")
    set(CPACK_PACKAGE_DESCRIPTION
"Several stations take bearings on a distress beacon; SATER Map draws\n\
 each bearing with its uncertainty cone on a map and computes the zone\n\
 where they cross, with its MGRS reference.  Mission timer, bearing\n\
 history, kilometres driven, beacon position, offline map tiles, and\n\
 exports to PDF report, KML, JSON and stand-alone HTML.")
    set(CPACK_STRIP_FILES ON)
    set(CPACK_PACKAGING_INSTALL_PREFIX "/usr")
    set(CPACK_GENERATOR "DEB")
    set(CPACK_DEBIAN_PACKAGE_SECTION "hamradio")
    set(CPACK_DEBIAN_PACKAGE_PRIORITY "optional")
    set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
    set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
    set(CPACK_DEBIAN_PACKAGE_DEPENDS
        "qml6-module-qtquick, qml6-module-qtquick-controls, qml6-module-qtquick-layouts, qml6-module-qtquick-templates, qml6-module-qtquick-window, qml6-module-qtquick-dialogs, qml6-module-qtqml-workerscript, qml6-module-qtqml-models, qml6-module-qt-labs-folderlistmodel")
    # The native GTK file dialogs on GNOME instead of Qt's own.
    set(CPACK_DEBIAN_PACKAGE_RECOMMENDS "qt6-gtk-platformtheme")
    set(CPACK_DEBIAN_COMPRESSION_TYPE "xz")
    include(CPack)
elseif(WIN32)
    install(TARGETS sater-map RUNTIME DESTINATION .)
endif()

# ---- tests ------------------------------------------------------------------------
if(SATER_BUILD_TESTS AND NOT ANDROID)
    find_package(Qt6 COMPONENTS Test)
    if(Qt6Test_FOUND)
        enable_testing()
        # Geometry and file formats checked against vectors produced by the
        # Python version (tests/data/python_vectors.json).
        qt_add_executable(tst_core tests/tst_core.cpp)
        target_link_libraries(tst_core PRIVATE sater_core Qt6::Test)
        target_compile_definitions(tst_core PRIVATE SATER_TEST_DATA="${CMAKE_SOURCE_DIR}/tests/data")
        add_test(NAME tst_core COMMAND tst_core)
        set_tests_properties(tst_core PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

        # The real QML, driven headless: both layouts, dialogs, gestures.
        qt_add_executable(tst_ui tests/tst_ui.cpp)
        target_link_libraries(tst_ui PRIVATE sater_core Qt6::Test)
        target_compile_definitions(tst_ui PRIVATE SATER_QML_DIR="${CMAKE_SOURCE_DIR}/qml")
        add_test(NAME tst_ui COMMAND tst_ui)
        set_tests_properties(tst_ui PROPERTIES
            ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software")
    endif()
endif()
