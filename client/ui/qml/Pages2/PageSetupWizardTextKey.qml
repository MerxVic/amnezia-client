import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import PageEnum 1.0
import Style 1.0

import "./"
import "../Controls2"
import "../Controls2/TextTypes"
import "../Config"

PageType {
    id: root

    FlickableType {
        id: flickable

        anchors.top: parent.top
        anchors.bottom: bottomBar.top

        contentHeight: content.implicitHeight + 16

        ColumnLayout {
            id: content

            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right

            spacing: 0

            NavigationBarType {
                Layout.fillWidth: true

                title: qsTr("Connection key")
                description: qsTr("A line that starts with vpn://...")
            }

            TextFieldType {
                id: textKey

                Layout.fillWidth: true
                Layout.rightMargin: 16
                Layout.leftMargin: 16

                headerText: qsTr("Key")
                textField.placeholderText: "vpn://"
                buttonText: qsTr("Insert")

                clickedFunc: function() {
                    textField.text = ""
                    textField.paste()
                }
            }

            ButtonType {
                Layout.fillWidth: true
                Layout.leftMargin: 16
                Layout.rightMargin: 16
                text: qsTr("Get my profile from panel")
                onClicked: panelLoginDialog.open()
            }
        }
    }

    Connections {
        target: VdsPanelClient
        function onProfileReady(profile) {
            if (ImportController.extractConfigFromData(profile)) {
                PageController.goToPage(PageEnum.PageSetupWizardViewConfig)
            }
        }
        function onErrorOccurred(message) {
            PageController.showErrorMessage(message)
        }
    }

    Dialog {
        id: panelLoginDialog
        parent: Overlay.overlay
        anchors.centerIn: parent
        width: Math.min(parent.width - 32, 440)
        modal: true
        title: qsTr("VPN panel")
        standardButtons: Dialog.Ok | Dialog.Cancel
        onAccepted: {
            VdsPanelClient.fetchOwnProfile(panelUrl.text, panelUsername.text, panelPassword.text)
            panelPassword.text = ""
        }

        contentItem: ColumnLayout {
            spacing: 8
            TextField {
                id: panelUrl
                Layout.fillWidth: true
                placeholderText: qsTr("https://panel.example:8443/")
                inputMethodHints: Qt.ImhNoPredictiveText | Qt.ImhNoAutoUppercase
            }
            TextField {
                id: panelUsername
                Layout.fillWidth: true
                placeholderText: qsTr("Username")
                inputMethodHints: Qt.ImhNoPredictiveText | Qt.ImhNoAutoUppercase
            }
            TextField {
                id: panelPassword
                Layout.fillWidth: true
                placeholderText: qsTr("Password")
                echoMode: TextInput.Password
            }
        }
    }

    BottomBarType {
        id: bottomBar

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom

        ButtonType {
            id: continueButton

            Layout.fillWidth: true

            text: qsTr("Continue")

            onClicked: {
                if (ImportController.extractConfigFromData(textKey.textField.text)) {
                    PageController.goToPage(PageEnum.PageSetupWizardViewConfig)
                }
            }
        }
    }
}
