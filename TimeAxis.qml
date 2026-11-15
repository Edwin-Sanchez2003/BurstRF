import QtQuick

// Draws a vertical time axis along the left side of its parent.
// Labels reflect the currently visible part of the capture, so they update as the user scrolls.
Item {
    id: axis

    property real scrollOffsetPx: 0 // how far the flickable is scrolled, in pixels
    property real viewportPx: 0 // height of the visible area, in pixels
    property real msPerRow: 0 // milliseconds covered by one spectrogram row (one pixel)
    property int targetTicks: 8
    property color tickColor: "#aeb7c4"

    // Time at the top and bottom of the visible area, measured from the start of the capture
    readonly property real startMs: scrollOffsetPx * msPerRow
    readonly property real endMs: (scrollOffsetPx + viewportPx) * msPerRow
    readonly property real spanMs: endMs - startMs

    // Pick a "nice" step (1, 2, 5, or 10 times a power of ten) that gives roughly targetTicks ticks
    readonly property real stepMs: {
        if (spanMs <= 0)
            return 1
        const rough = spanMs / Math.max(1, targetTicks)
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

    readonly property real firstTickMs: Math.ceil(startMs / stepMs) * stepMs

    readonly property int tickCount: spanMs > 0 ? Math.floor(
                                                      (endMs - firstTickMs) / stepMs) + 1 : 0

    // Converts a time to a y position in pixels within this item
    function yForMs(ms) {
        return spanMs > 0 ? (ms - startMs) / spanMs * height : 0
    }

    // Formats a time for display, using seconds once the value passes 1000 ms
    function labelForMs(ms) {
        if (Math.abs(ms) >= 1000)
            return (ms / 1000).toFixed(2).replace(/\.?0+$/, "") + " s"
        return ms.toFixed(0) + " ms"
    }

    Repeater {
        model: axis.tickCount

        Item {
            required property int index
            readonly property real tickMs: axis.firstTickMs + index * axis.stepMs
            y: axis.yForMs(tickMs)
            width: axis.width
            height: 1

            // Tick mark on the right edge, pointing toward the spectrogram
            Rectangle {
                anchors.right: parent.right
                width: 6
                height: 1
                color: axis.tickColor
            }

            Text {
                anchors.right: parent.right
                anchors.rightMargin: 10
                anchors.verticalCenter: parent.verticalCenter
                text: axis.labelForMs(tickMs)
                color: axis.tickColor
                font.family: "Atkinson Hyperlegible"
                font.pointSize: 10
            }
        }
    }
}
