import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs


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

    readonly property color bgWindow: "#1e2127"
    readonly property color bgSpectro: "#111316"
    readonly property color btnBg: "#2d333b"
    readonly property color btnHover: "#3a4250"
    readonly property color btnPressed: "#252a31"
    readonly property color textPrimary: "#e6e9ef"
    readonly property color textMuted: "#aeb7c4"
    readonly property color accent: "#4c9ef0"

    color: bgWindow

    property int spectroWidth: root.spectroWidth = backend.spectrogramWidth(
                                   ) // or whatever the backend exposes

    property int firstVisible: 0
    property int lastVisible: -1

    function updateVisibleRange() {
        const h = backend.spectrogramChunkHeight()
        const count = backend.spectrogramChunkCount()
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
            color: root.textPrimary
            font: pickFile.font
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }

        background: Rectangle {
            radius: 4
            color: pickFile.down ? root.btnPressed : pickFile.hovered ? root.btnHover : root.btnBg
            border.color: pickFile.activeFocus ? root.accent : "transparent"
            border.width: 1
        }
    }

    // Displays the name of the currently loaded SigMF Recording.
    Text {
        id: fileNameText
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.margins: 12

        text: "No File Selected"
        font.family: "Atkinson Hyperlegible"
        font.pointSize: 14
        font.weight: Font.Medium
        color: root.textMuted
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
                fileNameText.text = backend.currentFileName()
            } else {
                console.log("Failed to load file!")
            }
        }
    }

    readonly property int spectroBorderWidth: 5
    readonly property color spectroBorder: "#8a7560"

    Rectangle {
        id: spectroFrame
        width: flick.width + 2 * root.spectroBorderWidth
        height: flick.height + 2 * root.spectroBorderWidth
        anchors.centerIn: flick

        color: root.bgSpectro
        border.color: root.spectroBorder
        border.width: root.spectroBorderWidth
    }

    // A scrollable window with which the spectrogram is displayed.
    Flickable {
        id: flick
        width: Math.min(column.implicitWidth, parent.width)
        anchors.top: parent.top
        anchors.topMargin: 12 + root.spectroBorderWidth
        anchors.bottom: fileNameText.top
        anchors.bottomMargin: 12 + root.spectroBorderWidth
        anchors.horizontalCenter: parent.horizontalCenter

        contentWidth: column.implicitWidth
        contentHeight: column.implicitHeight
        clip: true

        Column {
            id: column
            width: flick.width

            Repeater {
                id: specChunkRepeater
                model: backend.spectrogramChunkCount()
                // One delegate per chunk. It requests its own chunk when it
                // scrolls into view and only displays the chunk it asked for.
                Item {
                    id: chunk
                    implicitWidth: root.spectroWidth
                    implicitHeight: backend.spectrogramChunkHeight()
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
                }
            }
        }
    }

    Connections {
        target: backend
        function onRecordingLoaded() {
            specChunkRepeater.model = backend.spectrogramChunkCount()
            flick.contentY = 0
            flick.returnToBounds()
            root.updateVisibleRange()
        }
    }
}
