import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: control

    property string symbol
    property string name
    property string formattedPrice
    property string formattedChangePercent
    property bool isGaining
    property bool isLosing
    property color pageColor: "#101010"
    property color inkColor: "#eeeeee"
    property color gainColor: control.inkColor
    property color lossColor: control.inkColor
    property color neutralColor: control.inkColor
    property real uiScale: 1.0

    signal activated()

    function scaledSize(pixels) {
        return Math.max(1, Math.round(pixels * uiScale));
    }

    function mixColors(base, tint, amount) {
        return Qt.rgba(
            base.r + (tint.r - base.r) * amount,
            base.g + (tint.g - base.g) * amount,
            base.b + (tint.b - base.b) * amount, 1);
    }

    readonly property color cardBase: mixColors(pageColor, inkColor, 0.045)
    readonly property color cardHover: mixColors(pageColor, inkColor, 0.10)
    readonly property color cardPress: mixColors(pageColor, inkColor, 0.14)

    height: scaledSize(72)
    color: mouseArea.containsMouse && !mouseArea.pressed ? cardHover : cardBase
    border.width: 1
    border.color: mixColors(pageColor, inkColor, 0.08)

    Behavior on color { ColorAnimation { duration: 100 } }

    RowLayout {
        id: row
        anchors.fill: parent
        anchors.leftMargin: control.scaledSize(16)
        anchors.rightMargin: control.scaledSize(12)
        anchors.verticalCenter: parent.verticalCenter
        spacing: control.scaledSize(12)

        Column {
            Layout.alignment: Qt.AlignLeft | Qt.AlignVCenter
            Layout.fillWidth: true

            Text {
                text: control.symbol
                color: control.inkColor
                font.family: "iA Writer Mono S"
                font.pixelSize: control.scaledSize(18)
                font.bold: true
                elide: Text.ElideRight
                width: parent.width
            }

            Text {
                text: control.name ? control.name : control.symbol
                color: mixColors(control.pageColor, control.inkColor, 0.55)
                font.family: "iA Writer Mono S"
                font.pixelSize: control.scaledSize(13)
                elide: Text.ElideRight
                width: parent.width
            }
        }

        Column {
            Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
            spacing: control.scaledSize(2)

            Text {
                id: priceText
                text: control.formattedPrice
                color: control.inkColor
                font.family: "iA Writer Mono S"
                font.pixelSize: control.scaledSize(16)
                font.bold: true
                horizontalAlignment: Text.AlignRight
            }

            Rectangle {
                color: control.isGaining
                    ? Qt.rgba(0.20, 0.78, 0.35, 0.20)
                    : control.isLosing
                        ? Qt.rgba(0.98, 0.23, 0.19, 0.18)
                        : mixColors(control.pageColor, control.inkColor, 0.12)
                width: priceText.width
                height: percentText.height + control.scaledSize(6)

                Text {
                    id: percentText
                    anchors.right: parent.right
                    anchors.rightMargin: control.scaledSize(5)
                    anchors.verticalCenter: parent.verticalCenter
                    text: control.formattedChangePercent
                    color: control.isGaining
                        ? Qt.rgba(0.30, 0.85, 0.40, 1)
                        : control.isLosing
                            ? Qt.rgba(1.0, 0.30, 0.25, 1)
                            : control.inkColor
                    font.family: "iA Writer Mono S"
                    font.pixelSize: control.scaledSize(13)
                    font.bold: true
                }
            }
        }
    }

    MouseArea {
        id: mouseArea
        anchors.fill: parent
        hoverEnabled: true
        onClicked: control.activated()
    }
}
