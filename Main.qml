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
                property int chunkIndex: contentY > 0 ? Math.floor(
                                                            contentY / chunkHeight) : 0

                // Repeater: A virtual scrolling pool. The actual number of elements at any time is determined by
                // the model number.
                Repeater {
                    model: 6
                    delegate: Image {
                        required property int index

                        // -1 puts the first ImageChunk above the visible screen, model: 6 is just enough
                        // to put 1/2 extra below the visible screen.
                        // TODO: group model #, chunkIndex, and chunkHeight to keep a consistent number of chunks
                        // rendered no matter what!
                        property int slot: flickable.chunkIndex - 1 + index
                        property int slotY: slot * chunkHeight
                        property bool inBounds: slot >= 0
                                                && (slotY + chunkHeight) <= totalHeight

                        x: 0
                        y: slotY
                        // fixed size - not updated with scaling. also maps to ChunkImageProvider, so must match what backend is told to render.
                        width: contentW
                        height: chunkHeight

                        fillMode: Image.PreserveAspectCrop
                        cache: false // forces Repeater to not cache any Images.


                        /*
                          Qt's custom image provider URL scheme:
                          1. Looks up the registered QuickImageProvider named "chunks" in the QML engine.
                          2. Calls chunk.requestImage() on a background thread (sub-classed from QQuickAsyncImageProvider).
                          3. When the result is ready, the result is put back in the main thread and updates the image item.
                          NOTE: &t= is a cache-buster - as the URL is always unique, this forces the backend to re-render
                          the image chunk.
                        */
                        source: (slot >= 0
                                 && slotY < totalHeight) ? "image://chunks/chunk?y=" + slotY
                                                           + "&t=" + slot : ""

                        // 80ms fade-in for chunks when they're ready.
                        property bool suppressFade: false

                        onSlotChanged: {
                            suppressFade = true
                        }

                        opacity: status === Image.Ready ? 1.0 : 0.0

                        Behavior on opacity {
                            enabled: !suppressFade
                            NumberAnimation {
                                duration: 80
                            }
                        }

                        // Subtle loading placeholder, when user scrolls really fast...
                        Rectangle {
                            anchors.fill: parent
                            visible: parent.inBounds
                                     && parent.status !== Image.Ready
                            color: parent.status === Image.Error ? "#3a1a1a" : "#2a2a2a"

                            Text {
                                anchors.centerIn: parent
                                text: parent.status
                                      === Image.Error ? "Error loading chunk" : "Loading..."
                                color: parent.status === Image.Error ? "#aa4444" : "#555555"
                                font.pixelSize: 12
                            }
                        }
                    } // End Image
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
