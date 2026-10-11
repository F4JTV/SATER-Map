// ReportDialog.qml - the details of the PDF report (title, subject,
// keywords, comments), as the Python dialog; then the file is chosen.
//
// This file is part of SATER Map.
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

AppDialog {
    id: dialog
    title: qsTr("PDF report details")
    preferredWidth: 560
    standardButtons: Dialog.Ok | Dialog.Cancel
    signal detailsAccepted()

    property alias reportTitle: titleField.text
    property alias reportSubject: subjectField.text
    property alias reportKeywords: keywordsField.text
    property alias reportComment: commentArea.text

    onAccepted: detailsAccepted()

    ColumnLayout {
        width: parent.width
        spacing: 8
        GridLayout {
            Layout.fillWidth: true
            columns: 2
            columnSpacing: 10
            rowSpacing: 8
            Label { text: qsTr("Title:") }
            AppField { id: titleField; objectName: "reportTitle"; Layout.fillWidth: true
                       text: qsTr("SATER mission report"); hint: qsTr("Document title") }
            Label { text: qsTr("Subject:") }
            AppField { id: subjectField; Layout.fillWidth: true
                       text: qsTr("Direction finding - Distress beacon search"); hint: qsTr("Document subject") }
            Label { text: qsTr("Keywords:") }
            AppField { id: keywordsField; Layout.fillWidth: true
                       text: qsTr("SATER, ADRASEC, direction finding, beacon, distress, 406MHz")
                       hint: qsTr("Keywords separated by commas") }
        }
        Label { text: qsTr("Comments / Notes:"); font.bold: true }
        TextArea {
            id: commentArea
            objectName: "reportComment"
            Layout.fillWidth: true
            Layout.preferredHeight: 120
            wrapMode: TextEdit.Wrap
            // An example in the empty box only (see AppField.qml).
            placeholderText: length === 0 && !activeFocus ? qsTr("Add your comments, observations or notes on the mission here...") : ""
            background: Rectangle {
                radius: 4
                color: window.mc.field
                border.width: commentArea.activeFocus ? 2 : 1
                border.color: commentArea.activeFocus ? window.Material.accent : window.mc.fieldEdge
            }
        }
    }
}
