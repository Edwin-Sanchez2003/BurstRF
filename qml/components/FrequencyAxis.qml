import QtQuick

// Draws a horizontal frequency axis along the top of its parent.
// Inputs are plain numbers, so the component doesn't depend on the backend directly.
Item {
    id: axis

    property color tickColor: "#aeb7c4"
    property real centerHz: 0
    property real sampleRateSps: 0
    property int targetTicks: 8 // desired number of tick marks, not a hard count

    readonly property real spanHz: sampleRateSps
    readonly property real startHz: centerHz - spanHz / 2
    readonly property real endHz: centerHz + spanHz / 2

    // Pick a "nice" step (1, 2, or 5 times a power of ten) that gives roughly targetTicks ticks
    readonly property real stepHz: {
        if (spanHz <= 0)
            return 1
        const rough = spanHz / Math.max(1, targetTicks)
        const magnitude = Math.pow(10, Math.floor(Math.log(rough) / Math.LN10))
        const normalized = rough / magnitude
        let nice
        if (normalized < 1.5)
            nice = 1
        else if (normalized < 3.5)
            nice = 2
        else if (normalized < 7.5)
            nice = 5
        else
            nice = 10
        return nice * magnitude
    }

    // First tick is the smallest multiple of stepHz at or above startHz
    readonly property real firstTickHz: Math.ceil(startHz / stepHz) * stepHz

    // Number of ticks that fall inside the span
    readonly property int tickCount: stepHz > 0 ? Math.floor(
                                                      (endHz - firstTickHz) / stepHz) + 1 : 0

    // Converts a frequency to an x position in pixels within this item
    function xForHz(hz) {
        return (hz - startHz) / spanHz * width
    }

    // Formats a frequency for display, using MHz / kHz / Hz as appropriate
    function labelForHz(hz) {
        const abs = Math.abs(hz)
        if (abs >= 1e6)
            return (hz / 1e6).toFixed(3).replace(/\.?0+$/, "") + " MHz"
        if (abs >= 1e3)
            return (hz / 1e3).toFixed(3).replace(/\.?0+$/, "") + " kHz"
        return hz.toFixed(0) + " Hz"
    }

    // Tick marks and labels, one per tick
    Repeater {
        model: axis.tickCount

        Item {

            required property int index
            readonly property real tickHz: axis.firstTickHz + index * axis.stepHz
            x: axis.xForHz(tickHz)
            y: 0
            width: 1
            height: axis.height

            Rectangle {
                width: 1
                height: 6
                anchors.top: parent.top
                color: axis.tickColor
            }

            Text {
                anchors.top: parent.top
                anchors.topMargin: 8
                anchors.horizontalCenter: parent.horizontalCenter
                text: axis.labelForHz(parent.tickHz)
                color: axis.tickColor
                font.family: "Atkinson Hyperlegible"
                font.pointSize: 10
            }
        }
    }
}
