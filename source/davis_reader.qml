R""(
    import Chameleon 1.0
    import QtQuick 2.7
    import QtQuick.Layouts 1.1
    import QtQuick.Window 2.2
    Window {
        id: window
        visible: true
        width: 1.5*header_width
        height: 1.5*header_height
        Timer {
            interval: 20
            running: true
            repeat: true
            onTriggered: {
                dvs_display.trigger_draw();
            }
        }
        BackgroundCleaner {
            width: window.width
            height: window.height
        }
        DvsDisplay {
            objectName: "dvs_display"
            id: dvs_display
            width: window.width
            height: window.height
            //decay: 1e4
            //Layout.fillWidth: true
            //Layout.fillHeight: true
            Layout.alignment: Qt.AlignHCenter | Qt.AlignVCenter
            canvas_size: Qt.size(header_width, header_height)
        }
    }
)""
