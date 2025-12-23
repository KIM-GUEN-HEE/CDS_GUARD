

/*
This is a UI file (.ui.qml) that is intended to be edited in Qt Design Studio only.
It is supposed to be strictly declarative and only uses a subset of QML. If you edit
this file manually, you might introduce QML code that is not supported by Qt Design Studio.
Check out https://doc.qt.io/qtcreator/creator-quick-ui-forms.html for details on .ui.qml files.
*/
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import FileTransfer 1.0

Rectangle {
    width: 720
    height: 900
    // color: "#121212"
    color: "white"

    TransferController { id: backend }
    
    Connections {
        target: backend
        function onLogMessage(msg) 
        {
            log.text += msg + "\n"
        }
    }

    FileDialog {
        id: fileDialog
        nameFilters: ["All files (*)"]
        onAccepted: {
            // fileDialog.selectedFile → URL 형식이므로 toString() + replace 사용
            txt_text_input.text = fileDialog.selectedFile.toString();
        }
    }

    ColumnLayout {
        id: columnLayout
        x: 8
        y: 8
        width: 704
        height: 885

        Label {
            id: lbl_title_sender
            text: qsTr("Sender")
            font.pixelSize: 20
            font.bold: true
            Layout.fillWidth: true
            Layout.preferredWidth: 100
            Layout.bottomMargin: 5
        }

        GridLayout {
            id: gridLayout
            width: 100
            height: 100
            columns: 2

            Label {
                id: lbl_ip
                text: qsTr("Dest. IP Address")
                font.pixelSize: 14
                font.bold: true
                Layout.fillWidth: true
                Layout.preferredWidth: 100
            }

            TextField {
                id: txt_ip
                placeholderText: qsTr("Enter an IP address")
                Layout.fillWidth: true
                Layout.preferredWidth: 300
            }

            Label {
                id: lbl_port
                text: qsTr("Dest. Port Number")
                font.pixelSize: 14
                font.bold: true
                Layout.fillWidth: true
                Layout.preferredWidth: 100
            }

            TextField {
                id: txt_port
                placeholderText: qsTr("Enter a port number")
                Layout.fillWidth: true
                Layout.preferredWidth: 300
            }

            Label {
                id: lbl_format
                text: qsTr("Content Format")
                font.pixelSize: 14
                font.bold: true
                Layout.fillWidth: true
                Layout.preferredWidth: 100
            }

            RowLayout {
                id: rowLayout
                Layout.fillWidth: true
                Layout.preferredWidth: 300

                ButtonGroup {
                    id: format_group
                }

                RadioButton {
                    id: radio_btn_text
                    text: qsTr("Text")
                    checked: true
                    ButtonGroup.group: format_group
                    onCheckedChanged: {
                        btn_file_dialog.enabled = !checked
                        txt_text_input.enabled = checked
                    }
                }

                RadioButton {
                    id: radio_btn_file
                    text: qsTr("File")
                    ButtonGroup.group: format_group
                }
            }

            Label {
                id: lbl_text_input
                text: qsTr("Text")
                font.pixelSize: 14
                font.bold: true
                Layout.fillWidth: true
                Layout.preferredWidth: 100
            }

            TextField {
                id: txt_text_input
                placeholderText: qsTr("Enter text to send")
                Layout.fillWidth: true
                Layout.preferredWidth: 300
            }

            Label {
                id: lbl_file_input
                text: qsTr("File")
                font.pixelSize: 14
                font.bold: true
                Layout.fillWidth: true
                Layout.preferredWidth: 100
            }

            Button {
                id: btn_file_dialog
                text: qsTr("Open File")
                enabled: false
                onClicked: fileDialog.open()
            }
        }

        // Button {
        //     text: "눌러보세요"
        //     onClicked: backend.doSomething()
        // }

        Button {
            id: btn_send
            text: qsTr("Send")
            Layout.fillWidth: true
            Layout.topMargin: 5

            onClicked: {
                backend.sendData(
                    txt_ip.text || "127.0.0.1",           // IP 주소 기본값
                    parseInt(txt_port.text) || 5000,      // 포트 번호 기본값
                    txt_text_input.text,                  // 텍스트 내용
                    radio_btn_text.checked                // true: 텍스트, false: 파일
                )
            }
        }

        Rectangle {
            height: 1
            width: parent.width
            color: "#FFFFFF"
            Layout.topMargin: 20
            Layout.bottomMargin: 20
        }

        Label {
            id: lbl_title_receiver
            text: qsTr("Receiver")
            font.pixelSize: 20
            font.bold: true
            Layout.fillWidth: true
            Layout.preferredWidth: 100
            Layout.bottomMargin: 5
        }

        GridLayout {
            id: gridLayout1
            width: 100
            height: 100
            columns: 2

            Label {
                id: lbl_receiver
                text: qsTr("Listening")
                font.pixelSize: 14
                font.bold: true
                Layout.fillWidth: true
                Layout.preferredWidth: 100
            }

            RowLayout {
                id: rowLayout1
                Layout.fillWidth: true
                Layout.preferredWidth: 300

                Button {
                    id: btn_start_receiver
                    text: qsTr("Start Listening")
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
                    onClicked: {
                        backend.stopListening()
                        btn_start_receiver.enabled = true
                        btn_stop_receiver.enabled = false
                    }
                }
            }

            Label {
                id: lbl_log
                text: qsTr("Log")
                font.pixelSize: 14
                font.bold: true
                Layout.fillWidth: true
                Layout.preferredWidth: 100
            }

            Rectangle {
                border.color: "#888888"
                border.width: 1
                radius: 4
                color: "transparent"
                Layout.fillWidth: true
                Layout.preferredWidth: 300
                Layout.fillHeight: true

                Text {
                    id: log
                    color: "#333333"  // 더 잘 보이게 진한 글씨
                    text: ""           // 초기 텍스트 제거
                    font.pixelSize: 13
                    wrapMode: Text.Wrap  // 줄바꿈
                    anchors.fill: parent
                    anchors.margins: 5
                }
            }
        }
    }
}
