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

    // Configuration — keep in sync with C++ backend
    // These represent pixel values on the QImages
    readonly property int chunkHeight: 300
    readonly property int totalHeight: 90000 // your "infinite" content height
    readonly property int contentW: 800

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
            // pass to C++ backend - FileDialog returns a QUrl datatype.
            console.log("Selected: " + selectedFile)


            /*
            if (sigMFBackend.loadFile(selectedFile) === false) {
                // TODO: Handle when the file fails to load (returns False)
                // Probably should throw a pop-up to the user!
                console.log("Failed to load file!")
            } else {
                console.log("Successfully loaded: " + selectedFile)
                console.log("Chunk count: " + sigMFBackend.chunkCount)
                mainScreen.datasetName = selectedFile.toString().split(
                            "/").pop()
            }
            */
        }
    }

    // Center the flickable + scrollbar group in the window
    Item {
        id: viewportGroup

        //width: Math.min(contentW + scrollBar.width, parent.width) // Clamp the group width to the window — don't overflow on small windows
        width: parent.width // Flickable + ScrollBar -> keep at a constant width, as to not affect the pixel aspect ratio.
        height: parent.height
        anchors.centerIn: parent

        // Centers Spectrogram in the view.
        Item {
            id: contentColumn
            width: contentW + scrollBar.width
            height: parent.height
            anchors.horizontalCenter: parent.horizontalCenter


            /*
              A Flickable manages a virtual canvas larger than its viewport.
              Internally, it tracks contentX & contentY to render the view - this
              is realized as a scroll offset, and applies a transform on the children
              of the flickable.
            */
            Flickable {
                id: flickable

                // Fill the group minus the scrollbar column
                anchors {
                    left: parent.left
                    right: scrollBar.left
                    top: parent.top
                    bottom: parent.bottom
                }

                // contentWidth & Height represent the spectrogram's width & height.
                // These must reflect the size of the dataset/spectrogram parameters.
                contentWidth: contentW
                contentHeight: totalHeight
                flickableDirection: Flickable.VerticalFlick // only scroll vertically, no horizontal scroll.
                clip: true // Forces only the visible items to be rendered by Qt - good for when we add annotation bboxes/text.
                interactive: true // Flickable handles touch/mouse drag gestures (ie. Spectrogram navigation).
                boundsBehavior: Flickable.StopAtBounds


                /*
                  Quantize contentY → chunk index to avoid per-frame requests contentY is changed at your CPU's
                  render speed (ex: 60fps) when a user scrolls on the screen. This means when a user interacts
                  with it, it is constantly changing. chunkIndex is a derived Binding - this means that it also
                  dynamically updates when contentY is updated. However, in most cases it stays constant until
                  "chunkHeight" units have passed.
                  Importantly, "chunkIndexChanged" only gets emitted when the chunkIndex value actually changes
                  (not every time it gets evaluated). This means things listening for chunkIndexChanged only get
                  notified when it actually gets changed (in this case, triggering generation of new Image Chunks).
                  This is called "debouncing".
                  NOTE: properties are live relationships - they are a constraint the engine maintains. This is built
                  with signal/slots, which is made to be an automatic observer pattern in QML for properties.
                */
                property int chunkIndex: {
                    let maxContentY = totalHeight - height // 90000 - 1130 = 88870
                    let clampedY = Math.max(0, Math.min(contentY, maxContentY))
                    return Math.floor(clampedY / chunkHeight)
                }

                readonly property int chunksVisible: Math.ceil(
                                                         height / chunkHeight) // on Flickable
                readonly property int poolSize: chunksVisible + 3 // 1 above + visible + 2 below

                // Repeater: A virtual scrolling pool. The actual number of elements at any time is determined by
                // the model number.
                Repeater {
                    model: flickable.poolSize
                    delegate: Item {
                        required property int index

                        // No slot, no slotY, no inBounds properties.
                        // Everything computed fresh from the same two inputs.
                        readonly property int _slot: flickable.chunkIndex - 1 + index
                        readonly property int _slotY: (_slot >= 0
                                                       && (_slot * chunkHeight + chunkHeight) <= totalHeight) ? _slot * chunkHeight : -chunkHeight

                        x: 0
                        y: _slot * chunkHeight // position always tracks slot directly
                        width: contentW
                        height: chunkHeight

                        readonly property string chunkSource: {
                            let s = flickable.chunkIndex - 1 + index
                            let sy = s * chunkHeight
                            if (s < 0 || sy < 0
                                    || (sy + chunkHeight) > totalHeight)
                                return ""
                            return "image://chunks/chunk?y=" + sy + "&t=" + s
                        }

                        Image {
                            id: backBuffer
                            anchors.fill: parent
                            fillMode: Image.PreserveAspectCrop
                            cache: false
                            visible: frontBuffer.status !== Image.Ready
                        }

                        Image {
                            id: frontBuffer
                            anchors.fill: parent
                            fillMode: Image.PreserveAspectCrop
                            cache: false
                            source: chunkSource
                            opacity: 0.0

                            onStatusChanged: {
                                if (status === Image.Ready) {
                                    backBuffer.source = source
                                    opacity = 1.0
                                }
                            }

                            onSourceChanged: {
                                opacity = 0.0
                                if (source === "") {
                                    backBuffer.source = ""
                                }
                            }

                            Behavior on opacity {
                                NumberAnimation {
                                    duration: 80
                                }
                            }
                        }

                        Rectangle {
                            anchors.fill: parent
                            visible: chunkSource !== ""
                                     && frontBuffer.status !== Image.Ready
                                     && backBuffer.status !== Image.Ready
                            color: frontBuffer.status === Image.Error ? "#3a1a1a" : "#2a2a2a"
                            Text {
                                anchors.centerIn: parent
                                text: frontBuffer.status
                                      === Image.Error ? "Error loading chunk" : "Loading..."
                                color: frontBuffer.status === Image.Error ? "#aa4444" : "#555555"
                                font.pixelSize: 12
                            }
                        }
                    } // end delegate Item
                } // End Repeater
            } // End Flickable

            ScrollBar {
                id: scrollBar

                anchors {
                    right: parent.right
                    top: parent.top
                    bottom: parent.bottom
                }

                orientation: Qt.Vertical
                policy: ScrollBar.AlwaysOn
                // sets each unit of movement on the scrollbar equal to exactly one viewport movement on the screen.
                // this means when you use it, you move exactly one flickable height's worth up or down for every unit moved.
                // this makes sure that the scroll bar reflects the size of the data being presented.
                size: flickable.height / flickable.contentHeight

                // makes sure that the scrollbar handle reflects the current position of the content in the flickable (scrollbar
                // updated when scrolling via other methods - drag gesture, scroll wheel gesture.
                position: flickable.contentY / flickable.contentHeight

                // Feed scroll bar drags back into the flickable - flickable is updated when scrollbar is used.
                // if (pressed) guards agains infinite loop, where scroll bar feeds flickable feeds scrollbar.
                // only user-initiated drag events write to the contentY value.
                onPositionChanged: {
                    if (pressed)
                        flickable.contentY = position * flickable.contentHeight
                }

                // Track (the background groove)
                background: Rectangle {
                    color: "#2a2a2a"
                    radius: 4
                }

                // Handle (the draggable thumb)
                contentItem: Rectangle {
                    implicitWidth: 12
                    radius: 6
                    color: scrollBar.pressed ? "#ffffff" : scrollBar.hovered ? "#aaaaaa" : "#666666"

                    Behavior on color {
                        ColorAnimation {
                            duration: 100
                        }
                    }
                }
            } // End ScrollBar
        }
    }

    // Debug overlay
    // NOTE make sure to remove this in production, this is a costly updated - rendering text modifications at 60fps.
    Text {
        anchors {
            right: parent.right
            top: parent.top
            margins: 8
        }
        text: "y: " + Math.round(
                  flickable.contentY) + "  chunk: " + flickable.chunkIndex
              + "  win: " + Window.width + "×" + Window.height
        color: "#aaaaaa"
        font.pixelSize: 11
        z: 10 // render above everything else

        Rectangle {
            anchors.fill: parent
            color: "#aa000000"
            z: -1
            radius: 3
        }
    }
}
