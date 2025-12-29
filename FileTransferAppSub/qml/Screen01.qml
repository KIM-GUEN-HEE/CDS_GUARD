import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import FileTransfer 1.0

Item {
    id: root
    // 부모의 크기를 따라가도록 설정
    anchors.fill: parent

    TransferController { id: backend }

    Connections {
        target: backend
        function onLogMessage(msg) {
            log.text += msg + "\n"
        }
    }

    FileDialog {
        id: fileDialog
        nameFilters: ["All files (*)"]
        onAccepted: {
            txt_text_input.text = fileDialog.selectedFile.toString();
        }
    }

    // 1. 스크롤 가능하게 만드는 Flickable
    Flickable {
        id: flickable
        anchors.fill: parent
        contentWidth: parent.width
        contentHeight: columnLayout.implicitHeight + 40 // 내용의 실제 높이만큼 스크롤 범위 설정
        clip: true

        // 2. 내용물을 담는 레이아웃
        ColumnLayout {
            id: columnLayout
            width: parent.width - 40 // 좌우 여백 확보
            anchors.horizontalCenter: parent.horizontalCenter
            spacing: 15

            // 상단 여백
            Item { Layout.preferredHeight: 10 }

            Label {
                id: lbl_title_sender
                text: qsTr("Sender")
                font.pixelSize: 22
                font.bold: true
                Layout.fillWidth: true
            }

            GridLayout {
                id: gridLayout
                columns: root.width > 500 ? 2 : 1 // 창 너비에 따라 1열 또는 2열로 변환 (반응형)
                Layout.fillWidth: true
                rowSpacing: 10
                columnSpacing: 10

                Label { text: qsTr("Dest. IP Address"); font.bold: true; Layout.fillWidth: true }
                TextField {
                    id: txt_ip
                    placeholderText: qsTr("Enter an IP address")
                    Layout.fillWidth: true
                }

                Label { text: qsTr("Dest. Port Number"); font.bold: true; Layout.fillWidth: true }
                TextField {
                    id: txt_port
                    placeholderText: qsTr("Enter a port number")
                    Layout.fillWidth: true
                }

                Label { text: qsTr("Content Format"); font.bold: true; Layout.fillWidth: true }
                RowLayout {
                    RadioButton {
                        id: radio_btn_text
                        text: qsTr("Text")
                        checked: true
                        onCheckedChanged: {
                            btn_file_dialog.enabled = !checked
                            txt_text_input.enabled = checked
                        }
                    }
                    RadioButton {
                        id: radio_btn_file
                        text: qsTr("File")
                    }
                }

                Label { text: qsTr("Text/Path"); font.bold: true; Layout.fillWidth: true }
                TextField {
                    id: txt_text_input
                    placeholderText: qsTr("Enter text or file path")
                    Layout.fillWidth: true
                }

                Label { text: qsTr("File Select"); font.bold: true; Layout.fillWidth: true }
                Button {
                    id: btn_file_dialog
                    text: qsTr("Open File")
                    enabled: false
                    Layout.fillWidth: true
                    onClicked: fileDialog.open()
                }
            }

            Button {
                id: btn_send
                text: qsTr("Send")
                Layout.fillWidth: true
                onClicked: {
                    backend.sendData(
                        txt_ip.text || "127.0.0.1",
                        parseInt(txt_port.text) || 5000,
                        txt_text_input.text,
                        radio_btn_text.checked
                    )
                }
            }

            Rectangle {
                height: 1
                Layout.fillWidth: true
                color: "#DDDDDD"
                Layout.topMargin: 10
                Layout.bottomMargin: 10
            }

            Label {
                id: lbl_title_receiver
                text: qsTr("Receiver")
                font.pixelSize: 22
                font.bold: true
                Layout.fillWidth: true
            }

            RowLayout {
                Layout.fillWidth: true
                Button {
                    id: btn_start_receiver
                    text: qsTr("Start Listening")
                    Layout.fillWidth: true
                    onClicked: {
                        backend.startListening(parseInt(txt_port.text) || 5000)
                        btn_start_receiver.enabled = false
                        btn_stop_receiver.enabled = true
                    }
                }
                Button {
                    id: btn_stop_receiver
                    text: qsTr("Stop Listening")
                    enabled: false
                    Layout.fillWidth: true
                    onClicked: {
                        backend.stopListening()
                        btn_start_receiver.enabled = true
                        btn_stop_receiver.enabled = false
                    }
                }
            }

            Label { text: qsTr("Log"); font.bold: true }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 200 // 로그 창 높이 고정
                border.color: "#888888"
                radius: 4

                ScrollView { // 로그 내부에도 스크롤 적용
                    anchors.fill: parent
                    TextArea {
                        id: log
                        readOnly: true
                        font.pixelSize: 13
                        wrapMode: Text.Wrap
                    }
                }
            }

            Item { Layout.preferredHeight: 20 } // 하단 여백
        }

        // 3. 스크롤바 표시
        ScrollBar.vertical: ScrollBar {
            policy: ScrollBar.AsNeeded
        }
    }
}
