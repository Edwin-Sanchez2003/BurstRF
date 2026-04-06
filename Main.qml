import QtQuick
import QtQuick.Controls

ApplicationWindow {
    width: 1920
    height: 1080
    visible: true
    title: "BurstRF"

    // Configuration — keep in sync with C++ backend
    readonly property int chunkHeight: 300
    readonly property int totalHeight: 90000 // your "infinite" content height
    readonly property int contentW: 800

    // Center the flickable + scrollbar group in the window
    Item {
        id: viewportGroup

        // Clamp the group width to the window — don't overflow on small windows
        width: Math.min(contentW + scrollBar.width, parent.width)
        height: parent.height
        anchors.centerIn: parent

        Flickable {
            id: flickable

            // Fill the group minus the scrollbar column
            anchors {
                left: parent.left
                right: scrollBar.left
                top: parent.top
                bottom: parent.bottom
            }

            contentWidth: contentW
            contentHeight: totalHeight
            flickableDirection: Flickable.VerticalFlick
            clip: true
            interactive: true

            // Disable Flickable's own wheel handling
            WheelHandler {
                acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
                blocking: true // stops the event reaching Flickable's handler
                grabPermissions: PointerHandler.CanTakeOverFromAnything

                onWheel: event => {
                             flickable.contentY = Math.max(
                                 0, Math.min(
                                     flickable.contentY - event.angleDelta.y * 2.0,
                                     flickable.contentHeight - flickable.height))
                         }
            }

            // Quantize contentY → chunk index to avoid per-frame requests
            property int chunkIndex: Math.floor(contentY / chunkHeight)

            Repeater {
                model: 6
                delegate: Image {
                    required property int index

                    property int slot: flickable.chunkIndex - 1 + index
                    property int slotY: slot * chunkHeight

                    x: 0
                    y: slotY
                    // Stretch chunk to fill the flickable's visible width
                    width: flickable.width
                    height: chunkHeight

                    fillMode: Image.Stretch
                    cache: false

                    source: slot >= 0 ? "image://chunks/chunk?y=" + slotY + "&t=" + slot : ""

                    opacity: status === Image.Ready ? 1.0 : 0.0
                    Behavior on opacity {
                        NumberAnimation {
                            duration: 80
                        }
                    }

                    // Subtle loading placeholder
                    Rectangle {
                        anchors.fill: parent
                        color: "#2a2a2a"
                        visible: parent.status !== Image.Ready

                        Text {
                            anchors.centerIn: parent
                            text: "Loading..."
                            color: "#555555"
                            font.pixelSize: 12
                        }
                    }
                }
            }
        }

        ScrollBar {
            id: scrollBar

            anchors {
                right: parent.right
                top: parent.top
                bottom: parent.bottom
            }

            orientation: Qt.Vertical
            policy: ScrollBar.AlwaysOn
            size: flickable.height / flickable.contentHeight
            position: flickable.contentY / flickable.contentHeight

            // Feed scroll bar drags back into the flickable
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
        }
    }

    // Debug overlay
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
        z: 10

        Rectangle {
            anchors.fill: parent
            color: "#aa000000"
            z: -1
            radius: 3
        }
    }
}
