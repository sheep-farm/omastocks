import QtQuick

Canvas {
    id: canvas

    property var values: []
    property color lineColor: "#34c759"
    property color fillColor: Qt.rgba(lineColor.r, lineColor.g, lineColor.b, 0.15)
    property color gridColor: Qt.rgba(lineColor.r, lineColor.g, lineColor.b, 0.15)
    property real lineWidth: 2

    function draw() {
        requestPaint();
    }

    onValuesChanged: draw()
    onWidthChanged: draw()
    onHeightChanged: draw()

    onPaint: {
        var context = getContext("2d");
        context.clearRect(0, 0, width, height);

        if (values.length < 2)
            return;

        var validValues = [];
        for (var i = 0; i < values.length; ++i) {
            if (typeof values[i] === "number" && isFinite(values[i]))
                validValues.push(values[i]);
        }

        if (validValues.length < 2)
            return;

        var min = validValues[0];
        var max = validValues[0];
        for (var j = 1; j < validValues.length; ++j) {
            if (validValues[j] < min) min = validValues[j];
            if (validValues[j] > max) max = validValues[j];
        }

        var padding = height * 0.15;
        var chartHeight = height - 2 * padding;
        var chartWidth = width;
        var range = max - min;
        if (range === 0)
            range = 1;

        function xFor(i) {
            return (validValues.length === 1)
                ? 0
                : (i / (validValues.length - 1)) * chartWidth;
        }

        function yFor(v) {
            return padding + (1 - (v - min) / range) * chartHeight;
        }

        context.beginPath();
        context.moveTo(xFor(0), yFor(validValues[0]));
        for (var k = 1; k < validValues.length; ++k)
            context.lineTo(xFor(k), yFor(validValues[k]));

        context.lineTo(xFor(validValues.length - 1), height);
        context.lineTo(xFor(0), height);
        context.closePath();
        context.fillStyle = fillColor;
        context.fill();

        context.beginPath();
        context.moveTo(xFor(0), yFor(validValues[0]));
        for (var l = 1; l < validValues.length; ++l)
            context.lineTo(xFor(l), yFor(validValues[l]));
        context.strokeStyle = lineColor;
        context.lineWidth = lineWidth;
        context.lineCap = "round";
        context.lineJoin = "round";
        context.stroke();

        context.beginPath();
        context.arc(xFor(validValues.length - 1), yFor(validValues[validValues.length - 1]), 3, 0, 2 * Math.PI);
        context.fillStyle = lineColor;
        context.fill();
    }
}
