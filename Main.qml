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

                    // Request the chunk once, the first time it overlaps the viewport.
                    function requestIfVisible() {
                        if (requested)
                            return
                        if (y + height > flick.contentY
                                && y < flick.contentY + flick.height) {
                            requested = true
                            backend.generateChunk(chunkIndex)
                        }
                    }

                    onYChanged: requestIfVisible()
                    Component.onCompleted: requestIfVisible()

                    Connections {
                        target: flick
                        function onContentYChanged() {
                            chunk.requestIfVisible()
                        }
                        function onHeightChanged() {
                            chunk.requestIfVisible()
                        }
                    }

                    Connections {
                        target: backend
                        function onChunkReady(chunkIndex, img) {
                            if (chunkIndex === chunk.chunkIndex)
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
