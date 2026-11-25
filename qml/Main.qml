import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs

import BurstRF
import BurstRF.Theme
import "components"


/*
  TODO: Make it so the spectrogram can handle zoom & pan, and make sure this doesn't effect the
  annotations adversely.
  TODO: break components into their own QML files:
    - Button & FileDialog (& error handling).
    - Spectrogram
*/
ApplicationWindow {
    id: root
    width: 1920
    height: 1080
    visible: true
    title: "BurstRF"

    color: Theme.bgWindow

    property int spectroWidth: root.spectroWidth
                               = backend.spectrogramChunkWidth // or whatever the backend exposes

    property real sampleRateSps: 0

    property int firstVisible: 0
    property int lastVisible: -1

    function updateVisibleRange() {
        const h = backend.spectrogramChunkHeight
        const count = backend.spectrogramChunkCount
        if (h <= 0 || count <= 0 || flick.height <= 0) {
            firstVisible = 0
            lastVisible = -1
            return
        }
        const buffer = flick.height / 2 // one viewport of buffer on each side
        firstVisible = Math.max(0, Math.floor((flick.contentY - buffer) / h))
        lastVisible = Math.min(
                    count - 1,
                    Math.floor((flick.contentY + flick.height + buffer) / h))
    }

    Connections {
        target: flick
        function onContentYChanged() {
            root.updateVisibleRange()
        }
        function onHeightChanged() {
            root.updateVisibleRange()
        }
    }

    // Dataset Selection Button - used to pick a .sigmf-meta file from a File Dialog.
    Button {
        id: pickFile
        text: "Select SigMF Metadata File"
        onClicked: fileDialog.open()

        contentItem: Text {
            text: pickFile.text
            color: Theme.textPrimary
            font: pickFile.font
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }

        background: Rectangle {
            radius: 4
            color: pickFile.down ? Theme.btnPressed : pickFile.hovered ? Theme.btnHover : Theme.btnBg
            border.color: pickFile.activeFocus ? Theme.accent : "transparent"
            border.width: 1
        }
    }

    // Displays the name of the currently loaded SigMF Recording.
    Text {
        id: fileDescriptorText
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.margins: 12

        text: "No File Selected"
        font.family: "Atkinson Hyperlegible"
        font.pointSize: 14
        font.weight: Font.Medium
        color: Theme.textMuted
        elide: Text.ElideMiddle // shortens long names in the middle rather than cutting them off
    }

    // Creates a file dialog to pick a file from the user's device.
    FileDialog {
        id: fileDialog
        title: "Select SigMF Metadata File"
        fileMode: FileDialog.OpenFile
        nameFilters: ["SigMF Meta files (*.sigmf-meta)", "All files (*)"]
        onAccepted: {
            if (backend.loadFile(selectedFile)) {
                console.log("Successfully loaded: " + selectedFile)
                fileDescriptorText.text = backend.currentFileDescriptor
            } else {
                console.log("Failed to load file!")
            }
        }
    }

    readonly property int spectroBorderWidth: 5

    Rectangle {
        id: spectroFrame
        width: flick.width + 2 * root.spectroBorderWidth
        height: flick.height + 2 * root.spectroBorderWidth
        anchors.centerIn: flick

        color: Theme.bgSpectro
        border.color: Theme.spectroBorder
        border.width: root.spectroBorderWidth
    }

    // A scrollable window with which the spectrogram is displayed.
    Flickable {
        id: flick
        width: Math.min(column.implicitWidth, parent.width)
        anchors.top: parent.top
        anchors.topMargin: 40 + root.spectroBorderWidth
        anchors.bottom: fileDescriptorText.top
        anchors.bottomMargin: 12 + root.spectroBorderWidth
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.horizontalCenterOffset: (timeAxis.width + 8) / 2

        contentWidth: column.implicitWidth
        contentHeight: column.implicitHeight
        clip: true

        Column {
            id: column
            width: flick.width

            Repeater {
                id: specChunkRepeater
                model: backend.spectrogramChunkCount
                // One delegate per chunk. It requests its own chunk when it
                // scrolls into view and only displays the chunk it asked for.
                Item {
                    id: chunk
                    readonly property int fullHeight: backend.spectrogramChunkHeight
                    // rows before this chunk = index * fullHeight
                    readonly property int rowsRemaining: backend.spectrogramRowCount
                                                         - index * fullHeight
                    implicitWidth: root.spectroWidth
                    implicitHeight: Math.min(fullHeight, rowsRemaining)
                    width: implicitWidth
                    height: implicitHeight
                    clip: true

                    property int chunkIndex: index
                    property bool requested: false
                    readonly property bool inView: index >= root.firstVisible
                                                   && index <= root.lastVisible

                    function syncRequest() {
                        if (inView && !requested) {
                            requested = true
                            backend.generateChunk(index)
                            console.log(">>> Requested chunkIndex: " + chunkIndex)
                        } else if (!inView && requested) {
                            requested = false
                            spectrogram.clearImage()
                            console.log("<<< Cleared chunkIndex: " + chunkIndex)
                        }
                    }

                    onInViewChanged: syncRequest()
                    Component.onCompleted: syncRequest()

                    Connections {
                        target: backend
                        function onChunkReady(chunkIndex, img) {
                            // only set visible image if the chunk is still requested (in view)
                            if (chunkIndex === chunk.chunkIndex
                                    && chunk.requested)
                                spectrogram.setImage(img)
                        }
                    }

                    SpectrogramItem {
                        id: spectrogram
                        anchors.horizontalCenter: parent.horizontalCenter
                        anchors.top: parent.top
                    }
                } // end item
            } // end repeater
        } // end column
    } // end flickable

    // X-Axis for the spectrogram
    FrequencyAxis {
        id: freqAxis
        width: spectroFrame.width
        height: 24
        anchors.bottom: spectroFrame.top
        anchors.bottomMargin: 6 // gap between the axis and the spectrogram frame
        anchors.horizontalCenter: spectroFrame.horizontalCenter

        centerHz: backend.getCenterFrequencyHz
        sampleRateSps: backend.getSampleRateSps
        targetTicks: 8
    }

    // Y-Axis for the spectrogram
    TimeAxis {
        id: timeAxis
        width: 64
        anchors.top: flick.top
        anchors.bottom: flick.bottom
        anchors.right: spectroFrame.left
        anchors.rightMargin: 8

        scrollOffsetPx: flick.contentY
        viewportPx: flick.height
        msPerRow: root.sampleRateSps > 0 ? 1000 * root.spectroWidth / root.sampleRateSps : 0
        targetTicks: 8
    }

    // vertical scrollbar as another option to interact with spectrogram
    ScrollBar {
        id: vbar
        orientation: Qt.Vertical
        policy: ScrollBar.AlwaysOn // keep it "active" so the style doesn't fade it
        visible: flick.visibleArea.heightRatio < 1 // hide only when there's nothing to scroll
        anchors.top: spectroFrame.top
        anchors.bottom: spectroFrame.bottom
        anchors.left: spectroFrame.right
        anchors.leftMargin: 4 // small gap between the frame and the bar

        // Reflect the flickable's current scroll position and visible fraction
        size: flick.visibleArea.heightRatio
        position: flick.visibleArea.yPosition

        // Only move the content while the user is dragging the bar.
        // Without this check, flick -> bar -> flick would loop.
        onPositionChanged: {
            if (pressed)
                flick.contentY = position * flick.contentHeight
        }

        // Track behind the thumb
        background: Rectangle {
            implicitWidth: 12
            radius: 6
            color: Theme.bgSpectro
        }

        // Thumb: always drawn at full opacity, brighter while hovered or dragged
        contentItem: Rectangle {
            implicitWidth: 12
            radius: 6
            color: vbar.pressed ? Theme.accent : vbar.hovered ? Theme.textMuted : Theme.btnHover
        }
    }

    Connections {
        target: backend
        function onRecordingLoaded() {
            specChunkRepeater.model = backend.spectrogramChunkCount
            flick.contentY = 0
            flick.returnToBounds()
            root.updateVisibleRange()
            root.sampleRateSps = backend.getSampleRateSps
            freqAxis.centerHz = backend.getCenterFrequencyHz
            freqAxis.sampleRateSps = root.sampleRateSps
        }
    }
}
