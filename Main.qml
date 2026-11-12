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
    width: 1920
    height: 1080
    visible: true
    title: "BurstRF"

    // Dataset Selection Button - used to pick a .sigmf-meta file from a File Dialog.
    Button {
        text: "Select SigMF Metadata File"
        onClicked: fileDialog.open()
    }

    FileDialog {
        id: fileDialog
        title: "Select SigMF Metadata File"
        fileMode: FileDialog.OpenFile
        nameFilters: ["SigMF Meta files (*.sigmf-meta)", "All files (*)"]
        onAccepted: {
            if (backend.loadFile(selectedFile)) {
                console.log("Successfully loaded: " + selectedFile)
            } else {
                console.log("Failed to load file!")
            }
        }
    }


    /*
      Things I need from the dataset:
      - spectrogram width
      - ~~spectrogram height~~
      - number of chunks
    */
    Flickable {
        id: flick
        width: parent.width / 2
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.horizontalCenter: parent.horizontalCenter

        contentWidth: width
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
                    width: column.width
                    height: backend.spectrogramChunkHeight()
                    clip: true

                    property int chunkIndex: index
                    property bool requested: false

                    // Keeps the chunk's image in sync with whether it overlaps the viewport.
                    readonly property real margin: flick.height // one viewport of buffer
                    function updateVisibility() {
                        const top = flick.contentY - margin
                        const bottom = flick.contentY + flick.height + margin
                        const inView = (y + height > top) && (y < bottom)

                        if (inView && !requested) {
                            requested = true
                            backend.generateChunk(chunkIndex)
                        } else if (!inView && requested) {
                            requested = false
                            spectrogram.clearImage()
                        }
                    }

                    onYChanged: updateVisibility()
                    Component.onCompleted: updateVisibility()

                    Connections {
                        target: flick
                        function onContentYChanged() {
                            chunk.updateVisibility()
                        }
                        function onHeightChanged() {
                            chunk.updateVisibility()
                        }
                    }

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
        }
    }
}
