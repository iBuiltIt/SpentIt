import QtQuick
import QtQuick.Controls

TextField {
    id: field
    property var model: []
    property string savedText: ""
    property bool allowUncategorized: false
    property bool restoreOnFocusLoss: true
    property real popupPreferredWidth: width
    property string popupObjectName: objectName + "Dropdown"
    property string matchesObjectName: objectName + "Matches"
    property string scrollBarObjectName: objectName + "ScrollBar"
    property var choices: []
    property bool choosing: false
    signal choiceAccepted(var choice)
    signal backwardRequested()
    signal escapeRequested()
    property bool canUndoAssignment: false
    signal undoAssignmentRequested()
    text: savedText
    selectByMouse: true
    maximumLength: 256
    onSavedTextChanged: resetText()

    function resetText() { text=savedText; completion.close() }
    function refreshChoices() {
        let query=text.toLowerCase()
        let rows=model.filter(c=>(c.path+" "+(c.aliases||"")).toLowerCase().indexOf(query)>=0)
        if(!query && allowUncategorized)rows.unshift({id:"",path:"",uncategorized:true})
        choices=rows
        matches.currentIndex=rows.length?0:-1
        matches.positionViewAtBeginning()
    }
    function openChoices() {if(!readOnly){refreshChoices();completion.open()}}
    function selectIndex(index) {
        if(readOnly || index<0 || index>=model.length)return
        choose(model[index])
    }
    function choose(choice) {
        choosing=true
        text=choice.path
        completion.close()
        choiceAccepted(choice)
        choosing=false
    }
    function acceptChoice() {
        if(matches.currentIndex>=0 && matches.currentIndex<choices.length)choose(choices[matches.currentIndex])
    }
    onActiveFocusChanged: {
        if(activeFocus) {
            openChoices()
            // Run after the initial mouse press positions the caret.
            Qt.callLater(function(){if(field.activeFocus)field.selectAll()})
        } else if(!choosing) {
            completion.close()
            if(restoreOnFocusLoss)text=savedText
        }
    }
    onPressed: openChoices()
    onTextEdited: {refreshChoices();completion.open()}
    Keys.priority: Keys.BeforeItem
    Keys.onPressed: function(event) {
        if(event.key===Qt.Key_Backtab || (event.key===Qt.Key_Tab && (event.modifiers & Qt.ShiftModifier))) {
            resetText();backwardRequested();event.accepted=true
        } else if(event.key===Qt.Key_Return || event.key===Qt.Key_Enter || event.key===Qt.Key_Tab) {
            if(!completion.visible)openChoices()
            acceptChoice();event.accepted=true
        } else if(event.key===Qt.Key_Down || event.key===Qt.Key_Up) {
            if(!completion.visible)openChoices()
            else matches.currentIndex=Math.max(0,Math.min(choices.length-1,matches.currentIndex+(event.key===Qt.Key_Down?1:-1)))
            matches.positionViewAtIndex(matches.currentIndex,ListView.Contain);event.accepted=true
        } else if(event.key===Qt.Key_Escape) {
            if(completion.visible)resetText();else escapeRequested()
            event.accepted=true
        } else if(event.matches(StandardKey.Undo) && text===savedText && canUndoAssignment) {
            undoAssignmentRequested();event.accepted=true
        }
    }
    Popup {
        id:completion;objectName:field.popupObjectName
        y:field.height
        readonly property real windowWidth: field.Window.window ? field.Window.window.width : field.width
        width:Math.min(windowWidth-40,Math.max(field.width,field.popupPreferredWidth))
        x:field.Window.window?Math.min(0,windowWidth-20-field.mapToItem(field.Window.window.contentItem,0,0).x-width):0
        height:Math.min(280,Math.max(44,matches.contentHeight)+8)
        padding:4;focus:false;closePolicy:Popup.CloseOnPressOutside
        contentItem:ListView {
            id:matches;objectName:field.matchesObjectName
            clip:true;model:field.choices;currentIndex:-1
            boundsBehavior:Flickable.StopAtBounds
            ScrollBar.vertical:ScrollBar {
                id:bar;objectName:field.scrollBarObjectName
                z:10;policy:ScrollBar.AsNeeded;interactive:true;minimumSize:0.08
                implicitWidth:16;implicitHeight:16;padding:2
                contentItem:Rectangle {
                    visible:bar.size>0 && bar.size<0.999
                    implicitWidth:12;implicitHeight:12;radius:6
                    color:bar.pressed?"#52687b":bar.hovered?"#73879a":"#94a3b0"
                }
                background:Rectangle {color:"#e9edf0";radius:8;visible:bar.size>0 && bar.size<0.999}
            }
            delegate:ItemDelegate {
                required property var modelData
                required property int index
                objectName:"inlineCategoryChoice_"+(modelData.account?modelData.accountId:modelData.id||"uncategorized")
                width:matches.width-16
                text:modelData.uncategorized?"Uncategorized":modelData.path+(modelData.account?" [Account]":"")
                contentItem:Label {
                    text:parent.text;textFormat:Text.PlainText
                    verticalAlignment:Text.AlignVCenter;elide:Text.ElideRight
                }
                highlighted:index===matches.currentIndex
                focusPolicy:Qt.NoFocus
                onClicked:{matches.currentIndex=index;field.acceptChoice()}
            }
            Label {anchors.centerIn:parent;text:"No matches";visible:field.choices.length===0}
        }
    }
}
