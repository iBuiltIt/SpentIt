import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ToolButton {
    id: root
    required property var workflow
    required property var field
    signal selected(string value)
    text: "▦"
    Accessible.name: "Choose date"
    ToolTip.visible: hovered
    ToolTip.text: "Choose date"
    property string feedback: ""
    property var page: ({})
    property string original: ""
    property string cursor: ""
    property int view: 0
    readonly property var months: ["Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"]
    function browse(iso) {
        if(!iso)return
        cursor=iso
        page=workflow.calendarPage(workflow.dateDisplay(iso))
        if(view===2)years.positionViewAtIndex(Math.max(0,page.year-1),GridView.Contain)
    }
    function choose(iso) {
        if(!iso)return
        selected(workflow.dateDisplay(iso))
        popup.close()
        field.forceActiveFocus()
    }
    onClicked: {
        const value=workflow.calendarPage(field.text)
        if(value.error){feedback=value.error;errorTip.open();return}
        feedback="";original=value.iso;cursor=value.iso;page=value;view=0
        const point=mapToItem(Overlay.overlay,0,height)
        popup.x=Math.max(0,Math.min(point.x,Overlay.overlay.width-popup.width))
        popup.y=Math.max(0,Math.min(point.y,Overlay.overlay.height-popup.height))
        popup.open();keys.forceActiveFocus()
    }
    ToolTip {id:errorTip;text:root.feedback;timeout:5000}
    Popup {
        id:popup;objectName:"calendarPopup";parent:root.Overlay.overlay
        width:322;height:366;padding:10;modal:false;focus:true;popupType:Popup.Item
        closePolicy:Popup.CloseOnPressOutside
        onOpened:{popup.forceActiveFocus();keys.forceActiveFocus();Qt.callLater(function(){if(popup.opened){popup.forceActiveFocus();keys.forceActiveFocus()}})}
        contentItem: FocusScope {
            id:keys;objectName:"calendarKeys"
            Keys.onPressed: function(event) {
                event.accepted=true
                if(event.key===Qt.Key_Escape){if(root.view!==0)root.view=0;else popup.close();return}
                if(event.key===Qt.Key_Return || event.key===Qt.Key_Enter){if(root.view===0)root.choose(root.cursor);else root.view=0;return}
                let step=event.key===Qt.Key_Left?-1:event.key===Qt.Key_Right?1:event.key===Qt.Key_Up?(root.view===0?-7:-4):event.key===Qt.Key_Down?(root.view===0?7:4):0
                if(step)root.browse(root.workflow.calendarMove(root.cursor,root.view===0?step:0,root.view===1?step:0,root.view===2?step:0))
                else event.accepted=false
            }
            ColumnLayout {
                anchors.fill:parent;spacing:4
                RowLayout {
                    Layout.fillWidth:true
                    ToolButton {text:"‹";Accessible.name:"Previous month";onClicked:{root.browse(root.workflow.calendarMove(root.cursor,0,-1));keys.forceActiveFocus()}}
                    Button {text:root.months[(root.page.month||1)-1];Layout.fillWidth:true;onClicked:{root.view=1;keys.forceActiveFocus()}}
                    Button {text:root.page.year||"";Layout.fillWidth:true;onClicked:{root.view=2;years.positionViewAtIndex(Math.max(0,(root.page.year||1)-9),GridView.Beginning);keys.forceActiveFocus()}}
                    ToolButton {text:"›";Accessible.name:"Next month";onClicked:{root.browse(root.workflow.calendarMove(root.cursor,0,1));keys.forceActiveFocus()}}
                }
                Item {
                    Layout.fillWidth:true;Layout.fillHeight:true
                    ColumnLayout {
                        anchors.fill:parent;visible:root.view===0;spacing:0
                        RowLayout {Layout.fillWidth:true;spacing:0;Repeater {model:["Su","Mo","Tu","We","Th","Fr","Sa"];Label {required property string modelData;text:modelData;Layout.fillWidth:true;horizontalAlignment:Text.AlignHCenter}}}
                        GridLayout {
                            columns:7;rowSpacing:0;columnSpacing:0;Layout.fillWidth:true;Layout.fillHeight:true
                            Repeater {
                                model:root.page.days||[]
                                Button {
                                    required property var modelData
                                    Layout.fillWidth:true;Layout.fillHeight:true;Layout.preferredWidth:40
                                    text:modelData.day;enabled:!!modelData.iso
                                    highlighted:modelData.iso===root.original
                                    // This opacity replaces the style fade. 0.4 is the unavailable factor in controls/Button.qml.
                                    opacity:(modelData.month===root.page.month?1:0.55)*(enabled?1:0.4)
                                    Accessible.name:modelData.display
                                    Rectangle {anchors.fill:parent;anchors.margins:3;color:"transparent";radius:4;border.width:parent.modelData.today?2:parent.modelData.iso===root.cursor?1:0;border.color:palette.highlight}
                                    onClicked:root.choose(modelData.iso)
                                }
                            }
                        }
                    }
                    GridLayout {
                        anchors.fill:parent;visible:root.view===1;columns:4
                        Repeater {model:root.months;Button {required property string modelData;required property int index;text:modelData;Layout.fillWidth:true;Layout.fillHeight:true;highlighted:index+1===root.page.month;onClicked:{root.browse(root.workflow.calendarMove(root.cursor,0,index+1-root.page.month));root.view=0;keys.forceActiveFocus()}}}
                    }
                    GridView {
                        id:years;anchors.fill:parent;visible:root.view===2;clip:true;model:9999;cellWidth:width/4;cellHeight:40
                        ScrollBar.vertical:ScrollBar {}
                        delegate:Button {required property int index;width:years.cellWidth;height:40;text:index+1;highlighted:index+1===root.page.year;onClicked:{root.browse(root.workflow.calendarMove(root.cursor,0,0,index+1-root.page.year));root.view=0;keys.forceActiveFocus()}}
                    }
                }
                Button {text:"Today";Layout.alignment:Qt.AlignHCenter;onClicked:root.choose(root.page.today)}
            }
        }
    }
}
