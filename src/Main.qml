import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import QtQuick.Window

ApplicationWindow {
    id: win

    width: 480
    height: 720
    minimumWidth: 420
    minimumHeight: 620
    visible: true
    title: "Omastocks"

    readonly property bool darkMode: backend.darkMode
    readonly property color pageColor: backend.themeBackground
    readonly property color inkColor: backend.themeForeground
    readonly property color accentColor: backend.themeAccent
    readonly property color gainColor: darkMode ? Qt.rgba(0.30, 0.85, 0.40, 1) : Qt.rgba(0.13, 0.55, 0.13, 1)
    readonly property color lossColor: darkMode ? Qt.rgba(1.0, 0.30, 0.25, 1) : Qt.rgba(0.75, 0.15, 0.10, 1)

    readonly property real uiScale: Math.min(Math.min(width / 480, height / 720), 1.35)
    property real appliedTextScale: backend.textScale

    function scaledSize(pixels) {
        return Math.max(1, Math.round(pixels * uiScale));
    }

    function mixColors(base, tint, amount) {
        return Qt.rgba(
            base.r + (tint.r - base.r) * amount,
            base.g + (tint.g - base.g) * amount,
            base.b + (tint.b - base.b) * amount, 1);
    }

    Connections {
        target: backend

        function onTextScaleChanged() {
            var factor = backend.textScale / win.appliedTextScale;
            win.appliedTextScale = backend.textScale;
            if (win.visibility === Window.Windowed) {
                win.width = Math.round(win.width * factor);
                win.height = Math.round(win.height * factor);
                win.x = Math.round((Screen.width - win.width) / 2);
                win.y = Math.round((Screen.height - win.height) / 2);
            }
        }
    }

    Material.theme: darkMode ? Material.Dark : Material.Light
    Material.accent: accentColor
    color: pageColor

    Shortcut {
        sequences: ["Ctrl+Q"]
        context: Qt.ApplicationShortcut
        onActivated: win.close()
    }

    Item {
        id: face
        anchors.fill: parent
        anchors.margins: win.scaledSize(20)

        ColumnLayout {
            anchors.fill: parent
            spacing: win.scaledSize(16)

            RowLayout {
                Layout.fillWidth: true

                Text {
                    text: "Watchlist"
                    color: win.inkColor
                    font.family: "iA Writer Mono S"
                    font.pixelSize: win.scaledSize(28)
                    font.bold: true
                }

                Item { Layout.fillWidth: true }

                BusyIndicator {
                    running: backend.busy
                    visible: running
                    Layout.preferredWidth: win.scaledSize(22)
                    Layout.preferredHeight: win.scaledSize(22)
                }

                Button {
                    flat: true
                    enabled: !backend.busy
                    onClicked: backend.refresh()
                    Layout.preferredWidth: win.scaledSize(32)
                    Layout.preferredHeight: win.scaledSize(32)

                    contentItem: Text {
                        text: "↻"
                        color: backend.busy ? mixColors(win.pageColor, win.inkColor, 0.35) : win.inkColor
                        font.family: "iA Writer Mono S"
                        font.pixelSize: win.scaledSize(20)
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }

                    background: Rectangle {
                        color: "transparent"
                    }
                }
            }

            Item {
                Layout.fillWidth: true
                Layout.preferredHeight: win.scaledSize(40)
                height: win.scaledSize(40)

                Row {
                    id: searchRow
                    anchors.fill: parent
                    spacing: win.scaledSize(10)

                    TextField {
                        id: symbolInput
                        width: parent.width - sortButton.width - parent.spacing
                        height: parent.height
                        placeholderText: "Add tickers (e.g. AAPL, GOOGL), press Enter"
                        color: win.inkColor
                        placeholderTextColor: mixColors(win.pageColor, win.inkColor, 0.45)
                        font.family: "iA Writer Mono S"
                        font.pixelSize: win.scaledSize(15)

                        background: Rectangle {
                            color: mixColors(win.pageColor, win.inkColor, 0.06)
                            border.width: 1
                            border.color: mixColors(win.pageColor, win.inkColor, 0.13)
                        }

                        Keys.onReturnPressed: {
                            if (text.length > 0) {
                                backend.addSymbol(text);
                                text = "";
                                focus = true;
                            }
                        }
                        Keys.onEnterPressed: {
                            if (text.length > 0) {
                                backend.addSymbol(text);
                                text = "";
                                focus = true;
                            }
                        }
                    }

                    Button {
                        id: sortButton
                        width: win.scaledSize(70)
                        height: parent.height

                        contentItem: Text {
                            text: backend.sortModeLabel(backend.sortMode)
                            color: win.inkColor
                            font.family: "iA Writer Mono S"
                            font.pixelSize: win.scaledSize(13)
                            font.bold: true
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }

                        background: Rectangle {
                            color: mixColors(win.pageColor, win.inkColor, 0.08)
                            border.width: 1
                            border.color: mixColors(win.pageColor, win.inkColor, 0.16)
                        }

                        onClicked: {
                            var next = (backend.sortMode + 1) % backend.sortModeCount();
                            backend.sort(next);
                        }
                    }
                }
            }

            Text {
                Layout.fillWidth: true
                text: backend.error
                color: win.lossColor
                font.family: "iA Writer Mono S"
                font.pixelSize: win.scaledSize(13)
                wrapMode: Text.WordWrap
                visible: backend.error.length > 0
            }

            ListView {
                id: listView
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                spacing: win.scaledSize(10)
                model: backend.stocks

                ScrollBar.vertical: ScrollBar {
                    policy: ScrollBar.AsNeeded
                }

                delegate: StockCard {
                    width: listView.width
                    uiScale: win.uiScale
                    symbol: model.symbol
                    name: model.name
                    formattedPrice: model.formattedPrice
                    formattedChangePercent: model.formattedChangePercent
                    isGaining: model.isGaining
                    isLosing: model.isLosing
                    pageColor: win.pageColor
                    inkColor: win.inkColor
                    gainColor: win.gainColor
                    lossColor: win.lossColor

                    onActivated: {
                        backend.selectStock(index);
                        detailDrawer.open();
                    }
                }

                Text {
                    anchors.centerIn: parent
                    text: "No stocks in watchlist"
                    color: mixColors(win.pageColor, win.inkColor, 0.45)
                    font.family: "iA Writer Mono S"
                    font.pixelSize: win.scaledSize(15)
                    visible: listView.count === 0
                }
            }

            Text {
                id: lastUpdatedLabel
                Layout.fillWidth: true
                text: backend.lastUpdated.length > 0
                    ? "Updated " + backend.lastUpdated
                    : ""
                color: mixColors(win.pageColor, win.inkColor, 0.45)
                font.family: "iA Writer Mono S"
                font.pixelSize: win.scaledSize(12)
                horizontalAlignment: Text.AlignHCenter
                visible: backend.lastUpdated.length > 0
            }
        }
    }

    StockDetail {
        id: detailDrawer
        pageColor: win.pageColor
        inkColor: win.inkColor
        gainColor: win.gainColor
        lossColor: win.lossColor
        uiScale: win.uiScale
    }

    property rect normalGeometry: Qt.rect(x, y, width, height)
    property bool wasMaximized: false

    function trackNormalGeometry() {
        if (visibility === Window.Windowed)
            normalGeometry = Qt.rect(x, y, width, height);
    }

    onXChanged: trackNormalGeometry()
    onYChanged: trackNormalGeometry()
    onWidthChanged: trackNormalGeometry()
    onHeightChanged: trackNormalGeometry()

    onVisibilityChanged: function(visibility) {
        if (visibility === Window.Maximized || visibility === Window.FullScreen)
            wasMaximized = true;
        else if (visibility === Window.Windowed)
            wasMaximized = false;
    }

    Component.onCompleted: {
        var geometry = backend.windowGeometry();
        if (geometry.valid) {
            x = geometry.x;
            y = geometry.y;
            width = geometry.width;
            height = geometry.height;
            if (geometry.maximized) showMaximized();
        } else {
            width = Math.round(480 * backend.textScale);
            height = Math.round(720 * backend.textScale);
        }
    }

    Component.onDestruction: backend.saveWindowGeometry(
        normalGeometry.x, normalGeometry.y,
        normalGeometry.width, normalGeometry.height, wasMaximized)
}
