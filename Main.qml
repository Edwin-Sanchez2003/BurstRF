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
            if (!backend.loadFile(selectedFile)) {
                console.log("Failed to load file!")
            } else {
                console.log("Successfully loaded: " + selectedFile)
            }
        }
    }

    SpectrogramItem {
        id: spectrogram
        anchors.fill: parent
    }

    Connections {
        target: backend
        function onChunkReady(img) {
            spectrogram.setImage(img)
        }
    }
}
