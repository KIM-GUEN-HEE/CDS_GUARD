import QtQuick
import QtQuick.Controls

Window {
    id: window
    width: 500  // 초기 너비
    height: 700 // 초기 높이
    minimumWidth: 400 // 최소 너비 제한
    minimumHeight: 500

    visible: true
    title: "File Transfer System"

    Screen01 {
        // 부모 창의 크기에 맞게 꽉 채움
        anchors.fill: parent
    }
}
