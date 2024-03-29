/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

import QtQuick 2.12
import org.kde.kwin 3.0 as KWinComponents
import org.kde.kwin.private.overview 1.0
import org.kde.plasma.components 3.0 as PC3
import org.kde.plasma.core 2.0 as PlasmaCore

FocusScope {
    id: container
    focus: true

    property bool animationEnabled: false
    property bool organized: false

    function start() {
        container.animationEnabled = true;
        container.organized = true;
    }

    function stop() {
        container.organized = false;
    }

    Repeater {
        model: KWinComponents.ClientFilterModel {
            activity: KWinComponents.Workspace.currentActivity
            desktop: KWinComponents.Workspace.currentVirtualDesktop
            screenName: targetScreen.name
            clientModel: stackModel
            windowType: KWinComponents.ClientFilterModel.Desktop
        }

        KWinComponents.WindowThumbnailItem {
            id: windowThumbnail
            wId: model.client.internalId
            x: model.client.x - targetScreen.geometry.x
            y: model.client.y - targetScreen.geometry.y
            width: model.client.width
            height: model.client.height
        }
    }

    Rectangle {
        id: underlay
        anchors.fill: parent
        color: PlasmaCore.ColorScope.backgroundColor
        state: container.organized ? "active" : "initial"

        states: [
            State {
                name: "initial"
                PropertyChanges {
                    target: underlay
                    opacity: 0
                }
            },
            State {
                name: "active"
                PropertyChanges {
                    target: underlay
                    opacity: 0.4
                }
            }
        ]

        Behavior on opacity {
            OpacityAnimator { duration: effect.animationDuration }
        }

        TapHandler {
            onTapped: effect.deactivate();
        }
    }

    ExpoArea {
        id: heapArea
        screen: targetScreen
    }

    Column {
        x: heapArea.x
        y: heapArea.y
        width: heapArea.width
        height: heapArea.height

        Item {
            id: searchBar
            width: parent.width
            height: searchField.height + 2 * PlasmaCore.Units.largeSpacing
            state: container.organized ? "visible" : "hidden"

            PC3.TextField {
                id: searchField
                anchors.centerIn: parent
                width: Math.min(parent.width, 20 * PlasmaCore.Units.gridUnit)
                focus: true
                placeholderText: i18n("Search...")
                clearButtonShown: true
                Keys.priority: Keys.AfterItem
                Keys.forwardTo: heap
            }

            states: [
                State {
                    name: "hidden"
                    PropertyChanges {
                        target: searchBar
                        opacity: 0
                    }
                },
                State {
                    name: "visible"
                    PropertyChanges {
                        target: searchBar
                        opacity: 1
                    }
                }
            ]

            transitions: [
                Transition {
                    from: "hidden"; to: "visible"
                    OpacityAnimator {
                        duration: effect.animationDuration; easing.type: Easing.OutCubic
                    }
                },
                Transition {
                    from: "visible"; to: "hidden"
                    OpacityAnimator {
                        duration: effect.animationDuration; easing.type: Easing.InCubic
                    }
                }
            ]
        }

        WindowHeap {
            id: heap
            width: parent.width
            height: parent.height - searchBar.height
            padding: PlasmaCore.Units.largeSpacing
            filter: searchField.text
            animationEnabled: container.animationEnabled
            organized: container.organized
            model: KWinComponents.ClientFilterModel {
                activity: KWinComponents.Workspace.currentActivity
                desktop: KWinComponents.Workspace.currentVirtualDesktop
                screenName: targetScreen.name
                clientModel: stackModel
                windowType: ~KWinComponents.ClientFilterModel.Dock &
                        ~KWinComponents.ClientFilterModel.Desktop &
                        ~KWinComponents.ClientFilterModel.Notification;
            }
        }
    }

    Repeater {
        model: KWinComponents.ClientFilterModel {
            desktop: KWinComponents.Workspace.currentVirtualDesktop
            screenName: targetScreen.name
            clientModel: stackModel
            windowType: KWinComponents.ClientFilterModel.Dock
        }

        KWinComponents.WindowThumbnailItem {
            id: windowThumbnail
            wId: model.client.internalId
            x: model.client.x - targetScreen.geometry.x
            y: model.client.y - targetScreen.geometry.y
            width: model.client.width
            height: model.client.height

            TapHandler {
                onTapped: effect.deactivate();
            }
        }
    }

    KWinComponents.ClientModel {
        id: stackModel
    }

    Component.onCompleted: start();
}
