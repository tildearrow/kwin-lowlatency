/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

import QtQuick 2.12
import QtGraphicalEffects 1.12
import org.kde.kirigami 2.12 as Kirigami
import org.kde.kwin 3.0 as KWinComponents
import org.kde.kwin.private.overview 1.0
import org.kde.plasma.components 3.0 as PC3
import org.kde.plasma.core 2.0 as PlasmaCore

FocusScope {
    id: heap

    enum Direction {
        Left,
        Right,
        Up,
        Down
    }

    property alias model: windowsRepeater.model
    property int selectedIndex: -1
    property bool animationEnabled: false
    property real padding: 0
    property string filter: ""

    required property bool organized
    readonly property bool effectiveOrganized: expoLayout.ready && organized

    ExpoLayout {
        id: expoLayout
        x: heap.padding
        y: heap.padding
        width: parent.width - 2 * heap.padding
        height: parent.height - 2 * heap.padding
        mode: effect.layout
        spacing: PlasmaCore.Units.largeSpacing

        Repeater {
            id: windowsRepeater

            Item {
                id: thumb

                required property QtObject client
                required property int index

                readonly property bool selected: heap.selectedIndex == index
                readonly property bool hidden: {
                    if (heap.filter) {
                        return !client.caption.toLowerCase().includes(heap.filter.toLowerCase());
                    }
                    return false;
                }

                state: {
                    if (heap.effectiveOrganized) {
                        return hidden ? "active-hidden" : "active";
                    }
                    return client.minimized ? "initial-minimized" : "initial";
                }

                visible: opacity > 0
                z: client.stackingOrder

                KWinComponents.WindowThumbnailItem {
                    id: thumbSource
                    anchors.fill: parent
                    wId: thumb.client.internalId
                }

                PlasmaCore.IconItem {
                    id: icon
                    width: PlasmaCore.Units.iconSizes.large
                    height: width
                    source: thumb.client.icon
                    anchors.horizontalCenter: thumbSource.horizontalCenter
                    anchors.bottom: thumbSource.bottom
                    anchors.bottomMargin: -height / 4
                }

                PC3.Label {
                    id: caption
                    width: Math.min(implicitWidth, thumbSource.width)
                    anchors.top: icon.bottom
                    anchors.horizontalCenter: icon.horizontalCenter
                    elide: Text.ElideRight
                    text: thumb.client.caption
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    PlasmaCore.ColorScope.colorGroup: PlasmaCore.Theme.ComplementaryColorGroup

                    layer.enabled: true
                    layer.effect: DropShadow {
                        cached: true
                        horizontalOffset: 1
                        verticalOffset: 1
                        color: "black"
                        radius: Math.round(4 * PlasmaCore.Units.devicePixelRatio)
                        samples: radius * 2 + 1
                        spread: 0.35
                    }
                }

                ExpoCell {
                    id: cell
                    layout: expoLayout
                    naturalX: thumb.client.x - targetScreen.geometry.x - expoLayout.Kirigami.ScenePosition.x
                    naturalY: thumb.client.y - targetScreen.geometry.y - expoLayout.Kirigami.ScenePosition.y
                    naturalWidth: thumb.client.width
                    naturalHeight: thumb.client.height
                    persistentKey: thumb.client.internalId
                    bottomMargin: icon.height / 4 + caption.height
                }

                states: [
                    State {
                        name: "initial"
                        PropertyChanges {
                            target: thumb
                            x: thumb.client.x - targetScreen.geometry.x - expoLayout.Kirigami.ScenePosition.x
                            y: thumb.client.y - targetScreen.geometry.y - expoLayout.Kirigami.ScenePosition.y
                            width: thumb.client.width
                            height: thumb.client.height
                        }
                    },
                    State {
                        name: "initial-minimized"
                        extend: "initial"
                        PropertyChanges {
                            target: thumb
                            opacity: 0
                        }
                    },
                    State {
                        name: "active"
                        PropertyChanges {
                            target: thumb
                            x: cell.x
                            y: cell.y
                            width: cell.width
                            height: cell.height
                        }
                    },
                    State {
                        name: "active-hidden"
                        extend: "active"
                        PropertyChanges {
                            target: thumb
                            opacity: 0
                        }
                    }
                ]

                component TweenBehavior : Behavior {
                    enabled: heap.animationEnabled
                    NumberAnimation {
                        duration: effect.animationDuration
                        easing.type: Easing.InOutCubic
                    }
                }

                TweenBehavior on x {}
                TweenBehavior on y {}
                TweenBehavior on width {}
                TweenBehavior on height {}
                TweenBehavior on opacity {}

                PlasmaCore.FrameSvgItem {
                    anchors.fill: parent
                    anchors.margins: -PlasmaCore.Units.smallSpacing
                    imagePath: "widgets/viewitem"
                    prefix: "hover"
                    z: -1
                    visible: hoverHandler.hovered || selected
                }

                HoverHandler {
                    id: hoverHandler
                    onHoveredChanged: if (hovered != selected) {
                        heap.resetSelected();
                    }
                }

                TapHandler {
                    acceptedButtons: Qt.LeftButton
                    onTapped: {
                        KWinComponents.Workspace.activeClient = thumb.client;
                        effect.deactivate();
                    }
                }

                TapHandler {
                    acceptedButtons: Qt.MiddleButton
                    onTapped: thumb.client.closeWindow()
                }

                PC3.Button {
                    LayoutMirroring.enabled: Qt.application.layoutDirection == Qt.RightToLeft
                    icon.name: "window-close"
                    anchors.right: thumbSource.right
                    anchors.rightMargin: PlasmaCore.Units.largeSpacing
                    anchors.top: thumbSource.top
                    anchors.topMargin: PlasmaCore.Units.largeSpacing
                    implicitWidth: PlasmaCore.Units.iconSizes.medium
                    implicitHeight: implicitWidth
                    visible: (hovered || hoverHandler.hovered) && thumb.client.closeable
                    onClicked: thumb.client.closeWindow();
                }

                Component.onDestruction: {
                    if (selected) {
                        heap.resetSelected();
                    }
                }
            }
        }
    }

    function findFirstItem() {
        for (let candidateIndex = 0; candidateIndex < windowsRepeater.count; ++candidateIndex) {
            const candidateItem = windowsRepeater.itemAt(candidateIndex);
            if (!candidateItem.hidden) {
                return candidateIndex;
            }
        }
        return -1;
    }

    function findNextItem(selectedIndex, direction) {
        if (selectedIndex == -1) {
            return findFirstItem();
        }

        const selectedItem = windowsRepeater.itemAt(selectedIndex);
        let nextIndex = -1;

        switch (direction) {
        case WindowHeap.Direction.Left:
            for (let candidateIndex = 0; candidateIndex < windowsRepeater.count; ++candidateIndex) {
                const candidateItem = windowsRepeater.itemAt(candidateIndex);
                if (candidateItem.hidden) {
                    continue;
                }

                if (candidateItem.y + candidateItem.height <= selectedItem.y) {
                    continue;
                } else if (selectedItem.y + selectedItem.height <= candidateItem.y) {
                    continue;
                }

                if (candidateItem.x + candidateItem.width < selectedItem.x + selectedItem.width) {
                    if (nextIndex == -1) {
                        nextIndex = candidateIndex;
                    } else {
                        const nextItem = windowsRepeater.itemAt(nextIndex);
                        if (candidateItem.x + candidateItem.width > nextItem.x + nextItem.width) {
                            nextIndex = candidateIndex;
                        }
                    }
                }
            }
            break;
        case WindowHeap.Direction.Right:
            for (let candidateIndex = 0; candidateIndex < windowsRepeater.count; ++candidateIndex) {
                const candidateItem = windowsRepeater.itemAt(candidateIndex);
                if (candidateItem.hidden) {
                    continue;
                }

                if (candidateItem.y + candidateItem.height <= selectedItem.y) {
                    continue;
                } else if (selectedItem.y + selectedItem.height <= candidateItem.y) {
                    continue;
                }

                if (selectedItem.x < candidateItem.x) {
                    if (nextIndex == -1) {
                        nextIndex = candidateIndex;
                    } else {
                        const nextItem = windowsRepeater.itemAt(nextIndex);
                        if (nextIndex == -1 || candidateItem.x < nextItem.x) {
                            nextIndex = candidateIndex;
                        }
                    }
                }
            }
            break;
        case WindowHeap.Direction.Up:
            for (let candidateIndex = 0; candidateIndex < windowsRepeater.count; ++candidateIndex) {
                const candidateItem = windowsRepeater.itemAt(candidateIndex);
                if (candidateItem.hidden) {
                    continue;
                }

                if (candidateItem.x + candidateItem.width <= selectedItem.x) {
                    continue;
                } else if (selectedItem.x + selectedItem.width <= candidateItem.x) {
                    continue;
                }

                if (candidateItem.y + candidateItem.height < selectedItem.y + selectedItem.height) {
                    if (nextIndex == -1) {
                        nextIndex = candidateIndex;
                    } else {
                        const nextItem = windowsRepeater.itemAt(nextIndex);
                        if (nextItem.y + nextItem.height < candidateItem.y + candidateItem.height) {
                            nextIndex = candidateIndex;
                        }
                    }
                }
            }
            break;
        case WindowHeap.Direction.Down:
            for (let candidateIndex = 0; candidateIndex < windowsRepeater.count; ++candidateIndex) {
                const candidateItem = windowsRepeater.itemAt(candidateIndex);
                if (candidateItem.hidden) {
                    continue;
                }

                if (candidateItem.x + candidateItem.width <= selectedItem.x) {
                    continue;
                } else if (selectedItem.x + selectedItem.width <= candidateItem.x) {
                    continue;
                }

                if (selectedItem.y < candidateItem.y) {
                    if (nextIndex == -1) {
                        nextIndex = candidateIndex;
                    } else {
                        const nextItem = windowsRepeater.itemAt(nextIndex);
                        if (candidateItem.y < nextItem.y) {
                            nextIndex = candidateIndex;
                        }
                    }
                }
            }
            break;
        }

        return nextIndex;
    }

    function resetSelected() {
        heap.selectedIndex = -1;
    }

    function selectNextItem(direction) {
        const nextIndex = findNextItem(heap.selectedIndex, direction);
        if (nextIndex != -1) {
            heap.selectedIndex = nextIndex;
        }
    }

    function selectLastItem(direction) {
        let last = heap.selectedIndex;
        while (true) {
            const next = findNextItem(last, direction);
            if (next == -1) {
                break;
            } else {
                last = next;
            }
        }
        if (last != -1) {
            heap.selectedIndex = last;
        }
    }

    onActiveFocusChanged: resetSelected();
    onFilterChanged: resetSelected();

    Keys.onPressed: {
        switch (event.key) {
        case Qt.Key_Escape:
            effect.deactivate();
            break;
        case Qt.Key_Up:
            selectNextItem(WindowHeap.Direction.Up);
            break;
        case Qt.Key_Down:
            selectNextItem(WindowHeap.Direction.Down);
            break;
        case Qt.Key_Left:
            selectNextItem(WindowHeap.Direction.Left);
            break;
        case Qt.Key_Right:
            selectNextItem(WindowHeap.Direction.Right);
            break;
        case Qt.Key_Home:
            selectLastItem(WindowHeap.Direction.Left);
            break;
        case Qt.Key_End:
            selectLastItem(WindowHeap.Direction.Right);
            break;
        case Qt.Key_PageUp:
            selectLastItem(WindowHeap.Direction.Up);
            break;
        case Qt.Key_PageDown:
            selectLastItem(WindowHeap.Direction.Down);
            break;
        case Qt.Key_Return:
        case Qt.Key_Space:
            let selectedItem = null;
            if (heap.selectedIndex != -1) {
                selectedItem = windowsRepeater.itemAt(heap.selectedIndex);
            } else {
                // If the window heap has only one visible window, activate it.
                for (let i = 0; i < windowsRepeater.count; ++i) {
                    const candidateItem = windowsRepeater.itemAt(i);
                    if (candidateItem.hidden) {
                        continue;
                    } else if (selectedItem) {
                        selectedItem = null;
                        break;
                    }
                    selectedItem = candidateItem;
                }
            }
            if (selectedItem) {
                KWinComponents.Workspace.activeClient = selectedItem.client;
                effect.deactivate();
            }
            break;
        default:
            return;
        }
        event.accepted = true;
    }
}
