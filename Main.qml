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
            if (!burstRFBackend.loadFile(selectedFile)) {
                console.log("Failed to load file!")
            } else {
                console.log("Successfully loaded: " + selectedFile)
            }
        }
    }

    Image {
        id: test_spectrogram
        // TODO: Implement Image Provider... Qt6-ism...
        source: "trimmedSamples(2)_auto_0__1_000_000.png"
        anchors.fill: parent
        fillMode: Image.Pad
        cache: false
        visible: true
    }
}
