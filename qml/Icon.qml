// Icon.qml - the symbols of the buttons and tabs, drawn rather than taken
// from a font: the phone's fonts do not all have the emoji and arrows the
// Python version used, and a missing glyph shows as an empty box (learned
// on AX25Chat).
//
// This file is part of SATER Map.
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick

Canvas {
    id: icon
    // plus, minus, trash, record, history, car, beacon, map, layers, menu,
    // more, close, play, stop, reset, settings, stations, mission, centre,
    // download, edit, pin, check, back, zone, eye, grid, info
    property string kind
    property color colour: "black"
    implicitWidth: 20
    implicitHeight: 20
    antialiasing: true
    renderTarget: Canvas.Image

    onKindChanged: requestPaint()
    onColourChanged: requestPaint()
    onWidthChanged: requestPaint()
    onHeightChanged: requestPaint()

    onPaint: {
        var c = getContext("2d")
        c.reset()
        var w = width, h = height
        if (w <= 0 || h <= 0) return
        var s = Math.min(w, h)
        c.save()
        c.translate((w - s) / 2, (h - s) / 2)
        c.fillStyle = colour
        c.strokeStyle = colour
        c.lineCap = "round"
        c.lineJoin = "round"
        c.lineWidth = Math.max(1.5, s * 0.1)
        var lw = c.lineWidth

        function line(x1, y1, x2, y2) { c.beginPath(); c.moveTo(x1 * s, y1 * s); c.lineTo(x2 * s, y2 * s); c.stroke() }
        function circle(x, y, r, fill) {
            c.beginPath(); c.arc(x * s, y * s, r * s, 0, 2 * Math.PI, false)
            if (fill) c.fill(); else c.stroke()
        }
        function poly(points, fill) {
            c.beginPath(); c.moveTo(points[0] * s, points[1] * s)
            for (var i = 2; i < points.length; i += 2) c.lineTo(points[i] * s, points[i + 1] * s)
            c.closePath()
            if (fill) c.fill(); else c.stroke()
        }
        function rect(x, y, rw, rh, fill) {
            if (fill) c.fillRect(x * s, y * s, rw * s, rh * s); else c.strokeRect(x * s, y * s, rw * s, rh * s)
        }

        switch (kind) {
        case "plus": line(0.5, 0.15, 0.5, 0.85); line(0.15, 0.5, 0.85, 0.5); break
        case "minus": line(0.15, 0.5, 0.85, 0.5); break
        case "send":
            // A paper plane.
            poly([0.1, 0.46, 0.9, 0.12, 0.62, 0.9, 0.46, 0.56], false)
            line(0.46, 0.56, 0.9, 0.12)
            break
        case "radio":
            // A mast and its waves (APRS).
            line(0.5, 0.42, 0.5, 0.92); line(0.34, 0.92, 0.66, 0.92)
            circle(0.5, 0.34, 0.07, true)
            c.beginPath(); c.arc(0.5 * s, 0.34 * s, 0.2 * s, -Math.PI * 0.8, -Math.PI * 0.2, false); c.stroke()
            c.beginPath(); c.arc(0.5 * s, 0.34 * s, 0.34 * s, -Math.PI * 0.8, -Math.PI * 0.2, false); c.stroke()
            break
        case "trash":
            line(0.15, 0.24, 0.85, 0.24); line(0.38, 0.12, 0.62, 0.12)
            poly([0.24, 0.3, 0.76, 0.3, 0.7, 0.9, 0.3, 0.9], false)
            line(0.42, 0.42, 0.42, 0.78); line(0.58, 0.42, 0.58, 0.78)
            break
        case "record":
            // A notepad and a pencil: "write this bearing down".
            poly([0.16, 0.12, 0.66, 0.12, 0.66, 0.5, 0.42, 0.9, 0.16, 0.9], false)
            line(0.27, 0.32, 0.55, 0.32); line(0.27, 0.5, 0.48, 0.5)
            c.lineWidth = lw * 1.3
            line(0.86, 0.42, 0.56, 0.86)
            break
        case "history":
            c.beginPath(); c.arc(0.52 * s, 0.52 * s, 0.36 * s, -Math.PI * 0.95, Math.PI * 0.8, false); c.stroke()
            poly([0.06, 0.36, 0.24, 0.5, 0.3, 0.3], true)
            line(0.52, 0.3, 0.52, 0.54); line(0.52, 0.54, 0.68, 0.64)
            break
        case "car":
            poly([0.1, 0.62, 0.18, 0.42, 0.3, 0.26, 0.7, 0.26, 0.82, 0.42, 0.9, 0.62, 0.9, 0.74, 0.1, 0.74], false)
            line(0.18, 0.44, 0.82, 0.44)
            circle(0.28, 0.76, 0.08, true); circle(0.72, 0.76, 0.08, true)
            break
        case "beacon":
            circle(0.5, 0.5, 0.14, true)
            circle(0.5, 0.5, 0.3, false)
            c.globalAlpha = 0.6; circle(0.5, 0.5, 0.44, false)
            break
        case "map":
            poly([0.1, 0.22, 0.36, 0.12, 0.64, 0.22, 0.9, 0.12, 0.9, 0.78, 0.64, 0.88, 0.36, 0.78, 0.1, 0.88], false)
            line(0.36, 0.12, 0.36, 0.78); line(0.64, 0.22, 0.64, 0.88)
            break
        case "layers":
            poly([0.5, 0.14, 0.9, 0.34, 0.5, 0.54, 0.1, 0.34], false)
            c.beginPath(); c.moveTo(0.1 * s, 0.52 * s); c.lineTo(0.5 * s, 0.72 * s); c.lineTo(0.9 * s, 0.52 * s); c.stroke()
            c.beginPath(); c.moveTo(0.1 * s, 0.68 * s); c.lineTo(0.5 * s, 0.88 * s); c.lineTo(0.9 * s, 0.68 * s); c.stroke()
            break
        case "menu":
            line(0.14, 0.26, 0.86, 0.26); line(0.14, 0.5, 0.86, 0.5); line(0.14, 0.74, 0.86, 0.74); break
        case "more": circle(0.5, 0.2, 0.08, true); circle(0.5, 0.5, 0.08, true); circle(0.5, 0.8, 0.08, true); break
        case "close": line(0.2, 0.2, 0.8, 0.8); line(0.8, 0.2, 0.2, 0.8); break
        case "play": poly([0.24, 0.12, 0.84, 0.5, 0.24, 0.88], true); break
        case "stop": rect(0.2, 0.2, 0.6, 0.6, true); break
        case "reset":
            c.beginPath(); c.arc(0.5 * s, 0.54 * s, 0.32 * s, -Math.PI * 0.6, Math.PI * 1.25, false); c.stroke()
            poly([0.26, 0.06, 0.48, 0.24, 0.22, 0.34], true)
            break
        case "settings":
            line(0.12, 0.26, 0.88, 0.26); line(0.12, 0.5, 0.88, 0.5); line(0.12, 0.74, 0.88, 0.74)
            circle(0.34, 0.26, 0.09, true); circle(0.66, 0.5, 0.09, true); circle(0.42, 0.74, 0.09, true)
            break
        case "stations":
            // An antenna mast radiating.
            line(0.5, 0.36, 0.5, 0.9); line(0.32, 0.9, 0.68, 0.9)
            circle(0.5, 0.3, 0.07, true)
            c.beginPath(); c.arc(0.5 * s, 0.3 * s, 0.2 * s, -Math.PI * 0.85, -Math.PI * 0.15, false); c.stroke()
            c.beginPath(); c.arc(0.5 * s, 0.3 * s, 0.2 * s, Math.PI * 0.15, Math.PI * 0.85, false); c.stroke()
            break
        case "mission":
            circle(0.5, 0.56, 0.34, false)
            line(0.5, 0.56, 0.5, 0.36); line(0.5, 0.56, 0.66, 0.66)
            line(0.4, 0.1, 0.6, 0.1); line(0.5, 0.1, 0.5, 0.2)
            break
        case "centre":
            circle(0.5, 0.5, 0.28, false); circle(0.5, 0.5, 0.08, true)
            line(0.5, 0.04, 0.5, 0.2); line(0.5, 0.8, 0.5, 0.96); line(0.04, 0.5, 0.2, 0.5); line(0.8, 0.5, 0.96, 0.5)
            break
        case "zone":
            c.setLineDash ? c.setLineDash([2, 2]) : 0
            circle(0.5, 0.5, 0.38, false)
            if (c.setLineDash) c.setLineDash([])
            circle(0.5, 0.5, 0.1, true)
            break
        case "download":
            line(0.5, 0.12, 0.5, 0.66); poly([0.28, 0.48, 0.72, 0.48, 0.5, 0.72], true)
            line(0.14, 0.88, 0.86, 0.88)
            break
        case "edit":
            c.lineWidth = lw * 1.4; line(0.78, 0.18, 0.28, 0.68)
            c.lineWidth = lw; poly([0.22, 0.72, 0.16, 0.86, 0.3, 0.8], true)
            break
        case "pin":
            c.beginPath()
            c.arc(0.5 * s, 0.38 * s, 0.26 * s, Math.PI * 0.82, Math.PI * 0.18, false)
            c.lineTo(0.5 * s, 0.92 * s); c.closePath(); c.fill()
            c.fillStyle = "white"; c.globalCompositeOperation = "destination-out"
            circle(0.5, 0.38, 0.1, true)
            break
        case "check": c.beginPath(); c.moveTo(0.16 * s, 0.52 * s); c.lineTo(0.4 * s, 0.76 * s); c.lineTo(0.86 * s, 0.24 * s); c.stroke(); break
        case "back": c.beginPath(); c.moveTo(0.66 * s, 0.14 * s); c.lineTo(0.3 * s, 0.5 * s); c.lineTo(0.66 * s, 0.86 * s); c.stroke(); break
        case "eye":
            c.beginPath(); c.moveTo(0.06 * s, 0.5 * s)
            c.quadraticCurveTo(0.5 * s, 0.08 * s, 0.94 * s, 0.5 * s)
            c.quadraticCurveTo(0.5 * s, 0.92 * s, 0.06 * s, 0.5 * s); c.stroke()
            circle(0.5, 0.5, 0.13, true)
            break
        case "grid":
            line(0.36, 0.1, 0.36, 0.9); line(0.64, 0.1, 0.64, 0.9); line(0.1, 0.36, 0.9, 0.36); line(0.1, 0.64, 0.9, 0.64)
            break
        case "info":
            circle(0.5, 0.5, 0.4, false); line(0.5, 0.46, 0.5, 0.72); circle(0.5, 0.3, 0.05, true)
            break
        }
        c.restore()
    }
}
