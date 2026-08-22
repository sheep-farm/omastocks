import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Drawer {
    id: drawer

    property color pageColor: "#101010"
    property color inkColor: "#eeeeee"
    property color gainColor: Qt.rgba(0.30, 0.85, 0.40, 1)
    property color lossColor: Qt.rgba(1.0, 0.30, 0.25, 1)
    property real uiScale: 1.0
    property var stock: backend.selectedStock
    property var chartValues: backend.chartValues
    property string chartSymbol: backend.chartSymbol

    function scaledSize(pixels) {
        return Math.max(1, Math.round(pixels * uiScale));
    }

    function mixColors(base, tint, amount) {
        return Qt.rgba(
            base.r + (tint.r - base.r) * amount,
            base.g + (tint.g - base.g) * amount,
            base.b + (tint.b - base.b) * amount, 1);
    }

    width: parent.width
    height: parent.height
    edge: Qt.BottomEdge
    modal: true
    interactive: true

    background: Rectangle {
        color: drawer.pageColor
    }

    contentItem: Item {
        anchors.fill: parent
        anchors.margins: drawer.scaledSize(20)

        ColumnLayout {
            anchors.fill: parent
            spacing: drawer.scaledSize(16)

            RowLayout {
                Layout.fillWidth: true

                Text {
                    text: drawer.stock.symbol ? drawer.stock.symbol : ""
                    color: drawer.inkColor
                    font.family: "iA Writer Mono S"
                    font.pixelSize: drawer.scaledSize(28)
                    font.bold: true
                }

                Item { Layout.fillWidth: true }

                Button {
                    flat: true
                    onClicked: {
                        backend.removeSymbol(drawer.stock.symbol);
                        drawer.close();
                    }
                    contentItem: Text {
                        text: "−"
                        color: drawer.lossColor
                        font.family: "iA Writer Mono S"
                        font.pixelSize: drawer.scaledSize(28)
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    background: Rectangle { color: "transparent" }
                }

                Button {
                    flat: true
                    onClicked: drawer.close()
                    contentItem: Text {
                        text: "×"
                        color: drawer.inkColor
                        font.family: "iA Writer Mono S"
                        font.pixelSize: drawer.scaledSize(28)
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    background: Rectangle { color: "transparent" }
                }
            }

            Text {
                text: drawer.stock.name || drawer.stock.symbol || ""
                color: mixColors(drawer.pageColor, drawer.inkColor, 0.55)
                font.family: "iA Writer Mono S"
                font.pixelSize: drawer.scaledSize(16)
                Layout.fillWidth: true
                elide: Text.ElideRight
            }

            Text {
                text: drawer.stock.formattedPrice || "—"
                color: drawer.inkColor
                font.family: "iA Writer Mono S"
                font.pixelSize: drawer.scaledSize(56)
                font.bold: true
                Layout.fillWidth: true
            }

            Text {
                text: drawer.stock.formattedChangePercent || ""
                color: drawer.stock.isGaining ? drawer.gainColor
                        : drawer.stock.isLosing ? drawer.lossColor
                        : drawer.inkColor
                font.family: "iA Writer Mono S"
                font.pixelSize: drawer.scaledSize(22)
                font.bold: true
                Layout.fillWidth: true
            }

            Row {
                Layout.alignment: Qt.AlignHCenter
                spacing: drawer.scaledSize(8)

                Repeater {
                    model: ["1D", "5D", "1M", "6M", "YTD", "1Y", "5Y"]

                    Button {
                        required property var modelData
                        flat: true
                        width: drawer.scaledSize(38)
                        height: drawer.scaledSize(32)

                        contentItem: Text {
                            text: modelData
                            color: (backend.chartRange === modelData.toLowerCase()) ? drawer.inkColor : mixColors(drawer.pageColor, drawer.inkColor, 0.5)
                            font.family: "iA Writer Mono S"
                            font.pixelSize: drawer.scaledSize(13)
                            font.bold: true
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }

                        background: Rectangle {
                            color: (backend.chartRange === modelData.toLowerCase())
                                ? mixColors(drawer.pageColor, drawer.inkColor, 0.14)
                                : mixColors(drawer.pageColor, drawer.inkColor, 0.06)
                            border.width: 1
                            border.color: (backend.chartRange === modelData.toLowerCase())
                                ? mixColors(drawer.pageColor, drawer.inkColor, 0.25)
                                : mixColors(drawer.pageColor, drawer.inkColor, 0.13)
                        }

                        onClicked: backend.fetchChart(drawer.stock.symbol, modelData.toLowerCase())
                    }
                }
            }

            Item {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.minimumHeight: drawer.scaledSize(200)

                ChartCanvas {
                    id: chart
                    anchors.fill: parent
                    values: (drawer.chartSymbol === drawer.stock.symbol) ? drawer.chartValues : []
                    lineColor: drawer.stock.isGaining ? drawer.gainColor
                                : drawer.stock.isLosing ? drawer.lossColor
                                : drawer.inkColor
                }

                Text {
                    anchors.centerIn: parent
                    text: chart.values.length < 2 ? "No chart data" : ""
                    color: mixColors(drawer.pageColor, drawer.inkColor, 0.5)
                    font.family: "iA Writer Mono S"
                    font.pixelSize: drawer.scaledSize(14)
                    visible: text !== ""
                }
            }

            Item { Layout.fillHeight: true }
        }
    }
}
