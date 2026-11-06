import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import QtCore
ApplicationWindow {
    id: window
    readonly property var workflowBackend: workflow
    Theme { id: theme; objectName: "applicationTheme" }
    color: theme.window
    palette.window: theme.window
    palette.windowText: theme.text
    palette.base: theme.surface
    palette.alternateBase: theme.alternate
    palette.text: theme.text
    palette.button: theme.button
    palette.buttonText: theme.text
    palette.brightText: theme.text
    palette.highlight: theme.highlight
    palette.highlightedText: theme.highlightedText
    palette.placeholderText: theme.muted
    palette.mid: theme.border
    palette.dark: theme.border
    // Basic menus and delegates paint these roles behind their selection text.
    palette.light: theme.selection
    palette.midlight: theme.selectionPressed
    palette.shadow: theme.window
    palette.toolTipBase: theme.surface
    palette.toolTipText: theme.text
    palette.link: theme.focus
    palette.linkVisited: theme.focus
    palette.accent: theme.focus
    palette.disabled.windowText: theme.disabled
    palette.disabled.text: theme.disabled
    palette.disabled.buttonText: theme.disabled
    palette.disabled.highlightedText: theme.disabled
    component UsableScrollBar: ScrollBar {
        id:bar;z:10;policy:ScrollBar.AsNeeded;interactive:true;minimumSize:0.08
        implicitWidth:16;implicitHeight:16;padding:2
        contentItem:Rectangle {visible:bar.size>0 && bar.size<0.999;implicitWidth:12;implicitHeight:12;radius:6;color:bar.pressed?theme.scrollPressed:bar.hovered?theme.scrollHover:theme.scrollThumb}
        background:Rectangle {color:theme.scrollTrack;radius:8;visible:bar.size>0 && bar.size<0.999}
    }
    component UsableScrollView: ScrollView {
        id:view
        ScrollBar.vertical:UsableScrollBar {
            parent:view;x:view.mirrored?0:view.width-width;y:view.topPadding;height:view.availableHeight
        }
        ScrollBar.horizontal:UsableScrollBar {
            parent:view;x:view.leftPadding;y:view.height-height;width:view.availableWidth
        }
    }
    component UsableComboBox: ComboBox {
        id:combo
        property real popupPreferredWidth: 180
        delegate: ItemDelegate {
            required property var model
            required property int index
            objectName: "comboChoice_" + index
            width: ListView.view.width
            text: model[combo.textRole]
            contentItem: Label {
                text: parent.text; textFormat: Text.PlainText
                color: parent.enabled ? theme.text : theme.disabled
                font: parent.font
                verticalAlignment: Text.AlignVCenter; elide: Text.ElideRight
            }
            font.weight: combo.currentIndex === index ? Font.DemiBold : Font.Normal
            highlighted: combo.highlightedIndex === index
            hoverEnabled: combo.hoverEnabled
        }
        popup:Popup {
            onVisibleChanged:window.diagnosticModalChanged(29,visible)
            y:combo.height;width:Math.min(window.width-40,Math.max(combo.width,combo.popupPreferredWidth));padding:4
            x:Math.min(0,window.width-20-combo.mapToItem(window.contentItem,0,0).x-width)
            height:Math.min(320,menu.contentHeight+8)
            contentItem:ListView {
                id:menu;clip:true;implicitHeight:contentHeight;model:combo.popup.visible?combo.delegateModel:null
                currentIndex:combo.highlightedIndex;boundsBehavior:Flickable.StopAtBounds
                ScrollBar.vertical:UsableScrollBar {objectName:"comboScrollBar"}
            }
        }
    }
    width: 1440; height: 850; minimumWidth:720; minimumHeight:560; visible: false
    title: s.active ? "SpentIt — "+s.displayPath : "SpentIt"

    property var diagnosticModalStack: []
    function diagnosticModalChanged(code,opened) {
        if(!workflow.diagnosticsEnabled)return
        let next=diagnosticModalStack.filter(function(value){return value!==code})
        if(opened)next.push(code)
        diagnosticModalStack=next
    }
    Connections {target:diagnosticFileMenu;function onVisibleChanged(){window.diagnosticModalChanged(0,diagnosticFileMenu.visible)}}
    Connections {target:importBlockersDialog;function onVisibleChanged(){window.diagnosticModalChanged(1,importBlockersDialog.visible)}}
    Connections {target:ledgerContext;function onVisibleChanged(){window.diagnosticModalChanged(5,ledgerContext.visible)}}
    Connections {target:accountContext;function onVisibleChanged(){window.diagnosticModalChanged(6,accountContext.visible)}}
    Connections {target:overwriteDialog;function onVisibleChanged(){window.diagnosticModalChanged(7,overwriteDialog.visible)}}
    Connections {target:openDialog;function onVisibleChanged(){window.diagnosticModalChanged(8,openDialog.visible)}}
    Connections {target:createDialog;function onVisibleChanged(){window.diagnosticModalChanged(9,createDialog.visible)}}
    Connections {target:importMapping;function onVisibleChanged(){window.diagnosticModalChanged(10,importMapping.visible)}}
    Connections {target:importDialog;function onVisibleChanged(){window.diagnosticModalChanged(11,importDialog.visible)}}
    Connections {target:backupDialog;function onVisibleChanged(){window.diagnosticModalChanged(12,backupDialog.visible)}}
    Connections {target:unsaved;function onVisibleChanged(){window.diagnosticModalChanged(13,unsaved.visible)}}
    Connections {target:changePreview;function onVisibleChanged(){window.diagnosticModalChanged(14,changePreview.visible)}}
    Connections {target:mergePreview;function onVisibleChanged(){window.diagnosticModalChanged(15,mergePreview.visible)}}
    Connections {target:bulkDialog;function onVisibleChanged(){window.diagnosticModalChanged(16,bulkDialog.visible)}}
    Connections {target:reapplyDialog;function onVisibleChanged(){window.diagnosticModalChanged(17,reapplyDialog.visible)}}
    Connections {target:matchedRuleDialog;function onVisibleChanged(){window.diagnosticModalChanged(19,matchedRuleDialog.visible)}}
    Connections {target:categoryContext;function onVisibleChanged(){window.diagnosticModalChanged(20,categoryContext.visible)}}
    Connections {target:categoryPicker;function onVisibleChanged(){window.diagnosticModalChanged(21,categoryPicker.visible)}}
    Connections {target:renameDialog;function onVisibleChanged(){window.diagnosticModalChanged(22,renameDialog.visible)}}
    Connections {target:deleteAccountDialog;function onVisibleChanged(){window.diagnosticModalChanged(23,deleteAccountDialog.visible)}}
    Connections {target:correctOpeningDialog;function onVisibleChanged(){window.diagnosticModalChanged(24,correctOpeningDialog.visible)}}
    Connections {target:currencyDialog;function onVisibleChanged(){window.diagnosticModalChanged(25,currencyDialog.visible)}}
    Connections {target:importCategoriesDialog;function onVisibleChanged(){window.diagnosticModalChanged(26,importCategoriesDialog.visible)}}
    Connections {target:discardDialog;function onVisibleChanged(){window.diagnosticModalChanged(27,discardDialog.visible)}}
    // Diagnostics publishes only this explicit semantic allowlist, at <=10 Hz.
    Label {z:1000;anchors.right:parent.right;anchors.top:parent.top;text:"Diagnostics enabled";visible:workflow.diagnosticsEnabled;color:theme.warning}
    Label {
        objectName: "helpLoadError"; z: 1000
        anchors.horizontalCenter: parent.horizontalCenter; anchors.bottom: parent.bottom; anchors.bottomMargin: 16
        visible: text.length > 0; text: helpLoadError; color: theme.error
        background: Rectangle { color: theme.surface; border.color: theme.error; anchors.fill: parent; anchors.margins: -8 }
    }
    property bool diagnosticSearchValid: true
    Connections {target:workflow;function onCompleted(action,success){if(action==="query")window.diagnosticSearchValid=success}}
    function publishDiagnostics() {
        let modals=0
        if(diagnosticFileMenu.visible) modals+=Math.pow(2,0)
        if(importBlockersDialog.visible) modals+=Math.pow(2,1)
        if(ledgerContext.visible) modals+=Math.pow(2,5)
        if(accountContext.visible) modals+=Math.pow(2,6)
        if(overwriteDialog.visible) modals+=Math.pow(2,7)
        if(openDialog.visible) modals+=Math.pow(2,8)
        if(createDialog.visible) modals+=Math.pow(2,9)
        if(importMapping.visible) modals+=Math.pow(2,10)
        if(importDialog.visible) modals+=Math.pow(2,11)
        if(backupDialog.visible) modals+=Math.pow(2,12)
        if(unsaved.visible) modals+=Math.pow(2,13)
        if(changePreview.visible) modals+=Math.pow(2,14)
        if(mergePreview.visible) modals+=Math.pow(2,15)
        if(bulkDialog.visible) modals+=Math.pow(2,16)
        if(reapplyDialog.visible) modals+=Math.pow(2,17)
        if(matchedRuleDialog.visible) modals+=Math.pow(2,19)
        if(categoryContext.visible) modals+=Math.pow(2,20)
        if(categoryPicker.visible) modals+=Math.pow(2,21)
        if(renameDialog.visible) modals+=Math.pow(2,22)
        if(deleteAccountDialog.visible) modals+=Math.pow(2,23)
        if(correctOpeningDialog.visible) modals+=Math.pow(2,24)
        if(currencyDialog.visible) modals+=Math.pow(2,25)
        if(importCategoriesDialog.visible) modals+=Math.pow(2,26)
        if(discardDialog.visible) modals+=Math.pow(2,27)
        if(reviewDifference.visible || reviewSurvivor.visible || statusDetails.visible) modals+=Math.pow(2,28)
        let pending=(startupPending?1:0)+(pendingAction?2:0)+(pendingSearch?4:0)+(pendingAccountNavigation?8:0)+(pendingCounterpart?16:0)+((ledgerViewRestore || reviewViewRestoring)?32:0)+(closingRequested?64:0)+(guardHandoff?128:0)+(workflow.busy?256:0)+(probeTimer.running?512:0)
        let observedList=currentPage==="review"?reviewQuestions:ledger
        let first=currentPage==="review"?reviewVisibleIndex():observedList.indexAt(1,observedList.contentY+1)
        let last=observedList.indexAt(1,observedList.contentY+observedList.height-1)
        let count=0
        for(let i=0;i<observedList.contentItem.children.length;i++)if(observedList.contentItem.children[i].hasOwnProperty(currentPage==="review"?"modelData":"record"))count++
        // Keep the removed projected-toggle slot reserved for stable diagnostic bit indices.
        let controls=[search,null,editDate,editPayee,editMemo,editTotal,catalogSearch,ruleLabel,ruleExpression,location,unlockPassword]
        let visibleControls=0,enabledControls=0,focusRole=0
        for(let i=0;i<controls.length;i++){
            if(!controls[i])continue
            if(controls[i].visible)visibleControls+=Math.pow(2,i)
            if(controls[i].enabled)enabledControls+=Math.pow(2,i)
            if(controls[i].activeFocus)focusRole=i+1
        }
        workflow.observeUi({modalStack:diagnosticModalStack,editorKind:currentPage==="rules"?3:currentPage==="categories"?2:editEnabled?1:0,focusRole:focusRole,visibleControls:visibleControls,enabledControls:enabledControls,generation:workflow.diagnosticGeneration,surface:s.active?currentPage:settingUp?"setup":s.unlockRequired?"unlock":"welcome",modals:modals,pending:pending,
          sidebar:sidebarVisible,dirty:dirty||categoryDirty||rulesDirty,search:search.text.length>0,searchValid:diagnosticSearchValid,
          first:first,last:last,delegates:count,selectedVisible:!!ledger.currentItem && ledger.currentItem.y>=ledger.contentY && ledger.currentItem.y<ledger.contentY+ledger.height,
          ready:modals!==0 || (pending===0 && (!s.active || (currentPage!=="ledger" && currentPage!=="review") || observedList.count===0 || first>=0)),nativeExposed:false,
          blockers:(s.importBlockers||[]).length,blockersTotal:s.importBlockerCount||0})
    }
    Timer {interval:100;repeat:true;running:workflow.diagnosticsEnabled;onTriggered:window.publishDiagnostics()}
    property var s: workflow.state
    property bool startupPending: false
    menuBar: MenuBar {
        Menu {
            id:diagnosticFileMenu
            title: "File"
            MenuItem {
                objectName: "fileCloseDataset";text: "Close file"
                enabled: !!s.active && !workflow.busy
                onTriggered: guard(function(){workflow.command("close")})
            }
        }
        Menu {
            title: "Help"
            MenuItem { objectName: "openHelp"; text: "SpentIt Help"; onTriggered: openHelp() }
        }
    }
    Shortcut { sequences: [StandardKey.HelpContents]; context: Qt.ApplicationShortcut; onActivated: openHelp() }
    property var selected: s.selected || ({})
    property bool processingIncomplete: !!s.pending && !!s.pending.processingIncomplete
    // View state belongs to occurrence identities, never recycled delegates.
    property var splitViews: ({})
    function splitKey(record) { return JSON.stringify([record.batch, record.ordinal]) }
    function toggleSplits(record) {
        let next=Object.assign({},splitViews)
        let key=splitKey(record)
        if(next[key]) delete next[key]; else next[key]=true
        splitViews=next
    }
    property var searchReturn: null
    property var pendingSearch: null
    property string appliedFilter: ""
    function transferCommand(action, args) {
        let request=Object.assign({},args)
        request.sequence=s.transferSequence
        request.operation=workflow.newIdentity()
        guard(function(){workflow.command(action,request)})
    }
    function transferDestinations() {return [{id:"",name:"Choose destination explicitly"}].concat((s.accounts||[]).filter(function(a){return a.id!==s.accountId}).map(function(a){return {id:a.id,name:a.name+" · "+a.currency+" · account "+a.id}}))}
    property var pendingCounterpart: null
    property var accountViewports: ({})
    function rememberAccountViewport() {
        if(currentPage!=="ledger" || !s.accountId || ledger.count===0)return
        let first=ledger.indexAt(1,ledger.contentY+1)
        if(first<0)first=0
        let item=ledger.itemAtIndex(first)
        let views=Object.assign({},accountViewports)
        views[s.accountId]={anchor:workflow.rowIdentity(first),index:first,offset:item?ledger.contentY-item.y:0,
            horizontal:ledgerScroll.contentItem.contentX}
        accountViewports=views
    }
    function ledgerTransfer(record, split) {
        let links=(s.transferNavigation||{})[record.batch+":"+record.ordinal]||{}
        return split ? links[split.allocationId]||null : null
    }
    function firstLedgerTransfer(record) {
        for(const split of record.splits) {
            const destination=ledgerTransfer(record,split)
            if(destination)return destination
        }
        return null
    }
    component LedgerAction: Button {
        id: action
        property int textAlignment: Text.AlignHCenter
        hoverEnabled: true
        padding: 4
        contentItem: Label {
            text: action.text; textFormat: Text.PlainText; font: action.font
            color: action.enabled ? theme.text : theme.disabled
            horizontalAlignment: action.textAlignment
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }
        background: Rectangle {
            radius: 3
            color: action.down ? theme.selectionPressed : action.hovered ? theme.selection : "transparent"
            border.width: action.visualFocus ? 1 : 0
            border.color: theme.focus
        }
    }
    component TransferArrow: LedgerAction {
        property var destination: null
        visible: !!destination
        width:28;height:28;padding:2
        text:destination?destination.arrow:""
        enabled:!!destination && !workflow.busy
        Accessible.name:destination?destination.description:""
        ToolTip.visible:hovered
        ToolTip.text:destination?destination.description:""
        onClicked:window.showCounterpart(destination)
    }
    function showCounterpart(item) {
        transferDialog.close()
        guard(function(){
            rememberAccountViewport()
            pendingCounterpart={batch:item.counterpartBatch,ordinal:item.counterpartOrdinal}
            workflow.command("select_account",{id:item.counterpartAccount,reveal:true,batch:item.counterpartBatch,ordinal:item.counterpartOrdinal})
        })
    }
    property var pendingImportIssue: null
    function openImportReview(item) {
        importBlockersDialog.close()
        guard(function(){
            rememberAccountViewport()
            pendingImportIssue=item.record<0?item:null
            pendingReviewInspection=item.record>=0
            reviewInspecting=item.record>=0
            pendingCounterpart=item.record>=0?{batch:item.batch,ordinal:item.record}:null
            workflow.command("select_account",{id:item.account,reveal:item.record>=0,reviewBatch:item.batch,batch:item.batch,ordinal:item.record})
        })
    }
    Dialog {
        id:importBlockersDialog;objectName:"importBlockersDialog";title:"What’s blocking commit?";modal:true
        width:Math.min(parent.width-40,900);height:Math.min(parent.height-40,600);anchors.centerIn:parent
        standardButtons:Dialog.Close
        contentItem:ListView {
            id:blockerList;objectName:"importBlockerList";clip:true;spacing:12
            model:s.importGroup?s.importGroup.blockers:[]
            boundsBehavior:Flickable.StopAtBounds
            header:Label {width:blockerList.width-24;text:"Choose a transaction to fix, or view a possible transfer match.";wrapMode:Text.Wrap;padding:8}
            ScrollBar.vertical:UsableScrollBar {objectName:"importBlockerScrollBar";policy:ScrollBar.AlwaysOn}
            delegate:ColumnLayout {
                id:blockerDelegate
                required property int index
                required property var modelData
                width:blockerList.width-24;spacing:4
                Button {
                    objectName:"importBlocker_"+index
                    Layout.fillWidth:true
                    text:modelData.name+(modelData.record>=0?" · Record "+(modelData.record+1):"")+" · "+modelData.reason
                    contentItem:Label {text:parent.text;wrapMode:Text.Wrap;textFormat:Text.PlainText}
                    onClicked:openImportReview(modelData)
                }
                Label {visible:modelData.candidateCount>0;Layout.fillWidth:true;text:"Possible counterparts ("+modelData.candidateCount+") — inspect before matching:";wrapMode:Text.Wrap}
                Repeater {
                    model:modelData.candidates
                    delegate:Button {
                        required property int index
                        objectName:"importCounterpart_"+blockerDelegate.index+"_"+index
                        required property var modelData
                        Layout.fillWidth:true;Layout.leftMargin:20
                        text:modelData.name+" · Record "+(modelData.record+1)+" · "+modelData.date+" · "+modelData.amount+" "+modelData.currency+" · "+modelData.status
                        contentItem:Label {text:parent.text;wrapMode:Text.Wrap;textFormat:Text.PlainText}
                        onClicked:openImportReview(modelData)
                    }
                }
            }
        }
    }

    function sortedCategories(rows, order) {
        return rows.slice().sort(function(a,b){
            if(!!a.account!==!!b.account)return a.account?1:-1
            let nameOrder=a.path.toLowerCase().localeCompare(b.path.toLowerCase()) || a.id.localeCompare(b.id)
            if(a.account)return nameOrder
            if(order==="added") {
                // Decimal strings preserve the full stored 64-bit revision range.
                return a.createdOrder.length-b.createdOrder.length || a.createdOrder.localeCompare(b.createdOrder) || a.id.localeCompare(b.id)
            }
            return order==="descending"?-nameOrder:nameOrder
        })
    }
    FontMetrics {id:categoryFontMetrics;font:window.font}
    readonly property real categorySelectorWidth: {
        let width=240
        for(let c of (s.categories||[]).concat(s.accountCategories||[]))
            if(!c.deleted)width=Math.max(width,categoryFontMetrics.advanceWidth(c.path+(c.account?" [Account]":""))+64)
        return Math.ceil(width)
    }
    readonly property var ledgerCategoryChoices: [{id:"",path:"Uncategorized"}].concat(sortedCategories((s.categories||[]).concat(s.accountCategories||[]).filter(function(c){return c.assignable || (c.account && !c.archived)})).map(function(c){return c.account?{id:c.id,accountId:c.accountId,path:c.path+" [Account]",account:true}:c}))
    property var pendingLedgerCategory: null
    function applyLedgerCategory(choice, allocation) {
        guard(function(){
            captureLedgerView("category")
            workflow.command(choice.account?"assign_transfer_batch":"release_transfer_batch",{destination:choice.accountId||"",category:choice.id,allocation:allocation||"",sequence:s.transferSequence})
        })
    }
    function chooseLedgerCategory(choice) {
        if(workflow.busy || s.readOnly || !(s.selectionCount>0))return
        const rows=s.selection||[]
        ledgerContext.close()
        if(rows.some(function(row){return (row.splits||[]).length!==1})) {
            pendingLedgerCategory=choice
            bulkCategoryDialog.open()
        } else applyLedgerCategory(choice,rows.length===1?rows[0].splits[0].allocationId:"")
    }
    Dialog {id:bulkCategoryDialog;objectName:"bulkCategoryDialog";title:"Choose split";modal:true;anchors.centerIn:parent;width:Math.min(600,window.width-40);standardButtons:Dialog.Cancel
        readonly property var rows:s.selection||[]
        contentItem:ColumnLayout {
            Label {text:bulkCategoryDialog.rows.length===1?"Choose the split to change.":"Select one split transaction at a time to change its category.";wrapMode:Text.Wrap;Layout.fillWidth:true}
            Repeater {
                model:bulkCategoryDialog.rows.length===1?(bulkCategoryDialog.rows[0].splits||[]):[]
                delegate:Button {
                    required property var modelData
                    required property int index
                    objectName:"ledgerCategorySplit_"+index
                    text:modelData.transferLabel;Layout.fillWidth:true
                    onClicked:{const choice=pendingLedgerCategory;pendingLedgerCategory=null;bulkCategoryDialog.close();applyLedgerCategory(choice,modelData.allocationId)}
                }
            }
        }
    }
    property var selectedEditRequest: null
    property bool restoringSelection: false
    readonly property int selectedEditCount:selectedEditRequest?selectedEditRequest.selected.length:0
    function editSelectedTransactions(restore) {
        const rows=(s.selection||[]).map(function(r){return {batch:r.batch,ordinal:r.ordinal,payee:r.payee}})
        if(!rows.length || workflow.busy || s.readOnly)return
        ledgerContext.close()
        guard(function(){
            restoringSelection=restore
            selectedEditRequest={selected:rows,account:s.accountId,generation:s.ledgerEditGeneration}
            selectedPayee.text=rows.every(r=>r.payee===rows[0].payee)?rows[0].payee:""
            selectedEditDialog.open()
            if(!restore)selectedPayee.forceActiveFocus()
        })
    }
    function applySelectedEdit() {
        if(!selectedEditRequest || workflow.busy)return
        const request=selectedEditRequest
        if(!restoringSelection)request.payee=selectedPayee.text
        captureLedgerView("selected-edit")
        selectedEditDialog.close()
        ledger.forceActiveFocus()
        workflow.command(restoringSelection?"restore_imported_selection":"rename_payee_selection",request)
    }
    Dialog {
        id:selectedEditDialog;objectName:"selectedEditDialog";modal:true;anchors.centerIn:parent;width:Math.min(560,window.width-40)
        title:restoringSelection?"Restore imported values?":"Rename payee"
        contentItem:ColumnLayout {
            Label {Layout.fillWidth:true;wrapMode:Text.Wrap;text:restoringSelection?
                "Restore imported values for "+selectedEditCount+" selected "+(selectedEditCount===1?"transaction":"transactions")+"? This replaces their current dates, amounts, payees, memos, and splits/categories.":
                "Enter a payee name for "+selectedEditCount+" selected "+(selectedEditCount===1?"transaction.":"transactions.")}
            NavigableTextField {id:selectedPayee;objectName:"selectedPayee";visible:!restoringSelection;Layout.fillWidth:true;Accessible.name:"New payee name";onAccepted:applySelectedEdit()}
        }
        footer:DialogButtonBox {
            Button {objectName:"cancelSelectedEdit";text:"Cancel";DialogButtonBox.buttonRole:DialogButtonBox.RejectRole;onClicked:selectedEditDialog.close()}
            Button {objectName:"confirmSelectedEdit";text:restoringSelection?"Restore":"Rename";enabled:!workflow.busy;DialogButtonBox.buttonRole:DialogButtonBox.AcceptRole;onClicked:applySelectedEdit()}
        }
    }
    Menu {
        id:ledgerContext;objectName:"ledgerContext";popupType:Popup.Item;cascade:true
        delegate:MenuItem {hoverEnabled:true}
        MenuItem {objectName:"addTransactionContext";visible:!window.reportLedger;text:"Add transaction…";enabled:!s.readOnly && !workflow.busy;onTriggered:addTransaction()}
        MenuItem {objectName:"renamePayeeSelection";text:"Rename payee…";enabled:!s.readOnly && !workflow.busy && (s.selectionCount||0)>0;onTriggered:editSelectedTransactions(false)}
        MenuItem {objectName:"restoreImportedSelection";text:"Restore imported values…";enabled:!s.readOnly && !workflow.busy && (s.selectionCount||0)>0;onTriggered:editSelectedTransactions(true)}
        MenuItem {objectName:"duplicateMergeSelection";visible:!window.reportLedger;text:"Merge";enabled:!s.readOnly && !workflow.busy && (s.selectionCount||0)>=2;onTriggered:guard(function(){answerReview({key:"selected"},"merge",s.selection||[])})}
        MenuItem {objectName:"mergeSelection";visible:!window.reportLedger;text:"Link";enabled:!s.readOnly && !!s.transfersSupported && !workflow.busy && (s.selectionCount||0)>0;onTriggered:mergeSelected()}
        MenuItem {objectName:"deleteSelectedContext";text:"Delete selected…";enabled:!s.readOnly && !workflow.busy && (s.selectionCount||0)>0;onTriggered:removeSelected()}
        Menu {
            id:ledgerCategoryMenu;objectName:"ledgerCategoryMenu";title:"Change category";popupType:Popup.Item;cascade:true
            enabled:!s.readOnly && !!s.transfersSupported && !workflow.busy && (s.selectionCount||0)>0
            width:Math.min(categorySelectorWidth,window.width-40);height:Math.min(420,contentItem.contentHeight+topPadding+bottomPadding)
            contentItem:ListView {
                implicitHeight:contentHeight;model:ledgerCategoryMenu.contentModel;clip:true
                currentIndex:ledgerCategoryMenu.currentIndex;boundsBehavior:Flickable.StopAtBounds
                ScrollBar.vertical:UsableScrollBar {objectName:"ledgerCategoryScrollBar"}
            }
            Instantiator {
                model:ledgerCategoryChoices
                delegate:MenuItem {
                    required property var modelData
                    required property int index
                    objectName:"ledgerCategoryChoice_"+index
                    text:modelData.path.replace(/&/g,"&&")
                    enabled:!modelData.account || modelData.accountId!==s.accountId
                    onTriggered:chooseLedgerCategory(modelData)
                }
                onObjectAdded:(index,object)=>ledgerCategoryMenu.insertItem(index,object)
                onObjectRemoved:(index,object)=>ledgerCategoryMenu.removeItem(object)
            }
        }
    }
    function restoreViewport(saved) {
        if(!saved || ledger.count===0)return
        ledger.forceLayout()
        let index=workflow.occurrenceIndex(saved.anchor.batch || "",saved.anchor.ordinal===undefined?-1:saved.anchor.ordinal)
        if(index<0)index=Math.max(0,Math.min(saved.index||0,ledger.count-1))
        ledger.positionViewAtIndex(index,ListView.Beginning)
        ledger.forceLayout()
        let item=ledger.itemAtIndex(index)
        if(item)ledger.contentY=Math.max(ledger.originY,Math.min(item.y+(saved.offset||0),ledger.originY+Math.max(0,ledger.contentHeight-ledger.height)))
        if(saved.horizontal!==undefined)ledgerScroll.contentItem.contentX=Math.max(0,Math.min(saved.horizontal,ledgerScroll.contentWidth-ledgerScroll.availableWidth))
    }
    property int accountViewportRestoreSerial: 0
    function restoreAccountViewport() {
        const account=s.accountId, saved=accountViewports[account], serial=++accountViewportRestoreSerial
        if(!saved)return
        Qt.callLater(function(){
            if(serial!==accountViewportRestoreSerial || currentPage!=="ledger" || s.accountId!==account)return
            restoreViewport(saved)
            // Materializing the anchored row can attach the open inline editor,
            // changing its height. Restore once more after that layout settles.
            Qt.callLater(function(){
                if(serial===accountViewportRestoreSerial && currentPage==="ledger" && s.accountId===account)restoreViewport(saved)
            })
        })
    }
    // Viewport restoration is session-only.
    property var ledgerViewRestore: null
    property string ledgerViewKind: ""
    readonly property var ledgerColumnWeights: reportLedger?[0.12,0.20,0.18,0.16,0.12,0.10,0.12]:[0.14, 0.24, 0.17, 0.15, 0.14, 0.16]
    readonly property int ledgerCategoryColumn: reportLedger?3:2
    function sidebarBalanceText(account) {
        return "Balance "+(account.currency||"")+" "+(account.balance||"").replace(/ /g,"\u00a0")
    }
    function captureLedgerView(kind) {
        if(ledger.count===0) {ledgerViewRestore=null;ledgerViewKind="";return}
        let first=ledger.indexAt(1,ledger.contentY+1)
        if(first<0) first=0
        let item=ledger.itemAtIndex(first)
        ledgerViewRestore={anchor:workflow.rowIdentity(first),index:first,offset:item?ledger.contentY-item.y:0,horizontal:ledgerScroll.contentItem.contentX}
        ledgerViewKind=kind
    }
    function guardedPreview(action, args) {
        guard(function(){
            captureLedgerView(action==="preview_bulk"?"bulk":action==="rules_reapply_preview"?"reapply":"change")
            workflow.command(action,args||{})
        })
    }
    function settleLedgerView(action, success) {
        if(ledgerViewRestore===null) return
        let related=false
        let restore=false
        let clear=false
        if(ledgerViewKind==="selected-edit") {
            related=action==="delete_selection" || action==="rename_payee_selection" || action==="restore_imported_selection" || action==="undo_edit"
            restore=related;clear=true
        } else if(ledgerViewKind==="category") {
            related=action==="assign_transfer_batch" || action==="release_transfer_batch" || action==="undo_transfer_assignment"
            restore=related;clear=true
        } else if(ledgerViewKind==="bulk") {
            related=action==="preview_bulk" || action==="apply_bulk" || action==="cancel_bulk"
            restore=success && related
            clear=!success || action==="apply_bulk" || action==="cancel_bulk" || !related
        } else if(ledgerViewKind==="reapply") {
            related=action==="rules_reapply_preview" || action==="rules_reapply_apply" || action==="cancel_reapply"
            restore=success && related
            clear=!success || action==="rules_reapply_apply" || action==="cancel_reapply" || !related
        } else if(ledgerViewKind==="change") {
            related=action==="preview_change" || action==="apply_change" || action==="cancel_change"
            restore=success && action==="apply_change"
            clear=!success || action==="apply_change" || action==="cancel_change" || !related
        } else clear=true
        if(!related && !restore) {ledgerViewRestore=null;ledgerViewKind="";return}
        if(restore) {
            let saved=ledgerViewRestore
            if(clear) {ledgerViewRestore=null;ledgerViewKind=""}
            Qt.callLater(function(){restoreViewport(saved);Qt.callLater(function(){restoreViewport(saved)})})
            return
        }
        if(clear) {ledgerViewRestore=null;ledgerViewKind=""}
    }
    // Navigation is local to this document; pages stay alive to retain ledger view state.
    readonly property bool reportLedger: currentPage==="reports" && reportsWorkspace.mode==="detail"
    property var pendingReportNavigation:null
    property string currentPage: "ledger"
    property var navigation: ["ledger"]
    property int navigationIndex: 0
    property var navigationAccounts: [""]
    property var pendingAccountNavigation: null
    property bool addingAccount: false
    property string accountCreationId: ""
    property string accountCreationDate: ""
    property bool sidebarVisible: true
    function revealAccountEntry() {
        const scroll=accountScroll.contentItem
        if(!scroll)return
        const minimum=scroll.originY
        const maximum=minimum+Math.max(0,scroll.contentHeight-scroll.height)
        let position=scroll.contentY
        if(addingAccount) {
            const top=newAccountName.mapToItem(scroll.contentItem,0,0).y
            if(top<position)position=top
            else if(top+newAccountName.height>position+scroll.height)
                position=top+newAccountName.height-scroll.height
        }
        scroll.contentY=Math.max(minimum,Math.min(maximum,position))
    }
    Connections {
        target:accountScroll.contentItem
        function onContentHeightChanged(){Qt.callLater(window.revealAccountEntry)}
        function onHeightChanged(){Qt.callLater(window.revealAccountEntry)}
    }
    // Drag state stays outside delegates so a completed reorder can replace them safely.
    property var accountDragRow: null
    property point accountDragPointer: Qt.point(0,0)
    property point accountDragOffset: Qt.point(0,0)
    property int accountDropIndex: -1
    property point accountDropPoint: Qt.point(0,0)
    function cancelAccountDrag() {accountDragRow=null;accountDropIndex=-1}
    function moveAccountDrag(point) {
        accountDragPointer=point
        accountDropIndex=-1
        if(!accountDragRow)return
        const viewport=accountScroll.mapFromItem(window.contentItem,point.x,point.y)
        if(viewport.x<0 || viewport.x>=accountScroll.availableWidth || viewport.y<0 || viewport.y>accountScroll.height)return
        const local=accountRows.mapFromItem(window.contentItem,point.x,point.y)
        const last=accountRepeater.itemAt(accountRepeater.count-1)
        if(!last || local.y<0 || local.y>last.y+last.height+6)return
        for(let i=0;i<accountRepeater.count;++i){
            const row=accountRepeater.itemAt(i)
            if(local.y<row.y+row.height/2){accountDropIndex=i;accountDropPoint=accountRows.mapToItem(window.contentItem,0,row.y);return}
        }
        accountDropIndex=accountRepeater.count;accountDropPoint=accountRows.mapToItem(window.contentItem,0,last.y+last.height)
    }
    function finishAccountDrag() {
        if(!accountDragRow)return
        const id=accountDragRow.modelData.id
        const slot=accountDropIndex
        cancelAccountDrag()
        if(slot<0)return
        let ids=(s.sidebarAccounts||s.accounts||[]).map(row=>row.id)
        const from=ids.indexOf(id)
        const to=slot>from?slot-1:slot
        if(from<0 || from===to)return
        ids.splice(from,1);ids.splice(to,0,id)
        workflow.reorderAccounts(ids)
    }
    Shortcut {sequence:"Escape";enabled:accountDragRow!==null;onActivated:cancelAccountDrag()}
    onActiveChanged:if(!active)cancelAccountDrag()
    Timer {
        interval:30;repeat:true;running:accountDragRow!==null
        onTriggered:{
            const point=accountScroll.mapFromItem(window.contentItem,accountDragPointer.x,accountDragPointer.y)
            if(point.x<0 || point.x>=accountScroll.availableWidth || point.y<0 || point.y>accountScroll.height)return
            const scroll=accountScroll.contentItem
            const delta=point.y<28?-10:point.y>accountScroll.height-28?10:0
            if(delta && scroll && scroll.contentHeight>scroll.height){
                scroll.contentY=Math.max(0,Math.min(scroll.contentHeight-scroll.height,scroll.contentY+delta))
                moveAccountDrag(accountDragPointer)
            }
        }
    }
    Rectangle {
        objectName:"accountDragPreview";parent:window.contentItem;z:1000
        visible:accountDragRow!==null;color:window.palette.window;border.color:theme.border;radius:3
        x:accountDragPointer.x-accountDragOffset.x+12;y:accountDragPointer.y-accountDragOffset.y
        width:accountDragRow?accountDragRow.width:0;height:accountDragRow?accountDragRow.height:0
        opacity:0.9
        ShaderEffectSource {anchors.fill:parent;sourceItem:accountDragRow;live:true}
    }
    function resetNavigation() {
        ++accountViewportRestoreSerial;accountViewports={};pendingCounterpart=null;pendingMerge=null;mergeHint="";currentPage="ledger"; navigation=["ledger"]; navigationAccounts=[s.accountId||""]; navigationIndex=0;pendingAccountNavigation=null
    }
    function navigate(page) {
        if(workflow.busy || !s.active || page===currentPage) return
        guard(function(){
            if(s.reportLedgerActive){pendingReportNavigation=function(){navigate(page)};workflow.command("report_leave");return}
            rememberAccountViewport()
            let entries=navigation.slice(0,navigationIndex+1)
            entries.push(page);navigation=entries;let accounts=navigationAccounts.slice(0,navigationIndex+1);accounts.push(s.accountId);navigationAccounts=accounts;navigationIndex=entries.length-1;currentPage=page
            if(page==="ledger")restoreAccountViewport()
        })
    }
    function chooseAccount(id) {
        if(workflow.busy) return
        if(id===s.accountId) {navigate("ledger");return}
        guard(function(){rememberAccountViewport();workflow.command("select_account",{id:id})})
    }
    function editOpening() {
        if(workflow.busy || s.readOnly || !s.accountSelected)return
        guard(function(){correctDate.text=s.openingDate||"";correctAmount.text=s.opening||"";correctOpeningDialog.open()})
    }
    function acceptDialog(dialog) {
        const ok=dialog.standardButton(Dialog.Ok)
        if(dialog.opened && !workflow.busy && ok && ok.enabled) dialog.accept()
    }
    property var pendingAccountMenu: null
    function openAccountMenu(id, item, x, y) {
        if(workflow.busy) return
        const point=item.mapToItem(window.contentItem,x,y)
        guard(function(){
            if(id===s.accountId) { accountContext.popup(point.x,point.y); return }
            pendingAccountMenu={id:id,x:point.x,y:point.y}
            rememberAccountViewport()
            workflow.command("select_account",{id:id})
        })
    }
    Menu {
        id:accountContext;objectName:"accountContextMenu"
        MenuItem {objectName:"accountImport";text:"Import…";enabled:!s.readOnly && !workflow.busy;onTriggered:{importTarget=s.accountId;importDialog.open()}}
        MenuItem {objectName:"undoLastImport";text:"Undo last import…";enabled:!s.readOnly && !workflow.busy && accountImports(s.accountId||"").length>0;onTriggered:guard(function(){let items=accountImports(s.accountId||"");if(items.length)requestImportUndo(items[0])})}
        MenuItem {objectName:"discardImport";text:"Discard new transactions…";enabled:!s.readOnly && !workflow.busy;onTriggered:guard(function(){discardDialog.open()})}
        parent:window.contentItem
        MenuItem {height:visible?implicitHeight:0;objectName:"renameAccount";text:"Rename…";visible:!!s.accountSelected;enabled:!workflow.busy;onTriggered:guard(function(){renameField.text=s.name||"";renameDialog.open()})}
        MenuItem {height:visible?implicitHeight:0;objectName:"archiveAccount";text:"Archive";visible:!!s.accountSelected && !s.archived;enabled:!workflow.busy;onTriggered:guard(function(){workflow.command("archive_account",{operation:workflow.newIdentity()})})}
        MenuItem {height:visible?implicitHeight:0;objectName:"reactivateAccount";text:"Reactivate";visible:!!s.archived;enabled:!workflow.busy;onTriggered:guard(function(){workflow.command("reactivate_account",{operation:workflow.newIdentity()})})}
        MenuItem {height:visible?implicitHeight:0;objectName:"deleteAccount";text:"Delete…";visible:!!s.accountSelected;enabled:!workflow.busy;onTriggered:guard(function(){deleteAccountDialog.open()})}
        MenuItem {height:visible?implicitHeight:0;objectName:"correctOpening";text:"Correct opening…";visible:!!s.accountSelected && !s.readOnly;enabled:!workflow.busy;onTriggered:editOpening()}
        MenuItem {height:visible?implicitHeight:0;objectName:"reassignCurrency";text:"Reassign currency…";visible:!!s.accountSelected && !s.readOnly;enabled:!workflow.busy;onTriggered:guard(function(){currencySearch.text="";currencyDialog.open()})}
        MenuItem {height:visible?implicitHeight:0;objectName:"undoAccountCommand";text:"Undo opening or currency…";visible:!!s.undoableAccountOperation;enabled:!workflow.busy;onTriggered:guard(function(){workflow.command("undo_account",{operation:workflow.newIdentity(),undoOf:s.undoableAccountOperation})})}
    }
    function revealHistoryOperation(operation) {
        let entries=s.entryHistory||[]
        for(let i=0;i<entries.length;i++) if(entries[i].operation===operation) {
            entryHistoryList.positionViewAtIndex(i,ListView.Contain);return
        }
    }
    function traverseNavigation(delta) {
        if(currentPage==="reports" && reportsWorkspace.mode!=="workspace" && delta<0){reportsWorkspace.back();return}
        let destination=navigationIndex+delta
        if(workflow.busy || !s.active || destination<0 || destination>=navigation.length) return
        guard(function(){
            if(s.reportLedgerActive){pendingReportNavigation=function(){traverseNavigation(delta)};workflow.command("report_leave");return}
            rememberAccountViewport()
            if(navigationAccounts[destination]!==s.accountId) {
                rememberAccountViewport()
                pendingAccountNavigation={index:destination,page:navigation[destination]}
                workflow.command("select_account",{id:navigationAccounts[destination]})
            } else {navigationIndex=destination;currentPage=navigation[destination];if(currentPage==="ledger")restoreAccountViewport()}
        })
    }
    Shortcut {sequence:"Ctrl+Shift+B";enabled:!!s.active;onActivated:sidebarVisible=!sidebarVisible}
    Shortcut {objectName:"duplicateMergeShortcut";sequence:"Alt+M";enabled:!window.reportLedger && !!s.active && !workflow.busy && !s.readOnly && (s.selectionCount||0)>=2;onActivated:guard(function(){answerReview({key:"selected"},"merge",s.selection||[])})}
    Shortcut {objectName:"mergeShortcut";sequence:"Alt+L";enabled:!window.reportLedger && !!s.active && !workflow.busy && !s.readOnly && !!s.transfersSupported;onActivated:mergeSelected()}
    Shortcut {sequences:[StandardKey.Back];enabled:!!s.active && !workflow.busy;onActivated:traverseNavigation(-1)}
    Shortcut {sequences:[StandardKey.Forward];enabled:!!s.active && !workflow.busy;onActivated:traverseNavigation(1)}
    property bool restoringImportedText: false
    property bool payeeTouched: false
    property bool memoTouched: false
    property bool categoryDirty: false
    property bool rulesDirty: false
    property bool rulePreviewSingle: false
    property string rulePreviewKey: ""
    property string rulePendingPreviewKey: ""
    property bool currentRulePreview: !!rulePreviewKey && rulePreviewKey===rulesPreviewKey()
    property bool ruleApplyAfterSave: false
    property bool ruleApplyAfterPreview: false
    function rulesPreviewKey() {return JSON.stringify([rulesImage(),rulePreviewSingle?ruleIndex:-1,ruleTransactionScope.currentIndex,false])}
    property int ruleEditGeneration: 0
    property var ruleBaseline: []
    property var ruleMovedIds: ({})
    property string ruleSavingId: ""
    property bool ruleSavingAll: false
    property bool rulesLeaving: false
    property var helpWindowInstance: null
    property var helpReturnItem: null
    property string helpLoadError: ""
    property bool helpOpening: false
    property var ruleValidationQueue: []
    property bool ruleValidatingAll: false
    property int unsavedRuleCount: {let generation=ruleEditGeneration;return dirtyRuleIndices().length}
    function ruleImage(r) {return {id:r.id,label:r.label||"",enabled:!!r.ruleOn,scope:r.scope||"transaction",input:r.input||"payee",expression:r.expression||"",payeeTemplate:r.payeeTemplate||"",memoTemplate:r.memoTemplate||"",categoryId:r.categoryId||"",inheritParent:!!r.inheritParent}}
    function savedRule(id) {return ruleBaseline.find(r=>r.id===id)}
    function newRuleIndex() {for(let i=0;i<ruleModel.count;i++)if(!savedRule(ruleModel.get(i).id))return i;return -1}
    function ruleChanged(index) {
        if(index<0 || index>=ruleModel.count)return false
        let row=ruleModel.get(index), before=savedRule(row.id)
        if(!before || row.pendingDelete || JSON.stringify(ruleImage(row))!==JSON.stringify(before))return true
        if(!ruleMovedIds[row.id])return false
        let current=[]
        for(let i=0;i<ruleModel.count;i++)if(savedRule(ruleModel.get(i).id))current.push(ruleModel.get(i).id)
        return current.indexOf(row.id)!==ruleBaseline.findIndex(r=>r.id===row.id)
    }
    function dirtyRuleIndices() {let out=[];for(let i=0;i<ruleModel.count;i++)if(ruleChanged(i))out.push(i);return out}
    function updateRulesDirty() {ruleEditGeneration++;rulesDirty=dirtyRuleIndices().length>0;rulePreviewKey=""}
    // The editor and a newly appended delegate change the scroll extent during
    // layout. Restore after polish, then once more after the new extent settles.
    property int ruleRevealPasses: 0
    Timer {
        id:ruleRevealTimer;interval:16
        onTriggered:{
            if(currentPage!=="rules" || ruleIndex<0)return
            let y=ruleEditor.mapToItem(rulesPage.contentItem.contentItem,0,0).y
            rulesPage.contentItem.contentY=Math.max(0,Math.min(y-60,rulesPage.contentItem.contentHeight-rulesPage.availableHeight))
            if(--ruleRevealPasses>0)restart()
        }
    }
    function revealRuleEditor() {ruleRevealPasses=2;ruleRevealTimer.restart()}
    function helpCatalogObject() {
        try { return helpCatalog } catch (error) { return null }
    }
    function helpOwner() {
        if (rulesLeaving) return rulesLeaveDialog
        if (reportsWorkspace.leaving) return reportsWorkspace.leaveWindow
        return null
    }
    // Release Help before the dialog hides. A transient child of a closing
    // dialog can close with it. The dialog stays application-modal.
    function setRulesLeaving(value) {
        if (!value && rulesLeaving && helpWindowInstance && helpWindowInstance.modalOwner === rulesLeaveDialog)
            helpWindowInstance.modalOwner = null
        rulesLeaving = value
        if (value && helpWindowInstance) helpWindowInstance.modalOwner = rulesLeaveDialog
    }
    function restoreHelpFocus() {
        const owner = helpOwner() || window
        const item = helpReturnItem
        Qt.callLater(function() {
            if (owner && owner.requestActivate) owner.requestActivate()
            if (item && item.forceActiveFocus) item.forceActiveFocus()
        })
    }
    function presentHelp() {
        const owner = helpOwner() || window
        helpReturnItem = owner.activeFocusItem
        helpWindowInstance.modalOwner = helpOwner()
        helpWindowInstance.syncOwner()
        helpWindowInstance.show()
        helpWindowInstance.raise()
        helpWindowInstance.requestActivate()
    }
    function openHelp() {
        const catalog = helpCatalogObject()
        if (!catalog || !catalog.loaded) { helpLoadError = "Help could not be opened."; return }
        helpLoadError = ""
        if (helpWindowInstance) { presentHelp(); return }
        if (helpOpening) return
        helpOpening = true
        const component = Qt.createComponent("qrc:/spentit/HelpWindow.qml")
        if (component.status !== Component.Ready) {
            helpOpening = false
            helpLoadError = "Help could not be opened."
            console.warn(component.errorString())
            return
        }
        helpWindowInstance = component.createObject(null, {catalog: catalog, mainWindow: window})
        helpOpening = false
        if (!helpWindowInstance) {
            helpLoadError = "Help could not be opened."
            console.warn(component.errorString())
            return
        }
        helpWindowInstance.helpClosed.connect(restoreHelpFocus)
        presentHelp()
    }
    function showRulesLeave() {
        showRulesPage()
        rulesLeaveDialog.x=window.x+(window.width-rulesLeaveDialog.width)/2
        rulesLeaveDialog.y=window.y+(window.height-rulesLeaveDialog.height)/2
        setRulesLeaving(true)
        rulesLeaveDialog.requestActivate()
    }
    function stayInRules() {setRulesLeaving(false);pendingAction=null}
    Connections {
        target: reportsWorkspace
        function onLeaveShown() { if (helpWindowInstance) helpWindowInstance.modalOwner = reportsWorkspace.leaveWindow }
        function onLeaveHiding() { if (helpWindowInstance && helpWindowInstance.modalOwner === reportsWorkspace.leaveWindow) helpWindowInstance.modalOwner = null }
        function onHelpRequested() { openHelp() }
    }
    Window {
        id:rulesLeaveDialog;objectName:"rulesLeaveDialog"
        transientParent:window
        flags:Qt.Dialog | Qt.WindowTitleHint | Qt.WindowSystemMenuHint | Qt.WindowCloseButtonHint
        // WindowModal becomes an attached sheet on macOS, without a draggable title bar.
        modality:Qt.ApplicationModal
        title:"Save changes before leaving Rules?"
        width:460;height:rulesLeaveContent.implicitHeight+40
        minimumWidth:460;maximumWidth:460
        minimumHeight:rulesLeaveContent.implicitHeight+40
        maximumHeight:rulesLeaveContent.implicitHeight+40
        color:window.palette.window
        visible:rulesLeaving
        onClosing:function(close) {
            // Quit also closes hidden windows with native handles. Only an active
            // confirmation may veto closure; a dismissed dialog must allow exit.
            if(!rulesLeaving || allowClose)return
            close.accepted=false
            if(!workflow.busy && !ruleValidatingAll)stayInRules()
        }
        Shortcut {sequence:"Escape";enabled:rulesLeaving && !workflow.busy && !ruleValidatingAll;onActivated:stayInRules()}
        Shortcut {sequences:[StandardKey.HelpContents];context:Qt.WindowShortcut;enabled:rulesLeaving;onActivated:openHelp()}
        ColumnLayout {
            id:rulesLeaveContent;objectName:"rulesLeaveContent"
            anchors.top:parent.top;anchors.left:parent.left;anchors.right:parent.right
            anchors.margins:20;spacing:16
            Label {
                Layout.fillWidth:true;wrapMode:Text.Wrap
                text:unsavedRuleCount===1?"You have unsaved changes to 1 rule.":"You have unsaved changes to "+unsavedRuleCount+" rules."
            }
            RowLayout {
                Layout.alignment:Qt.AlignRight
                enabled:!workflow.busy && !ruleValidatingAll
                Button {objectName:"rulesGuardSave";text:"Save";onClicked:saveAllRulesAndLeave()}
                Button {objectName:"rulesGuardDiscard";text:"Discard";onClicked:workflow.command("rules_discard")}
                Button {objectName:"rulesGuardStay";text:"Stay";onClicked:stayInRules()}
            }
        }
    }
    function discardRule() {
        if(ruleIndex<0)return
        let row=ruleModel.get(ruleIndex),before=savedRule(row.id),index=ruleIndex
        if(before && !ruleMovedIds[row.id])ruleModel.set(index,ruleRow(before))
        else if(before) {
            ruleModel.remove(index)
            let after=ruleBaseline.slice(ruleBaseline.findIndex(r=>r.id===before.id)+1).map(r=>r.id)
            index=ruleModel.count
            for(let i=0;i<ruleModel.count;i++)if(after.indexOf(ruleModel.get(i).id)>=0 || !savedRule(ruleModel.get(i).id)){index=i;break}
            ruleModel.insert(index,ruleRow(before))
        }
        else {ruleModel.remove(index);index=Math.min(index,ruleModel.count-1)}
        openRule(index);updateRulesDirty()
    }
    function saveAllRulesAndLeave() {
        if(!acceptRuleCategory()){stayInRules();revealRuleEditor();return}
        ruleValidationQueue=dirtyRuleIndices().filter(i=>!ruleModel.get(i).pendingDelete).map(i=>ruleModel.get(i).id)
        ruleValidatingAll=true;validateNextRule()
    }
    function validateNextRule() {
        if(!ruleValidationQueue.length) {
            ruleValidatingAll=false;ruleSavingAll=true;ruleSavingId=""
            workflow.command("rules_save",{rules:rulesImage()});return
        }
        let id=ruleValidationQueue.shift()
        for(let i=0;i<ruleModel.count;i++)if(ruleModel.get(i).id===id){openRule(i);break}
        previewThisRule()
    }
    property int ruleIndex: -1
    property string ruleCategoryId: ""
    property bool guardHandoff: false
    property bool ruleSync: false
    property string categoryAction: ""
    property string categoryFormId: ""
    property string categoryFormRevision: ""
    property int pickerIndex: -1
    property string pickerFeedback: ""
    property string categoryEditText: ""
    property bool categorySaving: false
    property var categoryMenuEntry: ({})
    property string categorySelection: ""
    property string categoryReveal: ""
    property string categorySort: "ascending"
    property var categoryVisitAdditions: []
    function categoryRows() {
        let rows=(s.categories||[]).concat(s.accountCategories||[]).filter(function(c){return !c.deleted && ((categoryAction==="rename" && c.id===categoryFormId) || ((showArchivedCategories.checked || !c.archived) && (c.id===categoryReveal || (c.path+" "+(c.aliases||"")).toLowerCase().indexOf(catalogSearch.text.toLowerCase())>=0)))})
        rows=sortedCategories(rows,categorySort)
        let additions=categoryVisitAdditions
        rows=rows.filter(c=>additions.indexOf(c.id)<0).concat(additions.map(id=>rows.find(c=>c.id===id)).filter(c=>!!c))
        return rows.concat(categoryAction==="create"?[{id:"",path:"",pending:true}]:[])
    }
    function revealCategorySelection() {
        Qt.callLater(function(){
            for(let i=0;i<catalogList.count;i++)if(catalogList.model[i].id===categorySelection) {
                catalogList.currentIndex=i;catalogList.positionViewAtIndex(i,ListView.Contain);break
            }
        })
    }
    function sortCategories(order) {
        guard(function(){categoryVisitAdditions=[];categorySort=order;revealCategorySelection()})
    }
    function loadCategoryForm() {
        categoryAction="";categoryDirty=false;categorySaving=false
    }
    function focusCategoryEditor() {
        Qt.callLater(function(){
            for(let i=0;i<catalogList.count;i++) {
                let c=catalogList.model[i]
                if((categoryAction==="create" && c.pending) || (categoryAction==="rename" && c.id===categoryFormId)) {
                    catalogList.positionViewAtIndex(i,ListView.Contain)
                    Qt.callLater(function(){let row=catalogList.itemAtIndex(i);if(row)row.focusEditor()})
                    break
                }
            }
        })
    }
    function newCategory() {
        categoryFormId="";categoryFormRevision=s.catalogRevision||"";categoryEditText="";categoryAction="create";categoryDirty=true
        focusCategoryEditor()
    }
    function renameCategory(c) {
        categoryFormId=c.id;categoryFormRevision=s.catalogRevision||"";categoryEditText=c.name;categoryAction="rename";categoryDirty=true
        focusCategoryEditor()
    }
    function cancelCategoryEdit() {
        loadCategoryForm();pendingAction=null
    }
    function finishCategoryEdit() {
        if(!categoryDirty || categorySaving)return
        if(categoryAction==="create" && categoryEditText.length===0) {
            let next=pendingAction;pendingAction=null;loadCategoryForm();if(next)guard(next)
            return
        }
        saveCategoryForm()
    }
    function saveCategoryForm(action) {
        if(categorySaving || workflow.busy)return
        categorySaving=true
        workflow.command("preview_category",{action:action||categoryAction,id:categoryFormId,revision:categoryFormRevision,
            name:categoryEditText})
    }
    function categoryMenuAction(action) {
        let c=categoryMenuEntry
        guard(function(){
            if(action==="rename")renameCategory(c)
            else {categoryFormId=c.id;categoryFormRevision=s.catalogRevision||"";categoryEditText=c.name;saveCategoryForm(action)}
        })
    }
    function openCategoryPicker(index) {
        pickerIndex=index;let row=splitModel.get(index);unmappedCategory.text=row.category;categorySearch.clear();categoryPicker.open()
    }
    property var pendingMerge: null
    property string mergeHint: ""
    function originLink(allocationId) {
        let origins=selected.transferOrigins||[]
        for(let i=0;i<origins.length;i++) if(origins[i].allocation===allocationId) return origins[i].link||""
        return ""
    }
    function allocationChoice(batch, ordinal) {
        let allocation=""
        if(selected.batch===batch && selected.ordinal===ordinal && transferAllocation.currentIndex>=0) {
            let row=transferAllocation.model[transferAllocation.currentIndex]
            if(row && row.allocationId) allocation=row.allocationId
        }
        return {account:s.accountId,batch:batch,ordinal:ordinal,allocation:allocation}
    }
    function mergeFields(choice, other) {
        return {leftAccount:choice.account,leftBatch:choice.batch,leftOrdinal:choice.ordinal,leftAllocation:choice.allocation||"",
            rightAccount:other.account,rightBatch:other.batch,rightOrdinal:other.ordinal,rightAllocation:other.allocation||""}
    }
    function mergeSelected() {
        if(workflow.busy || !s.active || s.readOnly || !s.transfersSupported) return
        let rows=s.selection||[]
        if(rows.length===2) {
            pendingMerge=null;mergeHint=""
            transferCommand("transfer_merge",mergeFields(allocationChoice(rows[0].batch,rows[0].ordinal),allocationChoice(rows[1].batch,rows[1].ordinal)))
            return
        }
        if(rows.length===1 && pendingMerge) {
            let saved=pendingMerge
            pendingMerge=null;mergeHint=""
            transferCommand("transfer_merge",mergeFields(saved,allocationChoice(rows[0].batch,rows[0].ordinal)))
            return
        }
        if(rows.length===1) {
            pendingMerge=allocationChoice(rows[0].batch,rows[0].ordinal)
            mergeHint="Choose the other split, then select Link again"
            return
        }
        mergeHint="Select one or two splits, then select Link"
    }
    function removeSelected() {
        let rows=s.selection||[]
        if(rows.length===1 && rows[0].batch && rows[0].batch.indexOf("~transfer-")===0)
            transferCommand("transfer_clear_row",{batch:rows[0].batch,ordinal:rows[0].ordinal})
        else if(rows.length && rows.every(r=>r.committed))guardedPreview("preview_bulk",{kind:"remove"})
        else guard(function(){
            if(!(s.selectionCount>0))return
            deleteSelectionRequest={selected:(s.selection||[]).map(r=>({batch:r.batch,ordinal:r.ordinal})),account:s.accountId,generation:s.ledgerEditGeneration}
            ledgerContext.close();deleteSelectionDialog.open()
        })
    }
    property var deleteSelectionRequest:null
    Dialog {
        id:deleteSelectionDialog;objectName:"deleteSelectionDialog";title:"Delete selected transactions?";modal:true;anchors.centerIn:parent;width:Math.min(520,window.width-40)
        contentItem:Label {wrapMode:Text.Wrap;text:deleteSelectionRequest && deleteSelectionRequest.selected.length===1?"Delete this transaction? This changes the account balance.":"Delete these "+(deleteSelectionRequest?deleteSelectionRequest.selected.length:0)+" transactions? This changes the account balance."}
        footer:DialogButtonBox {
            Button {text:"Cancel";onClicked:deleteSelectionDialog.close()}
            Button {objectName:"confirmDeleteSelection";text:"Delete";enabled:!workflow.busy;onClicked:{captureLedgerView("selected-edit");deleteSelectionDialog.close();workflow.command("delete_selection",deleteSelectionRequest)}}
        }
    }
    function accountForName(name) {
        let matches=(s.accountAliases||[]).filter(function(a){return workflow.accountNameMatches(a.name,name)})
        if(!matches.length)return ""
        if(matches.some(function(a){return a.accountId!==matches[0].accountId}))return "ambiguous"
        return matches[0].accountId
    }
    function chooseCategory(id,path) {
        let account=accountForName(path)
        if(account) {
            if(account==="ambiguous"){pickerFeedback="This alias identifies more than one account. Review the account mapping.";return}
            if(pickerIndex===-2){pickerFeedback="Rules cannot create account transfers. Use the transfer workflow.";return}
            if(account===s.accountId){pickerFeedback="A transfer must go to a different account.";return}
            chooseAccountDestination(account);return
        }
        if(pickerIndex===-2) { ruleCategoryId=id||""; writeRule(); categoryPicker.close(); return }
        if(pickerIndex<0 || pickerIndex>=splitModel.count)return
        let split=splitModel.get(pickerIndex)
        let link=originLink(split.allocationId)
        if(link && !committedEditing) {
            categoryPicker.close()
            transferCommand("transfer_retarget",{link:link,batch:selected.batch,ordinal:selected.ordinal,allocation:split.allocationId,mode:0,categoryId:id||"",categoryPath:path||""})
            return
        }
        splitModel.setProperty(pickerIndex,"category",path)
        splitModel.setProperty(pickerIndex,"categoryId",id)
        splitModel.setProperty(pickerIndex,"bindingRevision",id?(s.catalogRevision||""):"")
        splitModel.setProperty(pickerIndex,"categoryDisplay",id?path:path?"Unmapped: "+path:"Uncategorized")
        splitModel.setProperty(pickerIndex,"categoryTouched",true);dirty=true;categoryPicker.close();if(!selected.newManual)saveEditor()
    }
    function chooseAccountDestination(accountId) {
        if(pickerIndex<0 || pickerIndex>=splitModel.count) return
        let split=splitModel.get(pickerIndex)
        let link=originLink(split.allocationId)
        let stamp=workflow.newIdentity()
        categoryPicker.close()
        transferCommand("transfer_retarget",{link:link||stamp,batch:selected.batch,ordinal:selected.ordinal,allocation:split.allocationId,mode:1,destination:accountId,generatedBatch:"~transfer-"+stamp+"-destination"})
    }
    property bool editorClosed: false
    property Item ledgerEditorHost: null
    property var editView: null
    property var editFocus: null
    property bool committedEditing: false
    property bool editEnabled: (!!selected.editable || (committedEditing && !!selected.canCorrect)) && !s.readOnly
    property bool dirty: false
    property bool manualSplitAmounts: false
    property int automaticSplitIndex: -1
    property string remainderAllocation: ""
    property string splitEditMessage: ""
    property string splitSum: ""
    property var pendingAction: null
    property bool allowClose: false
    property bool closingRequested: false
    property bool commitBeforeQuit: false
    property bool commitAfterReview: false
    function requestCommit() {
        if(s.reviewReady===false){commitAfterReview=true;workflow.command("review_load");return}
        if((s.reviewCount||0)>0)reviewSheet.open()
        else workflow.command("commit")
    }
    function beginQuit() {
        if(!workflow.saveWindowPosition(window))return
        closingRequested=true
        window.hide()
        workflow.command("close")
    }
    function requestQuit() {
        if((s.newTransactions||0)>0)quitCommit.open()
        else {beginQuit()}
    }
    property bool settingUp: false
    function guard(action) { if (dirty) { pendingAction=action; if(!prepareSplitAmounts() && splitSum!=="")splitMismatch.open();else unsaved.open() } else if(categoryDirty || categorySaving) {pendingAction=action;finishCategoryEdit()} else if(rulesDirty) {pendingAction=action;showRulesLeave()} else if(reportsWorkspace.dirty) {reportsWorkspace.requestLeave(action)} else action() }
    function dismiss(dialog) { guardHandoff=true; dialog.close(); Qt.callLater(function(){guardHandoff=false}) }
    function addTransaction() {guard(function(){editorClosed=false;workflow.command("begin_manual")})}
    function discardEditor(next) {
        if(selected.newManual){dirty=false;pendingAction=next||null;workflow.command("cancel_manual")}
        else {loadEditor();if(next)guard(next)}
    }
    function loadEditor() {
        let d=selected.draft
        manualSplitAmounts=!!(d && d.manualSplitAmounts);splitEditMessage="";splitSum=""
        automaticSplitIndex=-1;remainderAllocation=d?(d.remainderAllocation||""):""
        splitModel.clear()
        if (d) {
            editDate.text=d.date; editPayee.text=d.payee; editMemo.text=d.memo; editTotal.text=d.total
            for(let a of d.splits) splitModel.append(a)
            if(!manualSplitAmounts){
                let candidates=d.splits.map(function(a){return {amount:a.amount,transferAmount:!!a.transferAmount || !!originLink(a.allocationId)}})
                automaticSplitIndex=remainderAllocation!==""?d.splits.findIndex(a=>a.allocationId===remainderAllocation):workflow.chooseSplitRemainder(d.total,candidates,d.currencyDefinition)
                if(automaticSplitIndex>=0 && candidates[automaticSplitIndex].transferAmount)automaticSplitIndex=-1
                if(automaticSplitIndex>=0)remainderAllocation=d.splits[automaticSplitIndex].allocationId
                else manualSplitAmounts=true
            }
        }
        dirty=!!selected.newManual;committedEditing=!!selected.canCorrect;payeeTouched=false;memoTouched=false;restoringImportedText=false
    }
    // Inspect the outer syntax without treating quoted text, escaped symbols or
    // character classes as anchors/alternation. Matching still belongs to PCRE2.
    function patternShape(body) {
        let depth=0, quoted=false, inClass=false, classStart=0
        let alternative=false, endAnchor=false, outerEnd=-1
        for(let i=0;i<body.length;++i) {
            let c=body[i]
            if(quoted){if(c==="\\" && body[i+1]==="E"){quoted=false;++i}continue}
            if(c==="\\"){if(body[i+1]==="Q")quoted=true;++i;continue}
            if(inClass){if(c==="]" && i>classStart)inClass=false;continue}
            if(c==="["){inClass=true;classStart=i+1;if(body[i+1]==="^")++classStart;continue}
            if(c==="(")++depth
            else if(c===")"){--depth;if(depth===0 && outerEnd<0)outerEnd=i}
            else if(c==="|" && depth===0)alternative=true
            else if(c==="$" && depth===0 && i===body.length-1)endAnchor=true
        }
        return {alternative:alternative,endAnchor:endAnchor,complete:!quoted && !inClass && depth===0,
            wrapped:body.startsWith("^(?:") && outerEnd===body.length-2 && endAnchor}
    }
    function wholeRecognized(expr) {
        let body=expr.startsWith("(?-i)")?expr.slice(5):expr, shape=patternShape(body)
        return shape.complete && body.startsWith("^") && shape.endAnchor && !shape.alternative
    }
    function ruleRow(rule) {
        let copy=Object.assign({payeeTemplate:"",memoTemplate:"",categoryId:"",inheritParent:false,label:"",expression:"",scope:"transaction",input:"payee"}, rule||{})
        copy.pendingDelete=false
        copy.ruleOn=!!rule.enabled
        delete copy.enabled
        return copy
    }
    function loadRulesForm() {
        ruleSync=true; ruleModel.clear()
        for(let rule of (s.rules||[])) ruleModel.append(ruleRow(rule))
        ruleMovedIds=({});ruleBaseline=[];for(let i=0;i<ruleModel.count;i++)ruleBaseline.push(ruleImage(ruleModel.get(i)))
        ruleIndex=-1; ruleCategoryId=""; rulesDirty=false; ruleSync=false;ruleEditGeneration++
    }
    function showRulesPage() {
        if(currentPage==="rules") return
        rememberAccountViewport()
        let entries=navigation.slice(0,navigationIndex+1); entries.push("rules"); navigation=entries;let accounts=navigationAccounts.slice(0,navigationIndex+1);accounts.push(s.accountId);navigationAccounts=accounts; navigationIndex=entries.length-1; currentPage="rules"
    }
    function rulesImage() {
        let rows=[]
        for(let i=0;i<ruleModel.count;i++)if(!ruleModel.get(i).pendingDelete)rows.push(ruleImage(ruleModel.get(i)))
        return rows
    }
    function writeRule() {
        if(ruleSync || ruleIndex<0 || ruleIndex>=ruleModel.count) return
        let inherit=false
        ruleModel.setProperty(ruleIndex,"label",ruleLabel.text)
        ruleModel.setProperty(ruleIndex,"expression",ruleExpression.text)
        ruleModel.setProperty(ruleIndex,"scope",ruleScope.currentIndex===1?"split":"transaction")
        ruleModel.setProperty(ruleIndex,"input",ruleInput.currentIndex===1?"memo":ruleInput.currentIndex===2?"category":"payee")
        ruleModel.setProperty(ruleIndex,"payeeTemplate",ruleScope.currentIndex===1?"":rulePayeeTemplate.text)
        ruleModel.setProperty(ruleIndex,"memoTemplate",ruleMemoTemplate.text)
        ruleModel.setProperty(ruleIndex,"inheritParent",inherit)
        ruleModel.setProperty(ruleIndex,"categoryId",inherit?"":ruleCategoryId)
        updateRulesDirty()
    }
    function openRule(index) {
        if(index<0 || index>=ruleModel.count) {ruleIndex=-1; return}
        ruleIndex=index; let r=ruleModel.get(index); ruleSync=true
        ruleLabel.text=r.label||""; ruleExpression.text=r.expression||""
        ruleScope.currentIndex=r.scope==="split"?1:0
        ruleInput.currentIndex=r.input==="memo"?1:r.input==="category"?2:0
        rulePayeeTemplate.text=r.payeeTemplate||""; ruleMemoTemplate.text=r.memoTemplate||""
        ruleCategoryId=r.categoryId||""
        ruleCategoryField.resetText()
        ruleCaseSensitive.checked=(r.expression||"").startsWith("(?-i)")
        ruleWholeField.checked=wholeRecognized(r.expression||"")
        ruleAssistNote.text=""; ruleSync=false;rulePreviewKey="";revealRuleEditor()
    }
    function setCase(on) {
        let expr=ruleExpression.text
        if(on) { if(!expr.startsWith("(?-i)")) expr="(?-i)"+expr }
        else if(expr.startsWith("(?-i)")) {
            let rest=expr.slice(5)
            if(rest.indexOf("(?i)")>=0 || rest.indexOf("(?-i)")>=0) { ruleAssistNote.text="Another case flag remains. Edit the expression."; ruleSync=true; ruleCaseSensitive.checked=true; ruleSync=false; return }
            expr=rest; ruleAssistNote.text=""
        }
        if(expr!==ruleExpression.text) ruleExpression.text=expr
        syncAssist();writeRule()
    }
    function setWhole(on) {
        let expr=ruleExpression.text
        let prefix=expr.startsWith("(?-i)")?"(?-i)":""
        let body=prefix?expr.slice(5):expr
        let shape=patternShape(body)
        if(!shape.complete){ruleAssistNote.text="Complete the pattern before changing Whole field.";syncAssist();return}
        if(on && !wholeRecognized(expr)) {
            if(shape.alternative)body="(?:"+body+")"
            else {if(body.startsWith("^"))body=body.slice(1);if(shape.endAnchor)body=body.slice(0,-1)}
            expr=prefix+"^"+body+"$"
        } else if(!on && wholeRecognized(expr))expr=prefix+(shape.wrapped?body.slice(4,-2):body.slice(1,-1))
        ruleAssistNote.text=""
        if(expr!==ruleExpression.text) ruleExpression.text=expr
        syncAssist();writeRule()
    }
    function syncAssist() { ruleSync=true; ruleCaseSensitive.checked=ruleExpression.text.startsWith("(?-i)"); ruleWholeField.checked=wholeRecognized(ruleExpression.text); ruleSync=false }
    function newRule() {
        if(ruleModel.count>=128 || newRuleIndex()>=0) return
        ruleModel.append(ruleRow({id:workflow.newIdentity(),enabled:true}))
        updateRulesDirty();ruleSearch.clear(); openRule(ruleModel.count-1)
    }
    function moveRule(index, delta) {
        let next=index+delta
        if(next<0 || next>=ruleModel.count) return
        let moved=Object.assign({},ruleMovedIds);moved[ruleModel.get(index).id]=true;ruleMovedIds=moved
        ruleModel.move(index,next,1)
        if(ruleIndex===index) ruleIndex=next; else if(ruleIndex===next) ruleIndex=index
        updateRulesDirty()
    }
    function toggleRule(index) { let row=ruleModel.get(index); if(!row) return; ruleModel.setProperty(index,"ruleOn",!row.ruleOn); updateRulesDirty() }
    function deleteRule(index) {
        if(!savedRule(ruleModel.get(index).id)) {ruleModel.remove(index);openRule(Math.min(index,ruleModel.count-1))}
        else {ruleModel.setProperty(index,"pendingDelete",true);openRule(index)}
        updateRulesDirty()
    }
    function copyRemoved(id) {
        if(ruleModel.count>=128 || newRuleIndex()>=0) return
        let tomb=null
        for(let item of (s.ruleTombstones||[])) if(item.id===id) tomb=item
        if(!tomb) return
        ruleModel.append(ruleRow({id:workflow.newIdentity(),label:tomb.label||"",enabled:true,scope:tomb.scope,input:tomb.input,expression:tomb.expression||"",payeeTemplate:tomb.payeeTemplate||"",memoTemplate:tomb.memoTemplate||"",categoryId:tomb.categoryId||"",inheritParent:!!tomb.inheritParent}))
        updateRulesDirty();ruleSearch.clear(); openRule(ruleModel.count-1)
    }
    function actionSummary(rule) {
        if(!rule) return "No action yet"
        let parts=[]
        if(rule.payeeTemplate) parts.push("payee "+rule.payeeTemplate)
        if(rule.memoTemplate) parts.push("memo "+rule.memoTemplate)
        if(rule.inheritParent) parts.push("inherit parent")
        else if(rule.categoryId) { let cat=(s.categoryById||{})[rule.categoryId]||{}; parts.push(cat.path||rule.categoryId) }
        return parts.length?parts.join("; "):"No action yet"
    }
    function acceptRuleCategory() {
        if(ruleIndex<0 || ruleModel.get(ruleIndex).pendingDelete)return true
        let match=ruleCategoryField.model.find(c=>workflow.categoryNameMatches(c.path,ruleCategoryField.text))
        if(!match){workspaceFeedback="Choose an existing category.";return false}
        if(ruleCategoryId!==match.id){ruleCategoryId=match.id;writeRule()}
        return true
    }
    function previewThisRule() {
        if(ruleIndex<0 || ruleModel.get(ruleIndex).pendingDelete || !acceptRuleCategory()) return
        rulePreviewSingle=true; previewAccountRules()
    }
    function previewAccountRules() {
        if(!acceptRuleCategory())return
        let args={accountScope:true,newOnly:ruleTransactionScope.currentIndex===1,replaceManual:false,unsaved:rulePreviewSingle?ruleChanged(ruleIndex):rulesDirty,singleRule:rulePreviewSingle}
        if(rulePreviewSingle){args.rule=ruleModel.get(ruleIndex).id;if(ruleChanged(ruleIndex))args.rules=[ruleImage(ruleModel.get(ruleIndex))]}
        else if(rulesDirty)args.rules=rulesImage()
        rulePendingPreviewKey=rulesPreviewKey();rulePreviewKey=""
        workflow.command("rules_preview",args)
    }
    function ruleTargets() {
        const splits=(selected.draft||{}).splits||[]
        let targets=[{index:-1,label:"Transaction (payee)",rules:[],creatable:!selected.manual}]
        if(splits.length>1)for(let i=0;i<splits.length;i++)targets.push({index:i,label:"Split "+(i+1)+" (memo) · "+splits[i].amount+" · "+splits[i].memo,rules:[],creatable:splits[i].sourceOrdinal>=0})
        for(const assignment of selected.ruleAssignments||[]) {
            if(!assignment.available)continue
            const index=assignment.scope==="split" && splits.length>1?assignment.allocation:-1
            const target=targets.find(t=>t.index===index)
            if(target && !target.rules.some(r=>r.rule===assignment.rule))target.rules.push(assignment)
        }
        return targets
    }
    function ruleCreateChoices() {return ruleTargets().filter(t=>t.creatable && !t.rules.length)}
    function ruleChoices() {
        let choices=[]
        for(const target of ruleTargets())for(const rule of target.rules)choices.push({rule:rule.rule,label:target.label+" · "+rule.label})
        return choices
    }
    function transactionRuleAction() {
        const choices=ruleCreateChoices()
        if(choices.length===1)createRuleTarget(choices[0].index)
        else if(choices.length>1)ruleSplitDialog.open()
    }
    function createRuleTarget(index) {ruleSplitDialog.close();if(index<0)createFromSource("payee");else createFromSource("memo",index)}
    function editTransactionRule() {
        const choices=ruleChoices()
        if(choices.length===1)editMatched(choices[0].rule)
        else if(choices.length>1)matchedRuleDialog.open()
    }
    function saveRules() {
        if(ruleIndex<0 || !acceptRuleCategory())return
        let row=ruleModel.get(ruleIndex)
        let image=ruleBaseline.filter(r=>r.id!==row.id)
        if(!row.pendingDelete) {
            // Place this identity relative to saved neighbours; unrelated draft edits
            // never enter this durable request.
            let before=""
            for(let i=ruleIndex+1;i<ruleModel.count;i++)if(image.some(r=>r.id===ruleModel.get(i).id)){before=ruleModel.get(i).id;break}
            let savedPosition=ruleBaseline.findIndex(r=>r.id===row.id)
            let position=savedPosition>=0 && !ruleMovedIds[row.id]?savedPosition:(before?image.findIndex(r=>r.id===before):image.length)
            image.splice(position,0,ruleImage(row))
        }
        ruleSavingAll=false;ruleSavingId=row.id
        workflow.command("rules_save",{rules:image})
    }
    function previewRules() {rulePreviewSingle=false;previewAccountRules()}
    function applyReady() {return !workflow.busy && ruleIndex>=0 && !ruleModel.get(ruleIndex).pendingDelete}
    function applyRules() {
        if(!applyReady() || !acceptRuleCategory())return
        rulePreviewSingle=true
        if(ruleChanged(ruleIndex)){ruleApplyAfterSave=true;saveRules()}
        else {ruleApplyAfterPreview=true;previewAccountRules()}
    }
    function rulesSaved() {
        let selectedId=ruleIndex>=0?ruleModel.get(ruleIndex).id:""
        if(ruleSavingAll)loadRulesForm()
        else {
            // Refresh the saved baseline without replacing any other local editor
            // row or its position. Other drafts can remain invalid or reordered.
            ruleBaseline=(s.rules||[]).map(r=>ruleImage(ruleRow(r)))
            for(let i=0;i<ruleModel.count;i++)if(ruleModel.get(i).id===ruleSavingId) {
                let saved=savedRule(ruleSavingId)
                if(saved)ruleModel.set(i,ruleRow(saved))
                else {ruleModel.remove(i);ruleIndex=-1}
                break
            }
            let moved=Object.assign({},ruleMovedIds);delete moved[ruleSavingId];ruleMovedIds=moved
        }
        for(let i=0;i<ruleModel.count;i++)if(ruleModel.get(i).id===selectedId){openRule(i);break}
        updateRulesDirty()
        ruleSavingId="";ruleSavingAll=false
        if(rulesLeaving && !rulesDirty){setRulesLeaving(false);let next=pendingAction;pendingAction=null;if(next)guard(next)}
    }
    function createFromSource(kind, allocation) {
        if(workflow.busy || !s.rulesSupported) return
        if(dirty) { pendingAction=function(){createFromSource(kind,allocation)}; unsaved.open(); return }
        if(categoryDirty) { pendingAction=function(){createFromSource(kind,allocation)}; finishCategoryEdit(); return }
        let args={kind:kind}; if(allocation!==undefined) args.allocation=allocation
        workflow.command("rules_prefill",args)
    }
    function editMatched(id) {
        if(workflow.busy) return
        if(dirty) { pendingAction=function(){editMatched(id)}; unsaved.open(); return }
        if(categoryDirty) { pendingAction=function(){editMatched(id)}; finishCategoryEdit(); return }
        if(!rulesDirty) loadRulesForm()
        for(let i=0;i<ruleModel.count;i++) if(ruleModel.get(i).id===id) { openRule(i); showRulesPage(); matchedRuleDialog.close(); return }
    }
    function acceptPrefill() {
        if(!rulesDirty) loadRulesForm()
        if(ruleModel.count>=128 || newRuleIndex()>=0) return
        let row=s.rulePrefill||{}; if(!row.id) return
        ruleModel.append(ruleRow(row))
        updateRulesDirty();ruleSearch.clear(); openRule(ruleModel.count-1); showRulesPage()
    }
    function inspectRecord(batch, ordinal) { guard(function(){workflow.command("inspect",{batch:batch,ordinal:ordinal})}) }
    function toggleRecord(batch, ordinal) {
        if(!editorClosed && selected.batch===batch && selected.ordinal===ordinal)
            guard(function(){editorClosed=true;ledger.forceActiveFocus()})
        else inspectRecord(batch,ordinal)
    }
    function rememberEditView() {
        captureLedgerView("edit")
        editView=ledgerViewRestore
        if(editView && (currentPage==="ledger" || reportLedger) && workflow.selectedRowIndex()>=0)editView.editorOffset=ledgerEditorHost?ledger.contentY-ledgerEditorHost.y:0
        ledgerViewRestore=null;ledgerViewKind=""
        let field=window.activeFocusItem
        editFocus=field?{name:field.objectName,position:field.cursorPosition}:null
    }
    function editorField(item,name) {
        if(item.objectName===name)return item
        for(let child of item.children){let found=editorField(child,name);if(found)return found}
        return null
    }
    function restoreEditView() {
        let view=editView,focus=editFocus;editView=null;editFocus=null
        Qt.callLater(function(){
            ledger.forceLayout()
            if(view){
                // Sorting can move the edited transaction. Follow its identity,
                // retaining its screen offset when possible, not its old index.
                if(view.editorOffset!==undefined && workflow.selectedRowIndex()>=0){
                    let index=workflow.selectedRowIndex()
                    ledger.positionViewAtIndex(index,ListView.Beginning)
                    ledger.forceLayout()
                    let row=ledger.itemAtIndex(index)
                    if(row)ledger.contentY=Math.max(ledger.originY,Math.min(row.y+view.editorOffset,ledger.originY+Math.max(0,ledger.contentHeight-ledger.height)))
                    ledger.positionViewAtIndex(index,ListView.Contain)
                }else restoreViewport(view)
            }
            // A newly materialized row acquires the editor height after the
            // first positioning pass. Settle that layout before restoring focus.
            Qt.callLater(function(){
                ledger.forceLayout()
                if(view && view.editorOffset!==undefined && workflow.selectedRowIndex()>=0)
                    ledger.positionViewAtIndex(workflow.selectedRowIndex(),ListView.Contain)
                let field=focus && focus.name?editorField(editor,focus.name):null
                if(field){field.forceActiveFocus();if(focus.position!==undefined)field.cursorPosition=focus.position}
            })
        })
    }
    function undoEdit() {
        guard(function(){if(s.canUndoManual){workflow.command("undo_manual");return;}if(s.groupEditUndo)captureLedgerView("selected-edit");else rememberEditView();workflow.command("undo_edit")})
    }
    function selectLedgerRow(row, modifiers) {
        guard(function(){workflow.command("select",{row:row,range:!!(modifiers & Qt.ShiftModifier),toggle:!!(modifiers & (Qt.ControlModifier|Qt.MetaModifier)),firstSearchSelection:!!searchReturn && !searchReturn.chosen})})
    }
    function restoreImportedValues() {
        if (!editEnabled || workflow.busy) return
        let fields=selected.sourceFields||[]
        if (!fields.some(f=>f.field==="payee")) return
        let value=name=>{let field=fields.find(f=>f.split<0 && f.field===name);return field?field.value:""}
        let restored=[]
        for(let field of fields) {
            if(field.split<0)continue
            if(!restored[field.split]) {
                let old=(selected.draft.splits||[]).find(a=>a.sourceOrdinal===field.split)
                restored[field.split]={category:"",memo:"",amount:"",allocationId:old?old.allocationId:"",
                    categoryId:"",bindingRevision:"",categoryDisplay:"Uncategorized",sourceOrdinal:field.split,
                    categoryTouched:true,memoTouched:false}
            }
            restored[field.split][field.field]=field.value
        }
        editDate.text=value("date");editPayee.text=value("payee");editMemo.text=value("memo");editTotal.text=value("total")
        splitModel.clear()
        for(let part of restored) {
            part.categoryDisplay=part.category||"Uncategorized"
            splitModel.append(part)
        }
        automaticSplitIndex=manualSplitAmounts?-1:restored.findIndex(a=>a.allocationId===remainderAllocation)
        if(automaticSplitIndex<0 && remainderAllocation!=="")manualSplitAmounts=true
        restoringImportedText=true;payeeTouched=false;memoTouched=false;dirty=true
        editDate.forceActiveFocus()
    }
    function removeEditorSplit(index) {
        if(index<0 || index>=splitModel.count || splitModel.count<=1)return
        dirty=true
        if(index===automaticSplitIndex){manualSplitAmounts=true;automaticSplitIndex=-1}
        else if(index<automaticSplitIndex)--automaticSplitIndex
        splitModel.remove(index)
        prepareSplitAmounts()
    }
    function resolveAmount(field, definition) {
        let result=workflow.amountValue(field.text,definition||s.currencyDefinition||"")
        if(result.valid)field.text=result.amount
    }
    function prepareSplitAmounts() {
        splitEditMessage="";splitSum=""
        if(!selected.draft || !selected.draft.currencyDefinition)return true
        let amounts=[]
        for(let i=0;i<splitModel.count;++i)amounts.push(splitModel.get(i).amount)
        let result=workflow.splitAmounts(editTotal.text,amounts,!manualSplitAmounts && automaticSplitIndex>=0,selected.draft.currencyDefinition,automaticSplitIndex)
        if(!result.valid){splitEditMessage=result.error;return false}
        if(!result.balanced){splitSum=result.sum;splitEditMessage="Splits add up to "+result.sum+". Adjust the split amounts or close the transaction to change its total.";return false}
        editTotal.text=result.total
        for(let i=0;i<splitModel.count;++i)splitModel.setProperty(i,"amount",result.amounts[i])
        return true
    }
    function saveEditor() {
        if(workflow.busy || !dirty)return
        if(!prepareSplitAmounts()){pendingAction=null;return}
        rememberEditView()
        let splits=[]
        for(let i=0;i<splitModel.count;i++) {let a=splitModel.get(i);splits.push({category:a.category,memo:a.memo,amount:a.amount,categoryId:a.categoryId||"",bindingRevision:a.bindingRevision||"",allocationId:a.allocationId||"",categoryTouched:!!a.categoryTouched,memoTouched:!!a.memoTouched})}
        workflow.command(selected.newManual?"save_manual":committedEditing?"accept_edit":"save", {remainderAllocation:remainderAllocation,manualSplitAmounts:manualSplitAmounts,kind:"correct",date:editDate.text,payee:editPayee.text,memo:editMemo.text,total:editTotal.text,splits:splits,restoreImportedText:restoringImportedText,payeeTouched:payeeTouched,memoTouched:memoTouched})
    }
    onClosing: function(close) {
        if(allowClose) return
        close.accepted=false
        if(workflow.busy) return
        guard(requestQuit)
    }
    Connections {
        target: workflow
        // Drop the old delegate index before a smaller filtered model replaces it.
        // The occurrence selection remains in the workflow and is restored below.
        function onModelAboutToBeReset() { ledger.currentIndex=-1 }
        function onChanged() {if(workflow.busy){cancelAccountDrag();captureReviewView()}}
        function onCompleted(action,success) {
            if(success && action==="report_drill"){search.clearSearch();editorClosed=true}
            if(success && action==="report_leave"){search.text=s.filter||"";editorClosed=true}
            if(action==="report_leave" && pendingReportNavigation){let next=pendingReportNavigation;pendingReportNavigation=null;if(success)Qt.callLater(next)}
            if(success && action==="preview_import_undo")importUndoConfirm.open()
            if(success && action==="undo_import"){importUndoConfirm.close();importUndoPicker.close();workspaceFeedback=(s.receipt||{}).summary||"Import undone."}
            if(action==="close" && !success && closingRequested){closingRequested=false;window.show();closeAuditFailure.open()}
            if(action==="review_load" && commitAfterReview){commitAfterReview=false;if(success)Qt.callLater(requestCommit);else commitBeforeQuit=false}

            if(success && (action==="inspect" || action==="undo_edit" || action==="import"))editorClosed=false
            if(success)workspaceFeedback=action==="review_batch"?"Selected matches merged. Undo reverses the whole batch.":action==="review_answer"?"Review decision saved.":action==="review_undo"?"Review decision undone.":"Ready"

            if(success && ["import","confirm_import_date","confirm_import_destination","map_import"].indexOf(action)>=0)workspaceFeedback=(s.importDestination || (s.importPlan||[]).length>0)?"Choose the import destination.":(s.importFeedback||s.importOutcome||"Import completed.")
            if(success && ["open","unlock"].indexOf(action)>=0 && s.startupImportCleanup)workspaceFeedback="The previous shutdown interrupted an import. That incomplete import has been removed. You can import the file again."
            if(success && action==="resolve_import_categories") {importCategories.close();workspaceFeedback="Imported categories applied.";Qt.callLater(showImportCategories)}
            if(success && ["import","confirm_import_date","confirm_import_destination","map_import","open","unlock"].indexOf(action)>=0)Qt.callLater(showImportCategories)
            if(success && action==="commit")workspaceFeedback=(s.receipt||{}).summary||"Transactions accepted."
            if(success && action==="assign_transfer_batch")workspaceFeedback="Transfers assigned. Undo reverses the whole batch."
            if(success && action==="release_transfer_batch")workspaceFeedback="Category changed. Undo is available."
            if(success && action==="undo_transfer_assignment")workspaceFeedback="Category change undone."
            if(success && (action==="save" || action==="accept_edit"))workspaceFeedback=s.canUndoEdit?"Edit saved. Undo is available for this session until another kind of change.":"Edit saved."
            if(success && action==="undo_edit")workspaceFeedback="Edit undone."
            if(success && action==="apply_bulk")workspaceFeedback=(s.bulkReceipt||{}).summary||"Changes saved."
            if(!success){ruleApplyAfterSave=false;ruleApplyAfterPreview=false;rulePreviewKey="";if(ruleValidatingAll){ruleValidatingAll=false;ruleValidationQueue=[];stayInRules();revealRuleEditor()}}
            if(success && action==="rules_reapply_apply")workspaceFeedback=(s.reapplyReceipt||{}).summary||"Rules reapplied."
            if(success && (action==="rules_save" || action==="rules_apply" || action==="rules_account_apply" || action==="rules_account_undo"))workspaceFeedback=(s.ruleReceipt||{}).summary||"Rules updated."
            if(success && action.indexOf("transfer_")===0 && !(s.transferMergePreview||{}).id && (s.transferReceipt||{}).operation)workspaceFeedback="Transfer operation saved."
            if(action==="link_exclusion_save" && success)linkExclusionDialog.close()
            if(action==="link_exclusion_preview" && !success)exclusionPreviewed=false
            if(action==="commit" && commitBeforeQuit) {
                commitBeforeQuit=false
                if(success){Qt.callLater(beginQuit)}
            }
            if(["open","unlock","create","close","prepare_create"].indexOf(action)>=0) startupPending=false
            if(action==="select_account" && pendingAccountMenu) {
                const request=pendingAccountMenu;pendingAccountMenu=null
                if(success && s.accountId===request.id) accountContext.popup(request.x,request.y)
            }
            if(action==="select_account" && pendingImportIssue){if(success)navigate("review");pendingImportIssue=null}
            if(!s.active) {pendingAccountMenu=null;accountContext.close()}
            if(action==="create" && s.overwriteRequired)overwriteDialog.open()
            ledger.currentIndex=workflow.selectedRowIndex()
            if(action==="select")ledger.currentIndex=s.selectionIndex===undefined?-1:s.selectionIndex
            if (action==="inspect" || action==="import" || action==="map_import")
                Qt.callLater(function(){if(ledger.currentIndex>=0) ledger.positionViewAtIndex(ledger.currentIndex,ListView.Contain)})
            if(!success && (action==="save_manual" || action==="save" || action==="accept_edit" || action==="preview_change" || action==="apply_change" || action==="preview_category" || action==="apply_category" || action==="rules_save" || action==="rules_apply" || action==="rules_account_apply" || action==="rules_account_undo" || action==="rules_discard" || action==="rules_preview")) {if(!rulesLeaving)pendingAction=null}
            if (!s.active || (success && (action==="create" || action==="open" || action==="unlock"))) splitViews=({})
            if(!s.active) {importUndoConfirm.close();importUndoPicker.close();importCategories.close();linkExclusionDialog.close();reviewInspecting=false;pendingReviewInspection=false;addingAccount=false;bulkDialog.close();reapplyDialog.close();changePreview.close();mergePreview.close();categoryPicker.close();setRulesLeaving(false);matchedRuleDialog.close();pendingAction=null;pendingMerge=null;mergeHint="";dirty=false;categoryDirty=false;committedEditing=false}
            if(action==="query" && pendingSearch) {
                let change=pendingSearch;pendingSearch=null
                if(success) {
                    appliedFilter=change.filter
                    if(change.start) searchReturn=change.start
                    if(change.clear) {
                        let saved=searchReturn;searchReturn=null
                        if(!saved.chosen) loadEditor()
                        Qt.callLater(function(){
                            if(saved.chosen) {let focus=s.selectionIndex===undefined?ledger.currentIndex:s.selectionIndex;if(focus>=0) ledger.positionViewAtIndex(focus,ListView.Contain)}
                            else restoreViewport(saved)
                        })
                    }
                }
            }
            if(success && action==="inspect" && searchReturn && workflow.selectedRowIndex()>=0)
                searchReturn.chosen=true
            if(success && action==="select" && searchReturn)searchReturn.chosen=true
            if(!s.active || (success && (action==="create" || action==="open" || action==="unlock"))) {
                searchReturn=null;pendingSearch=null;appliedFilter="";search.clear();resetNavigation()
                loadCategoryForm();categoryVisitAdditions=[];categorySort="ascending";categorySelection="";categoryReveal="";categoryMenuEntry=({});categoryContext.close()
                transferDate.text=workflow.todayDisplay();ledgerViewRestore=null;ledgerViewKind=""
            }
            if(!success && (action==="preview_category" || action==="apply_category")) {
                categorySaving=false
                if(categoryDirty && s.active)focusCategoryEditor()
            }
            if(!success && action==="select_account"){pendingAccountNavigation=null;pendingCounterpart=null;pendingReviewInspection=false}
            if(success) {
                if(action==="transfer_manual")resolveAmount(transferAmount)
                if(action==="correct_opening")correctAmount.text=s.opening
                if(action==="select_account" || action==="create_account" || action==="map_import" || action==="group_abort" || action==="archive_account" || action==="delete_account") {
                    addingAccount=false;splitViews={};searchReturn=null;pendingSearch=null;appliedFilter=s.filter||"";search.text=appliedFilter
                    if(pendingCounterpart){pendingCounterpart=null;Qt.callLater(function(){if(workflow.selectedRowIndex()>=0){ledger.currentIndex=workflow.selectedRowIndex();ledger.positionViewAtIndex(ledger.currentIndex,ListView.Beginning)}})}
                    else if(accountViewports[s.accountId])restoreAccountViewport()
                    if(pendingAccountNavigation) {navigationIndex=pendingAccountNavigation.index;currentPage=pendingAccountNavigation.page;pendingAccountNavigation=null}
                    else {let entries=navigation.slice(0,navigationIndex+1);entries.push("ledger");navigation=entries;let accounts=navigationAccounts.slice(0,navigationIndex+1);accounts.push(s.accountId);navigationAccounts=accounts;navigationIndex=entries.length-1;currentPage="ledger"}
                    if(pendingReviewInspection){currentPage="review";pendingReviewInspection=false}
                    loadEditor();loadRulesForm();loadCategoryForm()
                    if(currentPage==="history")Qt.callLater(function(){workflow.command("history")})
                }
                if(action.indexOf("transfer_")===0 || action==="assign_transfer_batch" || action==="release_transfer_batch" || action==="undo_transfer_assignment")loadEditor()
                if(action==="preview_bulk")bulkDialog.open()
                if(action==="apply_bulk"){bulkDialog.close();loadEditor()}
                if(action==="rules_reapply_preview")reapplyDialog.open()
                if(action==="rules_reapply_apply"){reapplyDialog.close();loadEditor()}
                if(action==="preview_category") workflow.command("apply_category",{preview:s.categoryPreview.id})
                if(action==="apply_category" && categoryAction==="create")categoryVisitAdditions=categoryVisitAdditions.concat([(s.categorySelected||{}).id])
                if(action==="category_select" || action==="apply_category")loadCategoryForm()
                if(action==="apply_category") {categorySelection=(s.categorySelected||{}).id||"";categoryReveal=categorySelection;revealCategorySelection();let next=pendingAction;pendingAction=null;categoryDirty=false;if(!dirty)loadEditor();if(next)guard(next)}
                if(action==="preview_change") changePreview.open()
                if(action==="apply_change") dismiss(changePreview)
                if(action==="transfer_merge" && (s.transferMergePreview||{}).id) mergePreview.open()
                if(action==="transfer_merge_apply") dismiss(mergePreview)
                if(["delete_selection","rename_payee_selection","restore_imported_selection","begin_manual","cancel_manual","save_manual","undo_manual","inspect","entry_history","import","map_import","group_abort","resume","save","accept_edit","undo_edit","decide","opening","commit","discard","evidence","create","open","unlock","close","apply_change","rename_account","archive_account","reactivate_account","delete_account","correct_opening","reassign_currency","undo_account"].indexOf(action)>=0) loadEditor()
                if(action==="save" || action==="save_manual" || action==="cancel_manual" || action==="accept_edit" || action==="apply_change") {
                    dirty=false
                    if(pendingAction) {let next=pendingAction;pendingAction=null;guard(next)}
                }
                if(action==="rules_load" && !rulesDirty) loadRulesForm()
                if(action==="rules_save")rulesSaved()
                if(action==="rules_discard"){loadRulesForm();setRulesLeaving(false);if(pendingAction){let next=pendingAction;pendingAction=null;guard(next)}}
                if((action==="rules_apply" || action==="rules_account_apply" || action==="rules_account_undo") && !dirty) loadEditor()
                if(action==="rules_save" && ruleApplyAfterSave){ruleApplyAfterSave=false;ruleApplyAfterPreview=true;previewAccountRules()}
                if(action==="rules_preview" && ruleValidatingAll){validateNextRule()}
                else if(action==="rules_preview"){
                    rulePreviewKey=rulePendingPreviewKey
                    if(ruleApplyAfterPreview){ruleApplyAfterPreview=false;workflow.command("rules_account_apply",{preview:s.rulePreview.operation})}
                }
                if(action==="rules_prefill") acceptPrefill()
                if((action==="create" || action==="open" || action==="unlock") && !rulesDirty) loadRulesForm()
                if(action==="create" || action==="unlock") {password.text="";confirmation.text="";unlockPassword.text=""}
                if(action==="close") {settingUp=false;probeTimer.restart();if(closingRequested){allowClose=true;Qt.callLater(function(){window.close();Qt.quit()})} search.clear();splitModel.clear();editDate.clear();editPayee.clear();editMemo.clear();editTotal.clear();dirty=false}
            }
            settleLedgerView(action,success)
            if(action==="save" || action==="save_manual" || action==="accept_edit" || action==="undo_edit")restoreEditView()
            if(success && action==="begin_manual"){editorClosed=false;Qt.callLater(function(){ledger.positionViewAtIndex(workflow.selectedRowIndex(),ListView.Contain);editPayee.forceActiveFocus()})}
        }
    }
    Component.onCompleted: {
        workflow.restoreWindowPosition(window)
        startupPending=!!s.startup
        probeTimer.start()
    }
    Timer {
        id:probeTimer;interval:startupPending?0:350
        onTriggered: {
            if(s.active || settingUp)return
            if(workflow.busy){restart();return}
            workflow.command(startupPending?"open":"probe",{path:location.text,startup:startupPending})
        }
    }
    function openDataset() {
        if(workflow.busy || s.path!==location.text)return
        workflow.command(s.unlockRequired?"unlock":"open",{path:location.text,password:unlockPassword.text})
    }
    property bool setupDateOverridden: false
    property var datePresets: ["YYYY-MM-DD","YYYYMMDD","DD-MM-YYYY","DD/MM/YYYY","MM-DD-YYYY","MM/DD/YYYY","Custom…"]
    function suggestSetupDate() {
        if(setupDateOverridden)return
        let format=workflow.suggestedDateFormat(currency.currentText)
        setupDate.currentIndex=datePresets.indexOf(format);customDate.text=format
    }
    function createDataset(overwrite) {
        location.text=workflow.datasetFilename(location.text)
        workflow.command("create",{path:location.text,name:accountName.text,currency:currency.currentText,protected:protection.checked,password:password.text,confirmation:confirmation.text,overwrite:overwrite,dateFormat:setupDate.currentIndex===6?customDate.text:setupDate.currentText,defaultCategories:defaultCategories.checked})
    }
    Dialog {
        id:overwriteDialog;objectName:"overwriteDatasetDialog";width:Math.min(600,window.width-64);title:"Overwrite dataset?";modal:true;anchors.centerIn:parent
        standardButtons:Dialog.Cancel
        Label {text:location.text+" already exists";textFormat:Text.PlainText;wrapMode:Text.Wrap;width:Math.min(560,window.width-100)}
        footer:DialogButtonBox {
            Button {text:"Cancel";DialogButtonBox.buttonRole:DialogButtonBox.RejectRole}
            Button {text:"Overwrite";objectName:"confirmOverwriteDataset";DialogButtonBox.buttonRole:DialogButtonBox.AcceptRole}
            onRejected:overwriteDialog.close()
            onAccepted:{overwriteDialog.close();createDataset(true)}
        }
    }
    ListModel {id:splitModel}
    ListModel {id:ruleModel}
    FileDialog { id:openDialog; title:"Choose your SpentIt dataset";currentFolder:workflow.documentsFolder
        onAccepted:{settingUp=false;location.text=workflow.localPath(selectedFile);unlockPassword.clear();probeTimer.restart()}
    }
    FileDialog { id:createDialog; fileMode:FileDialog.SaveFile; title:"New SpentIt dataset";currentFolder:workflow.documentsFolder;selectedFile:workflow.suggestedFile;defaultSuffix:"spentit";options:FileDialog.DontConfirmOverwrite
        onAccepted:location.text=workflow.datasetFilename(workflow.localPath(selectedFile))
    }
    ListModel {id:importCategoryModel}
    function showImportCategories() {
        let pending=s.importCategoryDecisions||[]
        if(!pending.length)return
        importCategories.source=pending[0].source
        allImportCategories.checked=true
        importCategoryModel.clear()
        for(let name of pending[0].names)importCategoryModel.append({categoryName:name,selected:true})
        importCategories.open()
    }
    Dialog {
        id:importCategories;objectName:"importCategories";title:"These imported categories don’t exist yet."
        modal:true;anchors.centerIn:parent;width:Math.min(window.width-40,640);height:Math.min(window.height-40,500)
        closePolicy:workflow.busy?Popup.NoAutoClose:Popup.CloseOnEscape
        property string source:""
        ColumnLayout {
            anchors.fill:parent
            Label {text:"Select the categories to create. Unselected categories will use Uncategorized.";wrapMode:Text.Wrap;Layout.fillWidth:true}
            CheckBox {id:allImportCategories;objectName:"allImportCategories";text:"All / None";checked:true;enabled:!workflow.busy
                onClicked:{for(let i=0;i<importCategoryModel.count;i++)importCategoryModel.setProperty(i,"selected",checked)}
            }
            ListView {
                Layout.fillWidth:true;Layout.fillHeight:true;clip:true;model:importCategoryModel
                ScrollBar.vertical:ScrollBar {policy:ScrollBar.AlwaysOn}
                delegate:CheckBox {required property string categoryName;required property bool selected;required property int index
                    width:ListView.view.width;text:categoryName;checked:selected;enabled:!workflow.busy
                    onClicked:importCategoryModel.setProperty(index,"selected",checked)
                }
            }
            RowLayout {
                Button {objectName:"applyImportCategories";text:"Apply Selected";enabled:!workflow.busy;onClicked:{let names=[];for(let i=0;i<importCategoryModel.count;i++)if(importCategoryModel.get(i).selected)names.push(importCategoryModel.get(i).categoryName);workflow.command("resolve_import_categories",{source:importCategories.source,selected:names})}}
                Button {objectName:"uncategorizedImport";text:"Use Uncategorized";enabled:!workflow.busy;onClicked:workflow.command("resolve_import_categories",{source:importCategories.source,selected:[]})}
                Button {text:"Close";enabled:!workflow.busy;onClicked:importCategories.close()}
            }
        }
    }
    Dialog {
        id:importMapping;objectName:"importAccountMapping";title:"Choose accounts for this import";modal:true;closePolicy:Popup.NoAutoClose
        anchors.centerIn:parent;width:Math.min(window.width-40,800);height:Math.min(window.height-40,620)
        visible:(s.importPlan||[]).length>0
        property var mappings:[]
        property var choices:[]
        onOpened:{
            let options=[{id:"",label:"Choose an account"},{id:"new",label:"Create a new account"}]
            for(let a of (s.accounts||[])) options.push({id:a.id,label:a.name+" · "+a.currency+" · #"+a.id})
            choices=options
            mappings=(s.importPlan||[]).map(function(a){return {source:a.source,account:"",create:false,currency:a.currency,date:a.openingDate,useReportedOpening:false}})
        }
        function update(index,key,value){let rows=mappings.slice();let row=Object.assign({},rows[index]);row[key]=value;rows[index]=row;mappings=rows}
        contentItem:ColumnLayout {
            Label {text:"Choose an existing account or create one for each account found in the import data.";wrapMode:Text.Wrap;Layout.fillWidth:true}
            ListView {
                id:mappingList;objectName:"sourceAccountMappingList";Layout.fillWidth:true;Layout.fillHeight:true;clip:true;spacing:12;model:s.importPlan||[]
                delegate:ColumnLayout {
                    required property var modelData;required property int index
                    id:mappingDelegate; width:mappingList.width
                    Label {text:modelData.name+" · "+modelData.type+(modelData.warning?"\n"+modelData.warning:"")+" · reported balance: "+modelData.reported;textFormat:Text.PlainText;Layout.fillWidth:true;wrapMode:Text.Wrap}
                    UsableComboBox {objectName:"sourceAccountChoice_"+index;model:importMapping.choices;textRole:"label";currentIndex:importMapping.choices.findIndex(function(c){return c.id===(importMapping.mappings[mappingDelegate.index]||{}).account});Layout.fillWidth:true;Accessible.name:"Destination for "+modelData.name
                        onActivated:function(choiceIndex){let item=importMapping.choices[choiceIndex];importMapping.update(mappingDelegate.index,"account",item.id);importMapping.update(mappingDelegate.index,"create",item.id==="new")}}
                    RowLayout {
                        visible:!!importMapping.mappings[index] && importMapping.mappings[index].create
                        Label {text:"Currency"}
                        UsableComboBox {objectName:"sourceCurrency_"+index;model:workflow.currencies;currentIndex:workflow.currencies.indexOf((importMapping.mappings[mappingDelegate.index]||{}).currency);Accessible.name:"Currency for "+modelData.name;onActivated:function(choiceIndex){importMapping.update(mappingDelegate.index,"currency",currentText)}}
                        Label {text:"Opening date"}
                        NavigableTextField {objectName:"sourceOpeningDate_"+index;Layout.fillWidth:true;text:importMapping.mappings[index]?importMapping.mappings[index].date:"";placeholderText:s.dateFormat||"YYYY-MM-DD";Accessible.name:"Opening date for "+modelData.name;onTextEdited:importMapping.update(mappingDelegate.index,"date",text)}
                    }
                    RowLayout {
                        visible:!!importMapping.mappings[index] && importMapping.mappings[index].create
                        Label {text:"Opening balance"}
                        UsableComboBox {objectName:"sourceOpeningBalance_"+index;Layout.fillWidth:true;Accessible.name:"Opening balance for "+modelData.name
                            model:modelData.offerReportedOpening?["0.00","Use reported balance as opening: "+modelData.reported]:["0.00"]
                            currentIndex:(importMapping.mappings[mappingDelegate.index]||{}).useReportedOpening?1:0
                            onActivated:function(choiceIndex){importMapping.update(mappingDelegate.index,"useReportedOpening",choiceIndex===1)}
                        }
                    }
                }
            }
            RowLayout {
                Button {objectName:"confirmImportMappings";text:"Import";enabled:!workflow.busy && importMapping.mappings.length>0 && importMapping.mappings.every(function(a){return a.account!==""});onClicked:workflow.command("map_import",{mappings:importMapping.mappings})}
                Button {text:"Cancel";enabled:!workflow.busy;onClicked:workflow.command("cancel_import_mapping")}
                Button {text:"Cancel processing";visible:workflow.busy;enabled:workflow.canCancel;onClicked:workflow.requestCancel()}
            }
            Label {text:errorDialog===importMapping?workflow.error:"";visible:text.length>0;wrapMode:Text.Wrap;Layout.fillWidth:true;textFormat:Text.PlainText}
        }
    }
    Dialog {
        id:importDateOrder;objectName:"importDateOrderDialog";title:"Which date order does this file use?"
        modal:true;closePolicy:Popup.NoAutoClose;anchors.centerIn:parent
        width:Math.min(window.width-48,560)
        visible:!!(s.importDateQuestion||{}).example
        property string choice:""
        onVisibleChanged:if(visible)choice=""
        ColumnLayout {
            anchors.fill:parent
            Label {Layout.fillWidth:true;wrapMode:Text.Wrap;textFormat:Text.PlainText
                text:"We couldn’t determine the date order from this file. For example, " + ((s.importDateQuestion||{}).example||"") + " could mean either:"}
            RadioButton {objectName:"importDateDMY";text:"Day / Month / Year — "+((s.importDateQuestion||{}).dmy||"");checked:importDateOrder.choice==="DMY";onClicked:importDateOrder.choice="DMY"}
            RadioButton {objectName:"importDateMDY";text:"Month / Day / Year — "+((s.importDateQuestion||{}).mdy||"");checked:importDateOrder.choice==="MDY";onClicked:importDateOrder.choice="MDY"}
            RowLayout {
                Button {objectName:"confirmImportDate";text:"Continue";enabled:!workflow.busy && importDateOrder.choice!=="";onClicked:workflow.command("confirm_import_date",{profile:importDateOrder.choice})}
                Button {text:"Cancel import";enabled:!workflow.busy;onClicked:workflow.command("cancel_import_date")}
            }
        }
    }
    Dialog {id:importDestinationDialog;objectName:"importDestinationDialog";title:"Choose import destination";modal:true;closePolicy:Popup.NoAutoClose;anchors.centerIn:parent
        visible:!!s.importDestination
        standardButtons:Dialog.Ok|Dialog.Cancel
        contentItem:UsableComboBox {id:importDestinationAccount;objectName:"importDestinationAccount";model:s.accounts||[];textRole:"name";Accessible.name:"Import destination account"}
        onAccepted:workflow.command("confirm_import_destination",{account:importDestinationAccount.currentIndex>=0?importDestinationAccount.model[importDestinationAccount.currentIndex].id:""})
        onRejected:workflow.command("cancel_import_destination")
    }
    property string importTarget: ""
    FileDialog { id:importDialog; title:"Import statement"; nameFilters:["Statement files (*.qif *.ofx *.qfx *.qbo *.aso)","QIF files (*.qif)","OFX files (*.ofx)","QFX files (*.qfx)","QBO files (*.qbo)","ASO files (*.aso)","All files (*)"]; onAccepted:workflow.command("import",{path:workflow.localPath(selectedFile),account:importTarget}) }
    FileDialog { id:backupDialog; fileMode:FileDialog.SaveFile; title:"Save a backup to a new file"; onAccepted:workflow.command("backup",{path:workflow.localPath(selectedFile)}) }
    Dialog {
        id:splitMismatch;objectName:"splitMismatchDialog";title:"Change transaction amount?";modal:true;anchors.centerIn:parent
        onRejected:pendingAction=null
        ColumnLayout {
            Label {textFormat:Text.PlainText;text:"The splits add up to "+splitSum+". Change the transaction amount from "+editTotal.text+" to "+splitSum+"?";wrapMode:Text.Wrap;Layout.maximumWidth:480}
            RowLayout {
                Button {objectName:"splitChangeAmount";text:"Change amount";onClicked:{editTotal.text=splitSum;dirty=true;splitMismatch.close();saveEditor()}}
                Button {objectName:"splitKeepEditing";text:"Keep editing";onClicked:{pendingAction=null;splitMismatch.close()}}
            }
        }
    }
    Dialog {
        id:unsaved;objectName:"unsavedDialog"; title:"Unsaved changes"; modal:true; anchors.centerIn:parent
        ColumnLayout {
            Label {textFormat:Text.PlainText;text:"Save your changes before leaving?"}
            RowLayout {
                Button {objectName:"guardSave";text:"Save";onClicked:{let next=pendingAction;dismiss(unsaved);pendingAction=next;if(!prepareSplitAmounts() && splitSum!=="")splitMismatch.open();else saveEditor()}}
                Button {objectName:"guardDiscard";text:"Discard";onClicked:{let next=pendingAction;pendingAction=null;dismiss(unsaved);discardEditor(next)}}
                Button {objectName:"guardStay";text:"Stay";onClicked:{unsaved.close();pendingAction=null}}
            }
        }
        onRejected:if(!guardHandoff){pendingAction=null}
    }
    Dialog {
        id:changePreview;objectName:"committedChangePreview";modal:true;anchors.centerIn:parent
        width:Math.min(window.width-48,820);height:Math.min(window.height-80,650)
        title:(s.changePreview||{}).title||"Review change"
        onRejected:if(!guardHandoff){pendingAction=null;workflow.command("cancel_change")}
        ColumnLayout {
            anchors.fill:parent
            Label {objectName:"changeBalanceImpact";textFormat:Text.PlainText;text:(s.changePreview||{}).balanceImpact||"";font.bold:true;wrapMode:Text.Wrap;Layout.fillWidth:true}
            UsableScrollView {
                id:previewScroll;Layout.fillWidth:true;Layout.fillHeight:true;contentWidth:availableWidth
                ColumnLayout {
                    width:previewScroll.availableWidth
                    Repeater {
                        model:(s.changePreview||{}).snapshots||[]
                        ColumnLayout {
                            required property var modelData
                            Layout.fillWidth:true
                            Label {textFormat:Text.PlainText;text:modelData.label;font.bold:true}
                            Label {textFormat:Text.PlainText;text:modelData.summary;wrapMode:Text.Wrap;Layout.fillWidth:true}
                            ListView {
                                id:previewAllocations;model:modelData.splits;clip:true;reuseItems:true
                                Layout.fillWidth:true;Layout.preferredHeight:Math.min(150,contentHeight)
                                ScrollBar.vertical:UsableScrollBar {}
                                delegate:Label {required property var modelData;width:previewAllocations.width;textFormat:Text.PlainText;text:modelData.category+" · "+modelData.memo+" · "+modelData.amount;wrapMode:Text.Wrap}
                            }
                        }
                    }
                }
            }
            Label {textFormat:Text.PlainText;text:errorDialog===changePreview?workflow.error:"";visible:text.length>0;color:theme.error;wrapMode:Text.Wrap;Layout.fillWidth:true}
            RowLayout {
                Button {objectName:"applyCommittedChange";text:(s.changePreview||{}).applyLabel||"Apply";enabled:!workflow.busy && !!(s.changePreview||{}).id;onClicked:workflow.command("apply_change",{preview:s.changePreview.id})}
                Button {objectName:"cancelCommittedChange";text:"Cancel";enabled:!workflow.busy;onClicked:{changePreview.close();pendingAction=null;workflow.command("cancel_change")}}
            }
        }
    }
    Dialog {
        id:mergePreview;objectName:"mergePreview";modal:true;anchors.centerIn:parent
        width:Math.min(window.width-48,720)
        title:"Link transactions"
        onRejected:if(!guardHandoff)workflow.command("cancel_merge")
        ColumnLayout {
            anchors.fill:parent
            Label {objectName:"mergePreviewNote";textFormat:Text.PlainText;text:(s.transferMergePreview||{}).note||"";wrapMode:Text.Wrap;Layout.fillWidth:true}
            Label {objectName:"mergePreviewImpact";textFormat:Text.PlainText;text:(s.transferMergePreview||{}).balanceImpact||"";font.bold:true;wrapMode:Text.Wrap;Layout.fillWidth:true}
            Label {textFormat:Text.PlainText;text:"Removes the duplicate transfer created by SpentIt. The transaction in the other account remains.";wrapMode:Text.Wrap;Layout.fillWidth:true}
            Label {textFormat:Text.PlainText;text:errorDialog===mergePreview?workflow.error:"";visible:text.length>0;color:theme.error;wrapMode:Text.Wrap;Layout.fillWidth:true}
            RowLayout {
                Button {objectName:"confirmMerge";text:"Link";enabled:!workflow.busy && !!(s.transferMergePreview||{}).id;onClicked:workflow.command("transfer_merge_apply",{preview:s.transferMergePreview.id})}
                Button {objectName:"cancelMerge";text:"Cancel";enabled:!workflow.busy;onClicked:{mergePreview.close();workflow.command("cancel_merge")}}
            }
        }
    }
    Dialog {
        id:bulkDialog;objectName:"bulkDialog";title:(s.bulkPreview||{}).title||"Review changes";modal:true;anchors.centerIn:parent;width:Math.min(850,window.width-80);height:Math.min(600,window.height-80)
        ColumnLayout {
            anchors.fill:parent
            Label {text:(s.bulkPreview||{}).impact||"";textFormat:Text.PlainText;wrapMode:Text.Wrap;Layout.fillWidth:true}
            ListView {
                id:bulkLines;objectName:"bulkPreviewLines";model:(s.bulkPreview||{}).lines||[];clip:true;reuseItems:true;Layout.fillWidth:true;Layout.fillHeight:true
                ScrollBar.vertical:UsableScrollBar {}
                delegate:Label {required property string modelData;width:bulkLines.width;text:modelData;textFormat:Text.PlainText;wrapMode:Text.Wrap}
            }
            Label {text:errorDialog===bulkDialog?workflow.error:"";textFormat:Text.PlainText;visible:text.length>0;color:theme.error;wrapMode:Text.Wrap;Layout.fillWidth:true}
            RowLayout {
                Button {objectName:"applyBulk";text:"Apply all";enabled:!workflow.busy && !!(s.bulkPreview||{}).id;onClicked:workflow.command("apply_bulk",{preview:s.bulkPreview.id})}
                Button {objectName:"cancelBulk";text:"Cancel";enabled:!workflow.busy;onClicked:{bulkDialog.close();workflow.command("cancel_bulk")}}
            }
        }
    }
    Dialog {
        id:reapplyDialog;objectName:"reapplyDialog";title:(s.reapplyPreview||{}).title||"Reapply rules";modal:true;anchors.centerIn:parent;width:Math.min(850,window.width-80);height:Math.min(600,window.height-80)
        ColumnLayout {
            anchors.fill:parent
            Label {text:(s.reapplyPreview||{}).impact||"";textFormat:Text.PlainText;wrapMode:Text.Wrap;Layout.fillWidth:true}
            Label {text:((s.reapplyPreview||{}).evaluated||0)+" accepted transactions checked · "+((s.reapplyPreview||{}).changed||0)+" changed";textFormat:Text.PlainText;wrapMode:Text.Wrap;Layout.fillWidth:true}
            ListView {
                id:reapplyLines;objectName:"reapplyPreviewLines";model:(s.reapplyPreview||{}).lines||[];clip:true;reuseItems:true;Layout.fillWidth:true;Layout.fillHeight:true
                ScrollBar.vertical:UsableScrollBar {}
                delegate:Label {required property string modelData;width:reapplyLines.width;text:modelData;textFormat:Text.PlainText;wrapMode:Text.Wrap}
            }
            Label {text:(s.reapplyPreview||{}).omitted||"";visible:text.length>0;textFormat:Text.PlainText;wrapMode:Text.Wrap;Layout.fillWidth:true}
            Label {text:errorDialog===reapplyDialog?workflow.error:"";textFormat:Text.PlainText;visible:text.length>0;color:theme.error;wrapMode:Text.Wrap;Layout.fillWidth:true}
            RowLayout {
                Button {objectName:"applyReapply";text:"Apply";enabled:!workflow.busy && !!(s.reapplyPreview||{}).id;onClicked:workflow.command("rules_reapply_apply",{preview:s.reapplyPreview.id})}
                Button {objectName:"cancelReapply";text:"Cancel";enabled:!workflow.busy;onClicked:{reapplyDialog.close();workflow.command("cancel_reapply")}}
            }
        }
    }
    Dialog {
        id:transferSuggestionSettings;title:"Transfer suggestions";modal:true;anchors.centerIn:parent;width:Math.min(window.width-48,700)
        contentItem:ColumnLayout {
                        Label {text:"Link suggestions";font.pixelSize:18}
                        Label {text:"Choose which transfer suggestions to hide.";wrapMode:Text.Wrap;Layout.fillWidth:true}
                        Button {text:"Hide suggestions…";onClicked:openLinkExclusion({payee:"",account:s.accountId},null)}
                        Repeater {model:s.linkExclusions||[];delegate:RowLayout {
                            required property var modelData;Layout.fillWidth:true
                            Label {text:(modelData.account==="0"?"All accounts":"Account "+modelData.account)+" · "+modelData.field+" · "+(modelData.regex?"Regex: ":"Exact: ")+modelData.expression;textFormat:Text.PlainText;elide:Text.ElideRight;Layout.fillWidth:true}
                            Button {text:"Edit";onClicked:openLinkExclusion({},modelData)}
                            Button {text:"Remove…";onClicked:openLinkExclusion({},Object.assign({},modelData,{removed:true}))}
                        }}
        }
    }
    Dialog {
        id:ruleSplitDialog;objectName:"ruleSplitDialog";title:"Create a rule for";modal:true;anchors.centerIn:parent;width:Math.min(window.width-48,600)
        contentItem:ColumnLayout {
            Repeater {model:ruleCreateChoices()
                Button {required property var modelData;objectName:"ruleSplitChoice_"+modelData.index;text:modelData.label
                    onClicked:createRuleTarget(modelData.index)}
            }
        }
    }
    Dialog {
        id:matchedRuleDialog;objectName:"matchedRuleDialog";title:"Choose a rule to edit";modal:true;anchors.centerIn:parent;width:Math.min(window.width-48,640)
        ColumnLayout {
            width:matchedRuleDialog.availableWidth
            Repeater {
                model:ruleChoices()
                Button {required property var modelData;objectName:"matchedRule_"+modelData.rule;text:modelData.label;onClicked:editMatched(modelData.rule)}
            }
        }
    }
    Menu {
        id:categoryContext;objectName:"categoryContextMenu"
        MenuItem {objectName:"renameCategory";text:"Rename";enabled:!categoryMenuEntry.account && !categoryMenuEntry.deleted;onTriggered:categoryMenuAction("rename")}
        MenuItem {objectName:"archiveCategory";text:categoryMenuEntry.ownArchived?"Unarchive":"Archive";enabled:!categoryMenuEntry.account && !categoryMenuEntry.deleted;onTriggered:categoryMenuAction(categoryMenuEntry.ownArchived?"unarchive":"archive")}
        MenuItem {objectName:"deleteCategory";text:"Delete unused category";enabled:!categoryMenuEntry.account && !categoryMenuEntry.deleted;onTriggered:categoryMenuAction("delete")}
    }
    Dialog {
        id:categoryPicker;objectName:"categoryPicker";onOpened:pickerFeedback="";title:"Choose split category";modal:true;anchors.centerIn:parent;width:Math.min(window.width-48,700);height:Math.min(window.height-80,650)
        ColumnLayout {
            anchors.fill:parent
            Label {textFormat:Text.PlainText;text:pickerIndex>=0 && pickerIndex<splitModel.count?"Current: "+splitModel.get(pickerIndex).categoryDisplay:"";wrapMode:Text.Wrap;Layout.fillWidth:true}
            ClearableSearchField {id:categorySearch;onClearRequested:clearSearch();objectName:"categoryPickerSearch";placeholderText:"Search categories";Layout.fillWidth:true;maximumLength:256;Accessible.name:"Category search";onAccepted:{let account=accountForName(categorySearch.text);if(account){chooseCategory("",categorySearch.text);return}let match=(s.categories||[]).find(function(c){return c.assignable && workflow.categoryNameMatches(c.path,categorySearch.text)});if(match)chooseCategory(match.id,match.path)}}
            ListView {
                id:pickerChoices;Layout.fillWidth:true;Layout.fillHeight:true;clip:true;reuseItems:true
                model:sortedCategories((s.categories||[]).concat(s.accountCategories||[]).filter(function(c){return !c.deleted && !c.archived && (c.path+" "+(c.aliases||"")).toLowerCase().indexOf(categorySearch.text.toLowerCase())>=0}))
                ScrollBar.vertical:UsableScrollBar {}
                delegate:Button {required property var modelData;width:pickerChoices.width;text:modelData.path+(modelData.account?" [Account]"+(pickerIndex===-2?" — use transfer workflow":"")+(modelData.accountId===s.accountId?" — current account":""):(modelData.group?" (group)":""));enabled:modelData.account?(pickerIndex!==-2 && modelData.accountId!==s.accountId):modelData.assignable;objectName:modelData.account?"pickAccount_"+modelData.accountId:"pickCategory_"+modelData.id;onClicked:if(modelData.account)chooseAccountDestination(modelData.accountId);else chooseCategory(modelData.id,modelData.path)}
            }
            Button {objectName:"chooseUncategorized";text:"Uncategorized";onClicked:chooseCategory("","")}
            NavigableTextField {id:unmappedCategory;objectName:"unmappedCategoryText";placeholderText:"Explicit unmapped classification text";Layout.fillWidth:true;Accessible.name:"Unmapped category text"}
            Button {objectName:"chooseUnmapped";text:"Keep as unmapped text";enabled:unmappedCategory.text.length>0;onClicked:chooseCategory("",unmappedCategory.text)}
            Label {text:pickerFeedback;visible:text.length>0;color:theme.error;wrapMode:Text.Wrap;Layout.fillWidth:true}
            Label {text:"Accounts are transfer destinations. Their aliases identify the same account. Rules cannot create transfers; use the transfer workflow.";wrapMode:Text.Wrap;Layout.fillWidth:true}
            Label {text:"Choose a category within a group.";wrapMode:Text.Wrap;Layout.fillWidth:true}
            Button {text:"Cancel";onClicked:categoryPicker.close()}
        }
    }
    Dialog {
        id:renameDialog;onOpened:renameField.forceActiveFocus();objectName:"renameAccountDialog";title:"Rename account";modal:true;anchors.centerIn:parent;standardButtons:Dialog.Ok|Dialog.Cancel
        width:Math.min(window.width-48,480)
        ColumnLayout {
            width:renameDialog.availableWidth
            NavigableTextField {id:renameField;objectName:"renameAccountName";Layout.fillWidth:true;Accessible.name:"Account name";onAccepted:acceptDialog(renameDialog)}
            Label {text:"This changes the current display name only.";wrapMode:Text.Wrap;Layout.fillWidth:true}
        }
        onAccepted:workflow.command("rename_account",{operation:workflow.newIdentity(),name:renameField.text})
    }
    Dialog {
        id:deleteAccountDialog;objectName:"deleteAccountDialog";title:"Delete empty account?";modal:true;anchors.centerIn:parent;width:Math.min(window.width-48,640);standardButtons:Dialog.Ok|Dialog.Cancel
        Label {width:deleteAccountDialog.availableWidth;wrapMode:Text.Wrap;textFormat:Text.PlainText;text:"Delete this empty account?"}
        onAccepted:workflow.command("delete_account",{operation:workflow.newIdentity()})
    }
    Dialog {
        id:correctOpeningDialog;objectName:"correctOpeningDialog";title:"Correct opening balance";modal:true;anchors.centerIn:parent;standardButtons:Dialog.Ok|Dialog.Cancel
        width:Math.min(window.width-48,520)
        ColumnLayout {
            width:correctOpeningDialog.availableWidth
            Label {text:"Update the opening date and/or balance.";wrapMode:Text.Wrap;Layout.fillWidth:true}
            NavigableTextField {id:correctDate;objectName:"correctOpeningDate";placeholderText:s.dateFormat||"YYYY-MM-DD";Accessible.name:"Corrected opening date";onAccepted:acceptDialog(correctOpeningDialog)}
            NavigableTextField {id:correctAmount;onEditingFinished:resolveAmount(correctAmount);objectName:"correctOpeningAmount";placeholderText:"Amount";Accessible.name:"Corrected opening amount";onAccepted:acceptDialog(correctOpeningDialog)}
        }
        onAccepted:workflow.command("correct_opening",{operation:workflow.newIdentity(),date:correctDate.text,amount:correctAmount.text})
    }
    Dialog {
        id:currencyDialog;objectName:"currencyDialog";title:"Change account currency";modal:true;anchors.centerIn:parent;width:Math.min(window.width-48,520);standardButtons:Dialog.NoButton
        ColumnLayout {
            width:currencyDialog.availableWidth
            Label {text:"Choose the account currency.";wrapMode:Text.Wrap;Layout.fillWidth:true}
            Label {objectName:"currencyWarning";visible:!!s.currencyWarning;wrapMode:Text.Wrap;Layout.fillWidth:true;textFormat:Text.PlainText;color:theme.warning;text:"Amounts will stay the same. For example, CAD 20 becomes GBP 20."}
            ClearableSearchField {id:currencySearch;onClearRequested:clearSearch();objectName:"currencySearch";placeholderText:"Type a currency code";Layout.fillWidth:true;Accessible.name:"Currency code";onAccepted:if(confirmCurrency.enabled)confirmCurrency.clicked()}
            UsableComboBox {
                id:currencyPick;objectName:"currencyPick";Layout.fillWidth:true;Accessible.name:"Matching currencies"
                model:{var typed=currencySearch.text.toUpperCase();var all=workflow.currencies;var out=[];for(var i=0;i<all.length;i++) if(typed.length===0 || all[i].indexOf(typed)===0) out.push(all[i]);return out}
                onActivated:function(index){currencySearch.text=currencyPick.model[index]}
            }
            RowLayout {
                Button {id:confirmCurrency;objectName:"confirmCurrency";text:"Reassign";enabled:!workflow.busy && workflow.currencies.indexOf(currencySearch.text.toUpperCase())>=0;onClicked:{let code=currencySearch.text.toUpperCase();currencyDialog.close();workflow.command("reassign_currency",{operation:workflow.newIdentity(),currency:code})}}
                Button {objectName:"cancelCurrency";text:"Cancel";onClicked:currencyDialog.close()}
            }
        }
    }
    Dialog {
        id:importCategoriesDialog;objectName:"importCategoriesDialog";title:"Imported categories";modal:true;anchors.centerIn:parent
        width:Math.min(window.width-48,680);height:Math.min(window.height-48,520);standardButtons:Dialog.Close
        ColumnLayout {
            anchors.fill:parent
            Label {Layout.fillWidth:true;wrapMode:Text.Wrap;textFormat:Text.PlainText;text:s.importGroup && s.importGroup.started?"These categories were added from your file. Tax details are shown for reference only.":"Accepting transactions adds these categories, including unused ones. Tax details are shown for reference only."}
            ListView {
                id:importCategoryList;objectName:"importCategoryList";Layout.fillWidth:true;Layout.fillHeight:true;clip:true
                model:s.importGroup?s.importGroup.categories:[]
                ScrollBar.vertical:ScrollBar {policy:ScrollBar.AlwaysOn;implicitWidth:16;minimumSize:0.08}
                delegate:Label {
                    required property var modelData
                    width:importCategoryList.width-20;padding:6;wrapMode:Text.Wrap;textFormat:Text.PlainText
                    text:modelData.name+" · "+modelData.type+(modelData.warning?"\n"+modelData.warning:"")+(modelData.description?"\n"+modelData.description:"")+(modelData.taxSchedule?"\nTax schedule (source): "+modelData.taxSchedule:"")+(modelData.budget?"\nBudget (source): "+modelData.budget:"")+(modelData.taxRelated?"\nTax-related (source)":"")
                }
            }
        }
    }
    function accountImports(account) {
        return (s.undoableImports||[]).filter(item=>(item.accounts||[]).indexOf(account)>=0)
    }
    property string importUndoLabel: ""
    function requestImportUndo(item) {
        importUndoLabel=item.label
        workflow.command("preview_import_undo",{source:item.source})
    }
    function openImportUndoPicker() {
        importUndoAccount.currentIndex=Math.max(0,(s.accounts||[]).findIndex(a=>a.id===(s.accountId||"")))
        importUndoPicker.open()
    }
    Dialog {
        id:importUndoPicker;objectName:"importUndoPicker";title:"Undo import";modal:true;anchors.centerIn:parent
        width:Math.min(window.width-48,720);height:Math.min(window.height-48,480)
        ColumnLayout {anchors.fill:parent
            Label {text:"Account"}
            UsableComboBox {id:importUndoAccount;objectName:"importUndoAccount";Layout.fillWidth:true;model:s.accounts||[];textRole:"name";enabled:!workflow.busy}
            Label {text:"Imports not yet accepted, newest first.";Layout.fillWidth:true;wrapMode:Text.Wrap}
            ListView {id:importUndoList;objectName:"importUndoList";Layout.fillWidth:true;Layout.fillHeight:true;clip:true;spacing:6
                model:accountImports(((s.accounts||[])[importUndoAccount.currentIndex]||{}).id||"")
                ScrollBar.vertical:ScrollBar {policy:ScrollBar.AsNeeded}
                delegate:ItemDelegate {required property var modelData;required property int index;objectName:"importUndoChoice_"+index;width:ListView.view.width
                    palette.highlightedText:theme.text
                    text:(index===0?"Latest · ":"")+modelData.label;enabled:!workflow.busy
                    onClicked:requestImportUndo(modelData)
                }
                Label {anchors.centerIn:parent;visible:importUndoList.count===0;text:"No imports available to undo."}
            }
            Label {Layout.fillWidth:true;wrapMode:Text.Wrap;textFormat:Text.PlainText;visible:errorDialog===importUndoPicker && workflow.error.length>0;text:visible?workflow.error:"";color:theme.error}
            Button {text:"Close";enabled:!workflow.busy;onClicked:importUndoPicker.close()}
        }
    }
    Dialog {
        id:importUndoConfirm;objectName:"importUndoConfirm";title:"Undo this import?";modal:true;anchors.centerIn:parent;width:Math.min(window.width-48,640)
        closePolicy:workflow.busy?Popup.NoAutoClose:Popup.CloseOnEscape
        ColumnLayout {anchors.fill:parent
            Label {Layout.fillWidth:true;wrapMode:Text.Wrap;textFormat:Text.PlainText;text:importUndoLabel}
            Label {Layout.fillWidth:true;wrapMode:Text.Wrap;textFormat:Text.PlainText
                text:"New transactions to remove: "+((s.importUndoPreview||{}).transactions||0)+". Import problems to remove: "+((s.importUndoPreview||{}).failures||0)+". Transfer matches made by this import will be undone.\nAffected accounts: "+(((s.importUndoPreview||{}).accounts)||[]).map(function(id){let a=(s.accounts||[]).concat(s.archivedAccounts||[]).find(a=>a.id===id);return a?a.name:id}).join(", ")+"."}
            Label {Layout.fillWidth:true;wrapMode:Text.Wrap;textFormat:Text.PlainText;visible:errorDialog===importUndoConfirm && workflow.error.length>0;text:visible?workflow.error:"";color:theme.error}
            RowLayout {
                Button {objectName:"confirmImportUndo";text:"Undo import";enabled:!workflow.busy;onClicked:workflow.command("undo_import",{source:(s.importUndoPreview||{}).source,token:(s.importUndoPreview||{}).token})}
                Button {text:"Cancel";enabled:!workflow.busy;onClicked:importUndoConfirm.close()}
            }
        }
    }
    Dialog {
        id:discardDialog;objectName:"discardImportDialog";width:Math.min(window.width-48,640);title:"Discard new transactions in this account?";modal:true;anchors.centerIn:parent
        property string previewToken:""
        onOpened:previewToken=(s.rollback||{}).token||""
        standardButtons:Dialog.Ok|Dialog.Cancel
        Label {width:discardDialog.availableWidth;wrapMode:Text.Wrap;textFormat:Text.PlainText;text:"Remove "+((s.rollback||{}).transactions||0)+" transactions not yet accepted and "+((s.rollback||{}).failures||0)+" import problems from "+(s.name||"")+". Matching transactions created by SpentIt may also be deleted. Transactions kept by another account's import and all accepted transactions remain.\nAffected accounts: "+(((s.rollback||{}).accounts)||[]).map(function(id){let match=(s.accounts||[]).find(function(a){return a.id===id});return match?match.name:id}).join(", ")+"."}
        onAccepted:workflow.command("discard",{token:previewToken})
    }
    Dialog {
        id:closeAuditFailure;objectName:"closeAuditFailure";title:"Couldn’t finish checking the file";modal:true;anchors.centerIn:parent;width:540
        ColumnLayout {anchors.fill:parent
            Label {Layout.fillWidth:true;text:errorDialog===closeAuditFailure?workflow.error:"";wrapMode:Text.Wrap;textFormat:Text.PlainText}
            Label {Layout.fillWidth:true;text:"SpentIt couldn’t finish checking the file. It will check again next time you open it.";wrapMode:Text.Wrap}
            RowLayout {
                Button {text:"Return";onClicked:closeAuditFailure.close()}
                Button {text:"Quit anyway";onClicked:{if(!workflow.saveWindowPosition(window))return;closeAuditFailure.close();closingRequested=true;window.hide();workflow.command("close",{skipAudit:true})}}
            }
        }
    }
    Dialog {
        id:quitCommit;objectName:"quitCommitDialog";title:"Accept transactions before quitting?";modal:true;anchors.centerIn:parent;width:520
        Label {width:480;wrapMode:Text.Wrap;text:"Your transactions are already saved. Accept them in all accounts now, or leave them for review next time."}
        footer:DialogButtonBox {
            Button {objectName:"quitCommitYes";text:"Accept and quit";onClicked:{quitCommit.close();commitBeforeQuit=true;requestCommit()}}
            Button {objectName:"quitCommitNo";text:"Quit without accepting";onClicked:{quitCommit.close();beginQuit()}}
            Button {objectName:"quitCommitCancel";text:"Cancel";onClicked:quitCommit.close()}
        }
    }
    Shortcut {
        sequences:[StandardKey.Undo]
        enabled:!reviewSheet.visible && (!!s.canUndoManual || !!s.canUndoEdit || !!s.canUndoReview || !!s.canUndoTransferAssignment) && !workflow.busy && !(window.activeFocusItem && typeof window.activeFocusItem.undo === "function")
        onActivated:if(s.canUndoEdit || s.canUndoManual)undoEdit();else {if(s.canUndoTransferAssignment)captureLedgerView("category");workflow.command(s.canUndoTransferAssignment?"undo_transfer_assignment":"review_undo")}
    }
    property var exclusionEndpoint: ({})
    property var exclusionEditing: ({})
    property bool exclusionPreviewed: false
    function openLinkExclusion(row, rule) {
        exclusionEndpoint=row||({})
        exclusionEditing=rule||{identity:"",field:"payee",expression:row.payee||"",account:"0",regex:false,endpointAccount:row.account}
        exclusionPreviewed=false
        exclusionField.currentIndex=exclusionEditing.field==="memo"?1:0
        exclusionText.text=exclusionEditing.expression||""
        exclusionRegex.checked=!!exclusionEditing.regex
        exclusionAccount.checked=exclusionEditing.account!=="0"
        linkExclusionDialog.open()
    }
    Dialog {
        id:linkExclusionDialog;objectName:"linkExclusionDialog";title:"Don't suggest links";modal:true;anchors.centerIn:parent;width:Math.min(window.width-40,760);height:Math.min(window.height-40,620)
        onRejected:workflow.command("link_exclusion_cancel")
        contentItem:ColumnLayout {
            Label {text:"Hide suggested links when either transaction matches this condition. Transfers still waiting for a bank transaction remain in Review.";wrapMode:Text.Wrap;Layout.fillWidth:true}
            UsableComboBox {id:exclusionField;objectName:"exclusionField";model:["Payee","Memo (including split memos)"];onActivated:{exclusionPreviewed=false;if(!exclusionEditing.identity)exclusionText.text=currentIndex===0?(exclusionEndpoint.payee||""):(exclusionEndpoint.memo||((exclusionEndpoint.splits||[]).find(function(part){return !!part.memo})||{}).memo||"")}}
            NavigableTextField {id:exclusionText;objectName:"exclusionText";Layout.fillWidth:true;placeholderText:"Matching condition";onTextEdited:exclusionPreviewed=false;Accessible.name:"Link exclusion condition"}
            CheckBox {id:exclusionRegex;objectName:"exclusionRegex";text:"Regular expression";onToggled:exclusionPreviewed=false}
            CheckBox {id:exclusionAccount;objectName:"exclusionAccount";text:"Only in this account";onToggled:exclusionPreviewed=false}
            Label {text:"Matches the whole field. Capitalization is ignored; spaces and punctuation must match.";wrapMode:Text.Wrap;Layout.fillWidth:true}
            RowLayout {
                Button {text:"Preview";objectName:"previewLinkExclusion";enabled:!workflow.busy;onClicked:{exclusionPreviewed=true;workflow.command("link_exclusion_preview",{identity:exclusionEditing.identity,field:exclusionField.currentIndex===0?"payee":"memo",expression:exclusionText.text,regex:exclusionRegex.checked,account:exclusionAccount.checked?(exclusionEditing.endpointAccount||exclusionEditing.account!=="0" && exclusionEditing.account||s.accountId):"0",removed:!!exclusionEditing.removed})}}
                Label {text:exclusionPreviewed && (s.linkExclusionPreview||{}).ready?((s.linkExclusionPreview.comparisons||[]).length+" affected comparisons"):"Preview before saving"}
            }
            ListView {Layout.fillWidth:true;Layout.fillHeight:true;clip:true;model:exclusionPreviewed?((s.linkExclusionPreview||{}).comparisons||[]):[];ScrollBar.vertical:UsableScrollBar {} delegate:Label {required property string modelData;width:ListView.view.width;text:modelData;textFormat:Text.PlainText;wrapMode:Text.Wrap}}
            Label {text:errorDialog===linkExclusionDialog?workflow.error:"";visible:text.length>0;color:theme.error;wrapMode:Text.Wrap;Layout.fillWidth:true}
        }
        footer:DialogButtonBox {
            Button {text:exclusionEditing.removed?"Remove exclusion":"Save";objectName:"saveLinkExclusion";enabled:!workflow.busy && exclusionPreviewed && !!(s.linkExclusionPreview||{}).ready;onClicked:workflow.command("link_exclusion_save")}
            Button {text:"Cancel";onClicked:{workflow.command("link_exclusion_cancel");linkExclusionDialog.close()}}
        }
    }
    property bool reviewInspecting: false
    property bool pendingReviewInspection: false
    function inspectReviewTransaction(row) {
        guard(function(){pendingReviewInspection=true;reviewInspecting=true;workflow.command("select_account",{id:row.account,reveal:true,batch:row.batch,ordinal:row.ordinal})})
    }
    property real reviewScrollPosition: 0
    property var reviewPending: ({})
    property var reviewSurvivors: []
    property var reviewBatch: ({})
    property var reviewRowChoices: ({})
    property string reviewSelectionGeneration: String(s.reviewGeneration||"")
    onReviewSelectionGenerationChanged: {reviewBatch=({});reviewRowChoices=({});reviewSurvivor.close()}
    onCurrentPageChanged: {
        categoryVisitAdditions=[];categorySort="ascending"
        if(currentPage!=="review") {reviewBatch=({});reviewRowChoices=({});if(reviewPending.batchIntent)reviewSurvivor.close()}
        else if(s.reviewReady===false && !workflow.busy)workflow.command("review_load")
    }
    property var reviewViewport: ({})
    property bool reviewViewRestoring: false
    function reviewVisibleIndex() {
        let index=reviewQuestions.indexAt(1,reviewQuestions.contentY+1)
        return index>=0?index:reviewQuestions.indexAt(1,reviewQuestions.contentY+reviewQuestions.spacing+1)
    }
    function captureReviewView() {
        if(currentPage!=="review")return
        let index=reviewVisibleIndex()
        if(index<0)index=0
        let item=reviewQuestions.itemAtIndex(index)
        reviewViewport={keys:(s.reviewQuestions||[]).map(q=>q.key),index:index,offset:item?reviewQuestions.contentY-item.y:0}
    }
    function restoreReviewView() {
        let keys=reviewViewport.keys||[], rows=s.reviewQuestions||[], index=-1
        for(let i=reviewViewport.index||0;i<keys.length && index<0;i++)index=rows.findIndex(q=>q.key===keys[i])
        if(index<0)for(let i=(reviewViewport.index||0)-1;i>=0 && index<0;i--)index=rows.findIndex(q=>q.key===keys[i])
        reviewQuestions.forceLayout()
        if(index>=0){reviewQuestions.positionViewAtIndex(index,ListView.Begin);reviewQuestions.contentY+=reviewViewport.offset||0}
        else reviewQuestions.contentY=0
        reviewQuestions.contentY=Math.max(reviewQuestions.originY,Math.min(reviewQuestions.contentY,reviewQuestions.originY+Math.max(0,reviewQuestions.contentHeight-reviewQuestions.height)))
    }
    function setReviewBatch(args) {
        let values=Object.assign({},reviewBatch);values[args.question]=args;reviewBatch=values
    }
    function selectReviewGroup(question, chosen) {
        if(reviewBatch[question.key]){let values=Object.assign({},reviewBatch);delete values[question.key];reviewBatch=values;return}
        let args={question:question.key,generation:s.reviewGeneration,answer:"merge",selected:chosen,batchIntent:true}
        let manual=chosen.filter(row=>row.manual)
        let accepted=manual.length>0?manual:chosen.filter(row=>row.committed)
        if(accepted.length===1){args.survivor=accepted[0];setReviewBatch(args)}
        else {reviewPending=args;reviewSurvivors=manual.length>0?manual:chosen;reviewSurvivor.open()}
    }
    property string workspaceFeedback: "Ready"
    // One visible owner for a global operation error. Dialog validation stays
    // beside its controls; closing the dialog returns the error to the status line.
    readonly property var errorDialog: statusDetails.visible?statusDetails:
        importUndoConfirm.visible?importUndoConfirm:importUndoPicker.visible?importUndoPicker:closeAuditFailure.visible?closeAuditFailure:reviewDifference.visible?reviewDifference:
        linkExclusionDialog.visible?linkExclusionDialog:reapplyDialog.visible?reapplyDialog:
        bulkDialog.visible?bulkDialog:mergePreview.visible?mergePreview:
        changePreview.visible?changePreview:importMapping.visible?importMapping:null
    Dialog {id:statusDetails;objectName:"statusDetails";title:"Status details";modal:true;anchors.centerIn:parent;width:Math.min(720,window.width-40)
        contentItem:Label {text:errorDialog===statusDetails?workflow.error:"";textFormat:Text.PlainText;wrapMode:Text.Wrap}
        standardButtons:Dialog.Close
    }
    function answerReview(question, answer, chosen) {
        captureReviewView()
        reviewScrollPosition=reviewQuestions.contentY
        let args={question:question.key,generation:s.reviewGeneration,answer:answer,selected:chosen||[]}
        if(answer==="merge") {
            let manual=(chosen||[]).filter(row=>row.manual)
            let accepted=manual.length>0?manual:(chosen||[]).filter(function(row){return row.committed})
            if(manual.length===1)args.survivor=manual[0]
            if(accepted.length!==1){reviewPending=args;reviewSurvivors=manual.length>0?manual:chosen;reviewSurvivor.open();return}
        }
        workflow.command("review_answer",args)
    }

    Dialog {
        id:reviewSurvivor;objectName:"reviewSurvivor";title:"Choose the transaction to keep";modal:true;anchors.centerIn:parent;width:Math.min(window.width-64,720)
        contentItem:ColumnLayout {
            Repeater {model:reviewSurvivors;delegate:Button {
                required property var modelData;required property int index;objectName:"reviewKeep_"+index;Layout.fillWidth:true
                text:modelData.date+" · "+modelData.payee+" · "+modelData.total+(modelData.committed?" · Accepted":"")+" — Keep this transaction"
                onClicked:{let args=Object.assign({},reviewPending,{survivor:modelData});reviewSurvivor.close();if(args.batchIntent)setReviewBatch(args);else workflow.command("review_answer",args)}
            }}
        }
        standardButtons:Dialog.Cancel
    }
    Dialog {
        id:reviewDifference;objectName:"reviewDifference";title:"Link the matching amount";modal:true;anchors.centerIn:parent;width:Math.min(window.width-64,620)
        onOpened:reviewRemainderCategory.currentIndex=0
        property var preview:s.reviewLinkPreview||({})
        onPreviewChanged: {if(preview.common)open();else close()}
        onRejected:workflow.command("review_cancel_link")
        contentItem:ColumnLayout {
            Label {text:"Link "+(reviewDifference.preview.common||"")+" and categorise "+(reviewDifference.preview.remainder||"")+" as";wrapMode:Text.Wrap;Layout.fillWidth:true}
            UsableComboBox {id:reviewRemainderCategory;objectName:"reviewRemainderCategory";Layout.fillWidth:true;textRole:"path";model:[{id:"",path:"Uncategorized"}].concat((s.categories||[]).filter(function(c){return c.assignable}));Accessible.name:"Category for difference"}
            Label {text:errorDialog===reviewDifference?workflow.error:"";visible:text.length>0;color:theme.error;wrapMode:Text.Wrap;Layout.fillWidth:true}
        }
        footer:DialogButtonBox {
            Button {text:"Link";objectName:"reviewDifferenceYes";enabled:!workflow.busy;onClicked:{let args=Object.assign({},reviewDifference.preview.args,{splitDifference:true,category:reviewRemainderCategory.model[reviewRemainderCategory.currentIndex].id});workflow.command("review_answer",args)}}
            Button {text:"Cancel";objectName:"reviewDifferenceNo";enabled:!workflow.busy;onClicked:workflow.command("review_cancel_link")}
        }
    }
    ColumnLayout {
        anchors.fill:parent;anchors.margins:12;spacing:8
        RowLayout {
            visible:!!s.active
            Button {objectName:"sidebarToggle";text:sidebarVisible?"Hide sidebar":"Show sidebar";visible:!!s.active;onClicked:sidebarVisible=!sidebarVisible;Accessible.name:text+" (Ctrl+Shift+B)"}
            Button {objectName:"navigationBack";text:"Back";visible:!!s.active;enabled:!workflow.busy && navigationIndex>0;onClicked:traverseNavigation(-1)}
            Button {objectName:"navigationForward";text:"Forward";visible:!!s.active;enabled:!workflow.busy && navigationIndex+1<navigation.length;onClicked:traverseNavigation(1)}
            Label {objectName:"workspaceTitle";textFormat:Text.PlainText;text:!s.active ? "Open or create a file" : currentPage==="ledger" ? s.name+" · "+s.currency : ({review:"Review",categories:"Categories",rules:"Rules",reports:"Reports",history:"Change history",settings:"Settings"})[currentPage] || "SpentIt";font.pixelSize:20;elide:Text.ElideRight;Layout.fillWidth:true}
            BusyIndicator {
                id:activityIndicator;objectName:"activityIndicator"
                running:workflow.busy;visible:running
                implicitWidth:36;implicitHeight:36;padding:0
                Accessible.name:"Working";Accessible.role:Accessible.Indicator
                contentItem:Canvas {
                    id:activityArc
                    property color arcColor:theme.focus
                    property color trackColor:theme.divider
                    onArcColorChanged:requestPaint()
                    onTrackColorChanged:requestPaint()
                    onPaint:{
                        let ctx=getContext("2d")
                        ctx.reset();ctx.lineWidth=5;ctx.lineCap="round"
                        let radius=Math.min(width,height)/2-3
                        ctx.beginPath();ctx.strokeStyle=trackColor
                        ctx.arc(width/2,height/2,radius,0,2*Math.PI);ctx.stroke()
                        ctx.beginPath();ctx.strokeStyle=arcColor
                        ctx.arc(width/2,height/2,radius,-Math.PI/2,Math.PI);ctx.stroke()
                    }
                    // Render-thread rotation keeps the painted arc moving without
                    // a JavaScript repaint for each animation frame.
                    RotationAnimator on rotation {
                        from:0;to:360;duration:850;loops:Animation.Infinite
                        running:activityIndicator.running && activityIndicator.visible
                    }
                }
            }
        }
        Item {visible:!s.active && (startupPending || workflow.openingDocument);Layout.fillWidth:true;Layout.fillHeight:true
            ColumnLayout {anchors.centerIn:parent;spacing:24
                Image {source:"qrc:/spentit/images/SpentIt_light_1024.png";Layout.preferredWidth:256;Layout.preferredHeight:256;Layout.alignment:Qt.AlignHCenter;fillMode:Image.PreserveAspectFit}
                Label {text:"Where did my money go?";font.pixelSize:24;Layout.alignment:Qt.AlignHCenter}
                Button {objectName:"startupCancelOperation";text:"Cancel";Layout.alignment:Qt.AlignHCenter;visible:workflow.canCancel;enabled:!workflow.cancellationRequested;onClicked:workflow.requestCancel()}
            }
            Label {anchors.left:parent.left;anchors.right:parent.right;anchors.bottom:parent.bottom;height:40;verticalAlignment:Text.AlignVCenter;text:workflow.cancellationRequested?"Cancellation requested…":workflow.phase;textFormat:Text.PlainText;elide:Text.ElideRight}
        }
        UsableScrollView {
            id:welcomeScroll;objectName:"welcomeScreen";visible:!s.active && !startupPending && !workflow.openingDocument;Layout.fillWidth:true;Layout.fillHeight:true;clip:true
            contentWidth:availableWidth
            ColumnLayout {
                width:Math.min(welcomeScroll.availableWidth-48,660);x:(welcomeScroll.availableWidth-width)/2
                spacing:18
                // Keep the logo responsive to system appearance changes.
                Item {Layout.preferredHeight:Math.max(28,Math.min(112,window.height*0.13));Layout.fillWidth:true}
                Image {
                    objectName: "welcomeLogo"
                    Layout.preferredWidth:128;Layout.preferredHeight:128;Layout.alignment:Qt.AlignHCenter
                    source: "qrc:/spentit/images/SpentIt_" + (Application.styleHints.colorScheme === Qt.Dark ? "dark" : "light") + "_256.png"
                    sourceSize.width:256;sourceSize.height:256;fillMode:Image.PreserveAspectFit
                    Accessible.ignored:true
                }
                Label {text:"SpentIt";font.pixelSize:36;font.bold:true;Layout.alignment:Qt.AlignHCenter}
                Label {text:"Where did my money go?";font.pixelSize:18;Layout.alignment:Qt.AlignHCenter}
                Item {Layout.preferredHeight:16}
                Label {text:"Choose your SpentIt file";font.pixelSize:16;Layout.fillWidth:true}
                NavigableTextField {id:location;objectName:"datasetPath";text:s.path || workflow.suggestedPath;Layout.fillWidth:true;placeholderText:"File path";Accessible.name:"File path"
                    enabled:!workflow.busy
                    onTextEdited:{unlockPassword.clear();if(!settingUp)probeTimer.restart()}
                    onAccepted:if(!settingUp)probeTimer.restart()
                }
                RowLayout {
                    spacing:12;enabled:!workflow.busy
                    Button {objectName:"browseDataset";text:"Browse…";onClicked:{openDialog.currentFolder=workflow.documentsFolder;openDialog.open()}}
                    Button {objectName:"newDataset";text:"New file";onClicked:{settingUp=true;location.text=workflow.suggestedPath;accountName.text="SpentIt";protection.checked=true;defaultCategories.checked=true;setupDateOverridden=false;password.clear();confirmation.clear();currency.currentIndex=workflow.currencies.indexOf("CAD");suggestSetupDate();workflow.command("prepare_create",{path:location.text})}}
                    Button {objectName:"openPublicDataset";text:"Open file";visible:!settingUp && s.path===location.text && !!s.canOpen && !s.unlockRequired;onClicked:openDataset()}
                }
                RowLayout {
                    visible:!settingUp && s.path===location.text && !!s.unlockRequired;Layout.fillWidth:true;enabled:!workflow.busy;spacing:12
                    NavigableTextField {id:unlockPassword;objectName:"unlockPassword";echoMode:TextInput.Password;placeholderText:"Password to open dataset";Layout.fillWidth:true;onAccepted:openDataset();Accessible.name:"Dataset password"}
                    Button {objectName:"openProtectedDataset";text:"Open file";onClicked:openDataset()}
                }
                Label {objectName:"welcomeError";text:errorDialog?"":workflow.error;visible:text.length>0;textFormat:Text.PlainText;color:theme.error;wrapMode:Text.Wrap;Layout.fillWidth:true;Accessible.name:text}
                GroupBox {
                    title:"Create a new dataset";visible:settingUp;Layout.fillWidth:true;enabled:!workflow.busy
                    ColumnLayout {
                        anchors.fill:parent;spacing:12
                        Button {text:"Browse new location…";onClicked:{createDialog.currentFolder=workflow.documentsFolder;createDialog.selectedFile=workflow.suggestedFile;createDialog.open()}}
                        Label {text:"Account name"}
                        NavigableTextField {id:accountName;objectName:"initialAccountName";text:"SpentIt";placeholderText:"Account name";Layout.fillWidth:true;Accessible.name:"Account name"}
                        RowLayout {Label {text:"Home / account currency"} UsableComboBox {id:currency;objectName:"setupCurrency";model:workflow.currencies;currentIndex:workflow.currencies.indexOf("CAD");Accessible.name:"Currency";onActivated:suggestSetupDate()}}
                        Label {text:"Date format"}
                        UsableComboBox {id:setupDate;objectName:"setupDateFormat";Layout.preferredWidth:230;model:datePresets;Accessible.name:"Date format";onActivated:{setupDateOverridden=true;if(currentIndex<6)customDate.text=currentText}}
                        NavigableTextField {id:customDate;objectName:"customDateFormat";visible:setupDate.currentIndex===6;text:"YYYY-MM-DD";placeholderText:"YYYY-MM-DD";maximumLength:10;Accessible.name:"Custom date format";onTextEdited:setupDateOverridden=true}
                        Label {text:"Example: "+workflow.dateFormatExample(setupDate.currentIndex===6?customDate.text:setupDate.currentText);wrapMode:Text.Wrap;Layout.fillWidth:true}
                        CheckBox {id:defaultCategories;objectName:"defaultCategories";text:"Use default categories";checked:true}
                        CheckBox {id:protection;objectName:"passwordProtection";text:"Password protection";checked:true}
                        NavigableTextField {id:password;objectName:"newDatasetPassword";echoMode:TextInput.Password;visible:protection.checked;placeholderText:"Password";Layout.fillWidth:true;Accessible.name:"New password"}
                        NavigableTextField {id:confirmation;objectName:"confirmDatasetPassword";echoMode:TextInput.Password;visible:protection.checked;placeholderText:"Confirm password";Layout.fillWidth:true;Accessible.name:"Confirm password"}
                        Label {textFormat:Text.PlainText;visible:protection.checked;text:"Keep your password safe. If you forget it, you cannot open this file or its backups.";wrapMode:Text.Wrap;Layout.fillWidth:true}
                        Label {textFormat:Text.PlainText;visible:!protection.checked;text:"Anyone with this file or a backup can read your financial information.";wrapMode:Text.Wrap;Layout.fillWidth:true;color:theme.error}
                        Button {objectName:"createDataset";text:"Create file";enabled:currency.currentIndex>=0;onClicked:createDataset(false)}
                    }
                }
                Item {Layout.preferredHeight:32}
            }
        }
        SplitView {
            id:shell;objectName:"durableShell"
            visible:!!s.active;Layout.fillWidth:true;Layout.fillHeight:true
            orientation:Qt.Horizontal
            Pane {
                id:sidebar;objectName:"accountSidebar";visible:sidebarVisible
                SplitView.preferredWidth:200;SplitView.minimumWidth:140;SplitView.maximumWidth:Math.max(140,Math.min(360,shell.width*0.35))
                ColumnLayout {
                    anchors.fill:parent
                    RowLayout {
                        Layout.fillWidth:true;spacing:6
                        Label {text:"Accounts";font.bold:true;font.pixelSize:14;Layout.fillWidth:true;Layout.minimumWidth:0;elide:Text.ElideRight}
                        Button {objectName:"newAccount";text:"+ New Account";font.pixelSize:12;leftPadding:6;rightPadding:6;topPadding:4;bottomPadding:4;Layout.preferredWidth:implicitContentWidth+12;Layout.preferredHeight:28;visible:!addingAccount;enabled:!workflow.busy && !!s.nextAccountId;onClicked:guard(function(){accountCreationId=s.nextAccountId;accountCreationDate=workflow.todayDisplay();newAccountName.clear();addingAccount=true;Qt.callLater(function(){newAccountName.forceActiveFocus();revealAccountEntry()})})}
                    }
                    SplitView {
                        orientation:Qt.Vertical;Layout.fillWidth:true;Layout.fillHeight:true
                        UsableScrollView {
                        id:accountScroll;objectName:"accountScroll";SplitView.fillHeight:true;SplitView.minimumHeight:120;contentWidth:availableWidth;clip:true
                        ColumnLayout {
                            id:accountRows;width:accountScroll.availableWidth
                            Rectangle {
                                parent:window.contentItem;z:1001
                                x:accountDropPoint.x;y:accountDropPoint.y-1.5
                                width:accountScroll.availableWidth;height:3
                                color:theme.focus;radius:1.5
                                visible:accountDragRow!==null && accountDropIndex>=0
                                objectName:"accountDropIndicator"
                            }
                            Repeater {
                                id:accountRepeater;model:s.sidebarAccounts||s.accounts||[]
                                onModelChanged:cancelAccountDrag()
                                Item {
                                    id:accountRow;required property var modelData
                                    objectName:"accountRow_"+modelData.id
                                    Layout.fillWidth:true;implicitHeight:accountRowContents.implicitHeight
                                    opacity:accountDragRow===accountRow?0.35:1
                                    ColumnLayout {
                                    id:accountRowContents;width:parent.width;spacing:0
                                    Button {
                                        id:accountRowButton
                                        objectName:modelData.id===s.accountId?"accountNavigation":"accountNavigation_"+modelData.id
                                        Layout.fillWidth:true;text:modelData.name;enabled:!workflow.busy
                                        highlighted:modelData.id===s.accountId && currentPage==="ledger"
                                        background:Rectangle {
                                            implicitWidth:100;implicitHeight:40
                                            color:parent.highlighted?theme.selection:parent.hovered?theme.alternate:theme.button
                                            border.width:parent.visualFocus?2:0;border.color:theme.focus
                                        }
                                        contentItem:Label {text:parent.text;color:parent.enabled?theme.text:theme.disabled;textFormat:Text.PlainText;elide:Text.ElideRight;verticalAlignment:Text.AlignVCenter}
                                        Accessible.name:"Open account "+modelData.name
                                        onClicked:chooseAccount(modelData.id)
                                        Keys.onMenuPressed:openAccountMenu(modelData.id,this,0,height)
                                        Keys.onPressed:function(event) {if(event.key===Qt.Key_F10 && (event.modifiers & Qt.ShiftModifier)){openAccountMenu(modelData.id,this,0,height);event.accepted=true}}
                                    }
                                    Label {
                                        objectName:"accountBalance_"+modelData.id
                                        Layout.fillWidth:true;Layout.minimumHeight:28;text:sidebarBalanceText(modelData)
                                        topPadding:2;bottomPadding:2;leftPadding:4;rightPadding:4
                                        wrapMode:Text.Wrap;textFormat:Text.PlainText;color:theme.text
                                        Accessible.name:text
                                    }
                                    }
                                    MouseArea {
                                        anchors.fill:parent;acceptedButtons:Qt.LeftButton|Qt.RightButton
                                        enabled:!workflow.busy;preventStealing:true
                                        property point startPoint
                                        property bool dragged:false
                                        cursorShape:accountDragRow===accountRow?Qt.ClosedHandCursor:Qt.ArrowCursor
                                        onPressed:function(mouse){startPoint=mapToItem(window.contentItem,mouse.x,mouse.y);dragged=false}
                                        onPositionChanged:function(mouse){
                                            if(!(pressedButtons & Qt.LeftButton))return
                                            const point=mapToItem(window.contentItem,mouse.x,mouse.y)
                                            if(!dragged && Math.hypot(point.x-startPoint.x,point.y-startPoint.y)>=Qt.styleHints.startDragDistance){
                                                dragged=true;accountDragOffset=accountRow.mapFromItem(window.contentItem,startPoint.x,startPoint.y);accountDragRow=accountRow
                                            }
                                            if(accountDragRow===accountRow)moveAccountDrag(point)
                                        }
                                        onReleased:function(mouse){if(dragged)finishAccountDrag()}
                                        onCanceled:cancelAccountDrag()
                                        onClicked:function(mouse){
                                            if(dragged)return
                                            if(mouse.button===Qt.RightButton)openAccountMenu(modelData.id,accountRow,mouse.x,mouse.y)
                                            else {accountRowButton.forceActiveFocus();chooseAccount(modelData.id)}
                                        }
                                    }
                                }
                            }
                            NavigableTextField {
                                id:newAccountName;objectName:"newAccountName";visible:addingAccount;Layout.fillWidth:true;enabled:!workflow.busy
                                placeholderText:"Account name · Enter";Accessible.name:"New account name"
                                onAccepted:if(text.length>0)workflow.command("create_account",{id:accountCreationId,name:text,currency:s.homeCurrency,date:accountCreationDate})
                                Keys.onEscapePressed:addingAccount=false
                            }
                            Button {text:"Cancel";visible:addingAccount;enabled:!workflow.busy;onClicked:addingAccount=false}
                            Label {text:"Archived";font.bold:true;visible:(s.archivedAccounts||[]).length>0}
                            Repeater {
                                model:s.archivedAccounts||[]
                                ColumnLayout {
                                    required property var modelData
                                    Layout.fillWidth:true;spacing:0
                                    Button {
                                        objectName:"archivedAccount_"+modelData.id
                                        Layout.fillWidth:true;text:modelData.name;enabled:!workflow.busy
                                        highlighted:modelData.id===s.accountId
                                        background:Rectangle {
                                            implicitWidth:100;implicitHeight:40
                                            color:parent.highlighted?theme.selection:parent.hovered?theme.alternate:theme.button
                                            border.width:parent.visualFocus?2:0;border.color:theme.focus
                                        }
                                        contentItem:Label {text:parent.text;color:parent.enabled?theme.text:theme.disabled;textFormat:Text.PlainText;elide:Text.ElideRight;verticalAlignment:Text.AlignVCenter}
                                        Accessible.name:"Open archived account "+modelData.name
                                        onClicked:chooseAccount(modelData.id)
                                        Keys.onMenuPressed:openAccountMenu(modelData.id,this,0,height)
                                        Keys.onPressed:function(event) {if(event.key===Qt.Key_F10 && (event.modifiers & Qt.ShiftModifier)){openAccountMenu(modelData.id,this,0,height);event.accepted=true}}
                                        MouseArea {
                                            anchors.fill:parent;acceptedButtons:Qt.RightButton
                                            onClicked:function(mouse){openAccountMenu(modelData.id,parent,mouse.x,mouse.y)}
                                        }
                                    }
                                    Label {
                                        objectName:"accountBalance_"+modelData.id
                                        Layout.fillWidth:true;Layout.minimumHeight:28;text:sidebarBalanceText(modelData)
                                        topPadding:2;bottomPadding:2;leftPadding:4;rightPadding:4
                                        wrapMode:Text.Wrap;textFormat:Text.PlainText;color:theme.text
                                        Accessible.name:text
                                    }
                                }
                            }
                        }
                        }

                    }
                    Button {objectName:"importStatement";text:"Import…";Layout.fillWidth:true;enabled:!workflow.busy;onClicked:guard(function(){importTarget="";importDialog.open()})}
                    Button {text:"Import categories…";Layout.fillWidth:true;visible:(s.importCategoryDecisions||[]).length>0;enabled:!workflow.busy;onClicked:showImportCategories()}
                    Button {objectName:"reviewNavigation";text:s.reviewReady===false?"Review":"Review ("+(s.reviewCount||0)+")";Layout.fillWidth:true;enabled:!workflow.busy;highlighted:currentPage==="review";onClicked:reviewSheet.open()}
                    Button {objectName:"categoryNavigation";text:"Categories";Layout.fillWidth:true;visible:!!s.categoriesSupported;enabled:!workflow.busy;onClicked:guard(function(){navigate("categories");workflow.command("category_list")})}
                    Button {objectName:"rulesNavigation";text:"Rules";Layout.fillWidth:true;visible:!!s.active;enabled:!workflow.busy;highlighted:currentPage==="rules";onClicked:guard(function(){navigate("rules");if(s.rulesSupported && !rulesDirty)workflow.command("rules_load")})}
                    Button {objectName:"reportsNavigation";text:"Reports";Layout.fillWidth:true;enabled:!workflow.busy;highlighted:currentPage==="reports";onClicked:guard(function(){navigate("reports");if(!reportsWorkspace.dirty)workflow.command("report_load")})}
                    Button {objectName:"historyNavigation";text:"Change history";Layout.fillWidth:true;visible:!!s.committedChangesSupported;enabled:!workflow.busy;onClicked:guard(function(){navigate("history");workflow.command("history")})}
                    Button {objectName:"sidebarSettings";text:"Settings";Layout.fillWidth:true;enabled:!workflow.busy;highlighted:currentPage==="settings";onClicked:navigate("settings")}
                }
            }
            Item {
                SplitView.fillWidth:true
                Rectangle {
                    id:workspaceStatus;objectName:"workspaceStatus";anchors.left:parent.left;anchors.right:parent.right;anchors.bottom:parent.bottom;height:40
                    color:palette.window
                    Rectangle {anchors.top:parent.top;width:parent.width;height:1;color:palette.mid}
                    RowLayout {anchors.fill:parent;anchors.margins:4
                        Label {id:workspaceStatusText;objectName:"operationPhase";Layout.fillWidth:true;Layout.minimumWidth:0
                            textFormat:Text.PlainText;elide:Text.ElideRight
                            text:workflow.busy?(workflow.cancellationRequested?"Cancellation requested…":workflow.phase):(workflow.error?(errorDialog?"":workflow.error):workspaceFeedback)
                            color:!workflow.busy && workflow.error && !errorDialog?theme.error:palette.windowText
                            Accessible.name:text;onTextChanged:function(text){if(text.length)Accessible.announce(text)}
                            ToolTip.visible:statusHover.hovered && truncated;ToolTip.text:text
                            HoverHandler {id:statusHover}
                        }
                        Button {objectName:"cancelOperation";text:"Cancel";visible:workflow.busy && workflow.canCancel;enabled:!workflow.cancellationRequested;onClicked:workflow.requestCancel()}
                        Button {objectName:"statusDetailsButton";text:"Details";visible:!workflow.busy && !!workflow.error && !errorDialog;onClicked:statusDetails.open()}
                    }
                }
                Item {
                    id:workspaceBody
                    anchors.left:parent.left;anchors.right:parent.right;anchors.top:parent.top;anchors.bottom:workspaceStatus.top;anchors.bottomMargin:4
                Item {
        id:reviewSheet;objectName:"reviewSheet";anchors.fill:parent;visible:currentPage==="review"
        property string title:s.reviewReady===false?"Review":"Review · "+(s.reviewCount||0)+" remaining"
        function open(){navigate("review");if(currentPage==="review" && s.reviewReady===false && !workflow.busy)workflow.command("review_load");reviewQuestions.forceActiveFocus()}
        function close(){reviewBatch=({});reviewRowChoices=({});navigate("ledger")}
        ColumnLayout {anchors.fill:parent;spacing:4
        ColumnLayout {
            Layout.fillWidth:true
            RowLayout {Layout.fillWidth:true;Layout.margins:12
                Label {text:reviewSheet.title;font.pixelSize:20;Layout.fillWidth:true}
                Button {objectName:"chooseImportUndo";text:"Undo import…";enabled:!workflow.busy && (s.undoableImports||[]).length>0;onClicked:guard(openImportUndoPicker)}
                Button {objectName:"commitImport";text:"Accept transactions";enabled:!workflow.busy && (s.newTransactions||0)>0;onClicked:guard(requestCommit)}
                Button {objectName:"mergeSelectedMatches";text:"Merge "+Object.keys(reviewBatch).length+" selected matches";enabled:!workflow.busy && Object.keys(reviewBatch).length>0
                    onClicked:{captureReviewView();workflow.command("review_batch",{groups:Object.keys(reviewBatch).map(key=>reviewBatch[key])})}}
                Button {text:"Clear selection";enabled:!workflow.busy && Object.keys(reviewBatch).length>0;onClicked:reviewBatch=({})}
            }
        }

        UsableScrollView {id:repairScroll;Layout.fillWidth:true;Layout.preferredHeight:Math.min(150,repairControls.implicitHeight);visible:repairControls.implicitHeight>0;enabled:!workflow.busy;contentWidth:availableWidth
            ColumnLayout {id:repairControls;width:repairScroll.availableWidth
                Label {visible:!!s.pending && !!s.pending.diagnostics;text:s.pending?s.pending.diagnostics||"":"";Layout.fillWidth:true;wrapMode:Text.Wrap;textFormat:Text.PlainText}
            Label {objectName:"singleRepairProgress";visible:!!s.pending && !s.importGroup && (s.pending.repairs>0 || s.pending.unsupported>0);Layout.fillWidth:true;wrapMode:Text.Wrap;textFormat:Text.PlainText
                text:s.pending?"Repairs: "+s.pending.repairsDone+" / "+s.pending.repairs+" records ("+s.pending.amountRepairs+" need amount repair). Unsupported records/sections: "+s.pending.unsupported+". "+(s.pendingProtected?"Transfers with an accepted transaction in the other account cannot be discarded.":"Use the account menu to discard new transactions."):""}
            Label {objectName:"groupRepairProgress";visible:!!s.importGroup && (s.importGroup.repairs>0 || s.importGroup.unresolved>0);Layout.fillWidth:true;wrapMode:Text.Wrap;textFormat:Text.PlainText
                text:s.importGroup?"Whole import repairs: "+s.importGroup.done+" / "+s.importGroup.repairs+" records ("+s.importGroup.amountRepairs+" need amount repair). Unsupported records/sections: "+s.importGroup.blocked+". Outstanding review items: "+s.importGroup.unresolved+". Review each account before accepting transactions.":""}
            RowLayout {
                visible:!!s.importGroup;Layout.fillWidth:true
                Button {objectName:"resolveMappedTransfers";text:"Resolve mapped transfers";enabled:!s.readOnly && !processingIncomplete && !!s.importGroup && s.importGroup.unresolvedTransfers>0;onClicked:guard(function(){workflow.command("group_resolve_transfers",{group:s.importGroup.id})})}
                Button {objectName:"showImportBlockers";text:"What’s blocking commit? ("+(s.importGroup?s.importGroup.unresolved:0)+")";enabled:!!s.importGroup && s.importGroup.unresolved>0;onClicked:importBlockersDialog.open()}
            }
            Button {objectName:"reviewImportedCategories";visible:!!s.importGroup && s.importGroup.categories.length>0;text:"Review imported categories ("+(s.importGroup?s.importGroup.categories.length:0)+")…";onClicked:importCategoriesDialog.open()}
            ColumnLayout {
                visible:!!s.groupCreated;Layout.fillWidth:true
                Label {text:"Reported balance: "+(s.groupReportedBalance||"")+" · Reviewed change: "+(s.groupReviewedChange||"")+" "+(s.currency||"")+" · Proposed opening: "+(s.groupProposedOpening||"");textFormat:Text.PlainText;wrapMode:Text.Wrap;Layout.fillWidth:true}
                RowLayout {
                    UsableComboBox {id:balanceMeaning;objectName:"groupBalanceMeaning";Layout.preferredWidth:330;model:["Choose balance meaning","Zero opening (balance agrees)","Treat as opening balance","Treat as closing balance","Ignore reported balance"] ;Accessible.name:"Reported balance meaning"}
                    NavigableTextField {id:groupOpeningDate;objectName:"groupOpeningDate";text:s.openingDate||"";placeholderText:s.dateFormat||"YYYY-MM-DD";Accessible.name:"Confirmed opening date"}
                    Button {objectName:"confirmGroupOpening";text:"Calculate opening balance";enabled:!s.readOnly && !!s.groupCanReviewBalance && balanceMeaning.currentIndex>0;onClicked:guard(function(){workflow.command("group_balance",{group:s.importGroup.id,source:s.importGroup.sourceAccount,revision:s.pending.revision,choice:balanceMeaning.currentIndex,date:groupOpeningDate.text})})}
                }
                Label {text:"The opening stays staged until this account first commits. Existing mapped account openings are never replaced.";wrapMode:Text.Wrap;Layout.fillWidth:true}
            }
            }
        }
        Item {Layout.fillWidth:true;Layout.preferredHeight:30;clip:true
            RowLayout {x:-reviewQuestions.contentX;
                width:reviewQuestions.contentWidth-24;height:30
                Item {Layout.preferredWidth:32}
                Label {text:"Date";font.bold:true;Layout.preferredWidth:95}
                Label {text:"Account";font.bold:true;Layout.preferredWidth:105}
                Label {text:"Payee";font.bold:true;Layout.fillWidth:true;Layout.minimumWidth:100}
                Label {text:"Category / splits";font.bold:true;Layout.preferredWidth:145}
                Label {text:"Withdrawal";font.bold:true;Layout.preferredWidth:100;horizontalAlignment:Text.AlignRight}
                Label {text:"Deposit";font.bold:true;Layout.preferredWidth:100;horizontalAlignment:Text.AlignRight}
                Label {text:"";Layout.preferredWidth:40}
                Item {Layout.preferredWidth:100}
                Item {Layout.preferredWidth:100}
            }
        }
        ListView {
            Layout.fillWidth:true;Layout.fillHeight:true
            id:reviewQuestions;objectName:"reviewQuestions";
            activeFocusOnTab:true;Accessible.role:Accessible.List;Accessible.name:"Review questions"
            Keys.onPressed:function(event){
                if(event.key===Qt.Key_Up || event.key===Qt.Key_Down){currentIndex=Math.max(0,Math.min(count-1,currentIndex+(event.key===Qt.Key_Down?1:-1)));positionViewAtIndex(currentIndex,ListView.Contain);event.accepted=true}
                else if((event.key===Qt.Key_Return || event.key===Qt.Key_Enter) && currentItem && currentItem.modelData.rows.length){inspectReviewTransaction(currentItem.modelData.rows[0]);event.accepted=true}
            }
            clip:true;spacing:4;contentWidth:Math.max(width,1000);model:s.reviewQuestions||[]
            onModelChanged:{reviewViewRestoring=true;Qt.callLater(function(){restoreReviewView();reviewViewRestoring=false})}
            onMovementEnded:captureReviewView()
    Shortcut {objectName:"reviewUndoShortcut";
        sequences:[StandardKey.Undo]
        enabled:reviewSheet.visible && !reviewDifference.visible && !reviewSurvivor.visible && !!s.canUndoReview && !workflow.busy && !(window.activeFocusItem && typeof window.activeFocusItem.undo === "function")
        onActivated:{if(s.canUndoTransferAssignment)captureLedgerView("category");workflow.command(s.canUndoTransferAssignment?"undo_transfer_assignment":"review_undo")}
    }
            ScrollBar.vertical:UsableScrollBar {}
            ScrollBar.horizontal:UsableScrollBar {}
            Label {anchors.centerIn:parent;visible:reviewQuestions.count===0;text:s.reviewReady===false?"Review has not loaded yet.":"Nothing left to review."}
            delegate:Item {
                id:questionCard;required property var modelData;required property int index;width:reviewQuestions.contentWidth-24;height:questionContent.implicitHeight
                property var chosen: reviewRowChoices[modelData.key]||[]
                property int counterpart: -1
                property string leftAllocation: ""
                property string rightAllocation: ""
                function selectCounterpart(row) {
                    counterpart=row
                    const candidate=row>=1?modelData.rows[row]:({})
                    leftAllocation=candidate.suggestedLeftAllocation||""
                    rightAllocation=candidate.suggestedRightAllocation||""
                }
                onModelDataChanged:{
                    let selected=-1
                    if(modelData.kind==="link") {
                        if(modelData.rows.length===2)selected=1
                        else {
                            const matches=[]
                            for(let i=1;i<modelData.rows.length;++i)if(modelData.rows[i].sameDate)matches.push(i)
                            if(matches.length===1)selected=matches[0]
                        }
                    }
                    selectCounterpart(selected)
                }
                Rectangle {anchors.fill:parent;color:questionCard.index%2?theme.alternate:theme.surface}
                function toggle(row, checked){let values=chosen.slice();if(checked)values.push(row);else values=values.filter(function(v){return v.key!==row.key});let choices=Object.assign({},reviewRowChoices);choices[modelData.key]=values;reviewRowChoices=choices;let groups=Object.assign({},reviewBatch);delete groups[modelData.key];reviewBatch=groups}
                ColumnLayout {id:questionContent;width:parent.width;spacing:4
                Rectangle {Layout.fillWidth:true;Layout.preferredHeight:2;color:theme.border}
                Item {Layout.preferredHeight:6}
                Label {text:questionCard.modelData.accountName||"Import problems";visible:!!questionCard.modelData.separator;font.bold:true;font.pixelSize:16;Layout.fillWidth:true}
                RowLayout {
                    Layout.fillWidth:true
                    CheckBox {objectName:"reviewGroupSelect_"+questionCard.index;text:reviewBatch[questionCard.modelData.key]?"Selected match "+(Object.keys(reviewBatch).indexOf(questionCard.modelData.key)+1):"Include match";visible:questionCard.modelData.kind==="same"
                        checked:!!reviewBatch[questionCard.modelData.key]
                        enabled:!workflow.busy && (questionCard.modelData.rows.length===2 || questionCard.chosen.length>=2)
                        onClicked:{
                            selectReviewGroup(questionCard.modelData,questionCard.modelData.rows.length===2?questionCard.modelData.rows:questionCard.chosen)
                            // A click can open a survivor choice without staging a group.
                            checked=Qt.binding(function(){return !!reviewBatch[questionCard.modelData.key]})
                        }
                        ToolTip.visible:hovered;ToolTip.text:"Choose at least two transaction rows in an ambiguous group before including the match."
                    }
                    Label {text:questionCard.modelData.reason;wrapMode:Text.Wrap;Layout.fillWidth:true}
                    Button {text:"Yes";objectName:"reviewYes";visible:questionCard.modelData.kind!=="correction" && questionCard.modelData.rows.length>1;enabled:!workflow.busy && (questionCard.modelData.kind==="same" || questionCard.counterpart>=1);onClicked:{
                        if(questionCard.modelData.kind==="same")answerReview(questionCard.modelData,"merge",questionCard.modelData.rows)
                        else {reviewScrollPosition=reviewQuestions.contentY;workflow.command("review_link_preview",{question:questionCard.modelData.key,generation:s.reviewGeneration,answer:"link",selected:[questionCard.modelData.rows[0],questionCard.modelData.rows[questionCard.counterpart]],leftAllocation:questionCard.leftAllocation,rightAllocation:questionCard.rightAllocation})}
                    }}
                    Button {text:"No";objectName:"reviewNo";visible:questionCard.modelData.kind!=="correction" && questionCard.modelData.rows.length>1;enabled:!workflow.busy;onClicked:answerReview(questionCard.modelData,questionCard.modelData.kind==="same"?"separate":"not_linked",[])}
                    Button {text:"Selected yes";objectName:"reviewSelectedYes";visible:questionCard.modelData.kind==="same" && questionCard.modelData.rows.length>2;enabled:!workflow.busy && questionCard.chosen.length>=2;onClicked:answerReview(questionCard.modelData,"merge",questionCard.chosen)}
                    Button {text:"Accept without bank match";objectName:"acceptMissingMatch";visible:!!questionCard.modelData.waitingLink;enabled:!workflow.busy;onClicked:answerReview(questionCard.modelData,"accept_missing",[])}
                    Button {objectName:"reviewCorrect";text:"Correct";visible:questionCard.modelData.kind==="correction";enabled:!workflow.busy;onClicked:openImportReview({account:questionCard.modelData.account,batch:questionCard.modelData.batch,record:questionCard.modelData.ordinal})}
                }
                Repeater {model:questionCard.modelData.rows;delegate:ColumnLayout {
                    id:reviewRow;required property var modelData;required property int index;Layout.fillWidth:true
                    property bool showSplits:false
                    RowLayout {
                        Layout.fillWidth:true
                        Item {Layout.preferredWidth:32;Layout.preferredHeight:36
                        CheckBox {objectName:"reviewRowSelect_"+reviewRow.index;anchors.centerIn:parent;enabled:!workflow.busy;checked:questionCard.chosen.some(row=>row.key===reviewRow.modelData.key);visible:questionCard.modelData.kind==="same" && questionCard.modelData.rows.length>2;onClicked:questionCard.toggle(reviewRow.modelData,checked);Accessible.name:"Select transaction "+reviewRow.modelData.payee}
                        RadioButton {anchors.centerIn:parent;objectName:"reviewLinkCandidate_"+reviewRow.index;visible:questionCard.modelData.kind==="link" && reviewRow.index>0;checked:questionCard.counterpart===reviewRow.index;onClicked:questionCard.selectCounterpart(reviewRow.index);Accessible.name:"Link to "+reviewRow.modelData.accountName+" "+reviewRow.modelData.payee}
                        }
                        Label {text:reviewRow.modelData.date;Layout.preferredWidth:95;textFormat:Text.PlainText}
                        Label {text:reviewRow.modelData.accountName;Layout.preferredWidth:105;elide:Text.ElideRight;textFormat:Text.PlainText}
                        Label {text:reviewRow.modelData.payee;Layout.fillWidth:true;Layout.minimumWidth:100;elide:Text.ElideRight;textFormat:Text.PlainText;MouseArea {anchors.fill:parent;onDoubleClicked:inspectReviewTransaction(reviewRow.modelData)}}
                        Button {flat:true;text:reviewRow.modelData.splits.length>1?(reviewRow.showSplits?"▾ ":"▸ ")+reviewRow.modelData.splits.length+" splits":(reviewRow.modelData.splits[0]||{}).categoryDisplay||"Uncategorized";Layout.preferredWidth:145;onClicked:reviewRow.showSplits=!reviewRow.showSplits}
                        Label {text:reviewRow.modelData.withdrawal;Layout.preferredWidth:100;horizontalAlignment:Text.AlignRight;textFormat:Text.PlainText}
                        Label {text:reviewRow.modelData.deposit;Layout.preferredWidth:100;horizontalAlignment:Text.AlignRight;textFormat:Text.PlainText}
                        Label {text:reviewRow.modelData.currency;Layout.preferredWidth:40;textFormat:Text.PlainText}
                        Item {Layout.preferredWidth:100;Layout.preferredHeight:40
                        Button {anchors.fill:parent;text:"Exclude…";ToolTip.text:"Don't suggest links…";ToolTip.visible:hovered;visible:questionCard.modelData.kind==="link";enabled:!workflow.busy;onClicked:openLinkExclusion(reviewRow.modelData,null)}
                        }
                        Button {Layout.preferredWidth:100;text:"Inspect";enabled:!workflow.busy;onClicked:inspectReviewTransaction(reviewRow.modelData)}
                    }
                    Repeater {model:reviewRow.showSplits?reviewRow.modelData.splits:[];delegate:Label {required property var modelData;Layout.fillWidth:true;leftPadding:24;text:modelData.amount+" · "+modelData.categoryDisplay+" · "+modelData.memo;textFormat:Text.PlainText;wrapMode:Text.Wrap}}
                    Label {text:reviewRow.modelData.note||"";visible:text.length>0;wrapMode:Text.Wrap;textFormat:Text.PlainText;Layout.fillWidth:true;font.italic:true}
                    Label {text:reviewRow.modelData.memo||"";visible:reviewRow.showSplits && text.length>0;wrapMode:Text.Wrap;textFormat:Text.PlainText;Layout.fillWidth:true}
                    RowLayout {
                        visible:questionCard.modelData.kind==="link" && reviewRow.modelData.splits.length>1 && (reviewRow.index===0 || reviewRow.index===questionCard.counterpart)
                        Layout.fillWidth:true;Layout.leftMargin:40;Layout.bottomMargin:8
                        Label {text:"Split to link"}
                        UsableComboBox {
                            objectName:"reviewSplit_"+reviewRow.index
                            Layout.preferredWidth:360;Layout.maximumWidth:reviewRow.width-140
                            model:reviewRow.modelData.splits;textRole:"reviewLabel"
                            currentIndex:model.findIndex(part=>part.allocationId===(reviewRow.index===0?questionCard.leftAllocation:questionCard.rightAllocation))
                            displayText:currentIndex<0?"Select split":currentText
                            onActivated:function(index){if(reviewRow.index===0)questionCard.leftAllocation=model[index].allocationId;else questionCard.rightAllocation=model[index].allocationId}
                        }
                        Item {Layout.fillWidth:true}
                    }
                }}
                Item {Layout.preferredHeight:8}
                }
            }
        }
        Frame {
        id:transferDialog;objectName:"transferDialog";Layout.fillWidth:true;Layout.preferredHeight:Math.min(400,reviewSheet.height*0.6)
        property bool expanded:false
        visible:expanded && currentPage==="review"
        function open(){navigate("review");expanded=true}
        function close(){expanded=false}
        contentItem:UsableScrollView {
            clip:true
            ColumnLayout {
                width:transferDialog.availableWidth;spacing:8;enabled:!workflow.busy
                RowLayout {Layout.fillWidth:true;Label {text:"Transfer details · "+(s.name||"");font.bold:true;Layout.fillWidth:true} Button {text:"Close details";onClicked:transferDialog.close()}}
                Label {text:"You can accept this transfer now and match it to an imported transaction later.";wrapMode:Text.Wrap;Layout.fillWidth:true}
                GroupBox {
                    title:"Choose a transfer destination";Layout.fillWidth:true
                    ColumnLayout {
                        anchors.fill:parent
                        RowLayout {
                            Label {text:"Destination"}
                            UsableComboBox {id:transferDestination;objectName:"transferDestination";model:transferDestinations();textRole:"name";currentIndex:0;onModelChanged:currentIndex=0;Layout.fillWidth:true;Accessible.name:"Transfer destination account"}
                        }
                        RowLayout {
                            NavigableTextField {id:transferDate;objectName:"transferDate";text:workflow.todayDisplay();placeholderText:s.dateFormat||"YYYY-MM-DD";Accessible.name:"Manual transfer date"}
                            NavigableTextField {id:transferAmount;onEditingFinished:resolveAmount(transferAmount);objectName:"transferAmount";placeholderText:"Signed amount in "+(s.currency||"");Accessible.name:"Manual transfer amount"}
                            NavigableTextField {id:transferPayee;objectName:"transferPayee";placeholderText:"Payee";Layout.fillWidth:true;Accessible.name:"Manual transfer payee"}
                            Button {objectName:"createManualTransfer";text:"Add transfer";enabled:!s.readOnly && transferDestination.currentIndex>0 && transferAmount.text.length>0;onClicked:transferCommand("transfer_manual",{link:workflow.newIdentity(),destination:transferDestination.model[transferDestination.currentIndex].id,date:transferDate.text,amount:transferAmount.text,payee:transferPayee.text,memo:transferMemo.text})}
                        }
                        Label {text:"Choose the destination account. If the matching transfer is already listed, link it instead of adding another.";textFormat:Text.PlainText;wrapMode:Text.Wrap;Layout.fillWidth:true}
                        NavigableTextField {id:transferMemo;placeholderText:"Memo (optional)";Layout.fillWidth:true;Accessible.name:"Manual transfer memo"}
                        RowLayout {
                            Label {text:"Selected split"}
                            UsableComboBox {id:transferAllocation;objectName:"transferAllocation";model:(selected.draft||{}).splits||[];textRole:"transferLabel";Layout.fillWidth:true;Accessible.name:"Selected transfer split"}
                            Button {objectName:"assignTransfer";text:"Use as transfer";enabled:!s.readOnly && !!selected.transferEligible && transferAllocation.currentIndex>=0 && transferDestination.currentIndex>0;onClicked:transferCommand("transfer_from_draft",{link:workflow.newIdentity(),destination:transferDestination.model[transferDestination.currentIndex].id,allocation:transferAllocation.model[transferAllocation.currentIndex].allocationId})}
                        }
                    }
                }
                Label {text:(s.transferReceipt||{}).operation?((s.transferReceipt||{}).balanceImpact||""):((s.transferReceipt||{}).summary||"");textFormat:Text.PlainText;wrapMode:Text.Wrap;Layout.fillWidth:true}
                Label {text:"Transfer history";font.bold:true}
                Repeater {
                    objectName:"transferJournalRepeater"
                    model:s.transferJournal||[]
                    delegate:Label {
                        required property var modelData
                        required property int index
                        objectName:"transferJournal_"+index
                        text:modelData.text
                        textFormat:Text.PlainText
                        wrapMode:Text.Wrap
                        Layout.fillWidth:true
                    }
                }
                Repeater {
                    model:s.transfers||[]
                    delegate:GroupBox {
                        required property var modelData
                        Layout.fillWidth:true;title:modelData.label
                        ColumnLayout {
                            anchors.fill:parent
                            Label {text:(modelData.committed?"Accepted":"Not yet accepted")+(modelData.protected?" · Other side already accepted":"")+(modelData.provisional?" · Provisional date":"")+(modelData.evidenced?" · Matched to imported transaction":modelData.accepted?" · Accepted without bank match":" · Needs review")+(modelData.proposed?" · Accept imported transactions to use this match":"");wrapMode:Text.Wrap;Layout.fillWidth:true}
                            RowLayout {
                                Button {text:"Accept without bank match";visible:!modelData.evidenced && !modelData.accepted;enabled:!s.readOnly && !modelData.proposed;onClicked:{const task=(s.reviewQuestions||[]).find(q=>q.waitingLink===modelData.id);if(task)answerReview(task,"accept_missing",[]);else transferCommand("transfer_accept",{link:modelData.id})}}
                                Button {text:"Accept transactions";visible:modelData.local && !modelData.committed;enabled:!s.readOnly;onClicked:guard(requestCommit) }
                                Button {text:"Delete this transfer";visible:modelData.isOrigin && modelData.local && !modelData.committed && !modelData.protected;enabled:!s.readOnly;onClicked:transferCommand("transfer_clear",{link:modelData.id})}
                                Button {text:"Show other account ↗";onClicked:showCounterpart(modelData)}
                            }
                            RowLayout {
                                Button {text:"Find matching transactions";visible:!modelData.evidenced && !modelData.proposed;enabled:!s.readOnly;onClicked:workflow.command("transfer_candidates",{link:modelData.id})}
                                Button {text:"Match selected split";visible:!modelData.evidenced && !modelData.proposed;enabled:!s.readOnly && !!selected.transferEligible && transferAllocation.currentIndex>=0;onClicked:transferCommand("transfer_match",{link:modelData.id,allocation:transferAllocation.model[transferAllocation.currentIndex].allocationId})}
                                Button {text:"Cancel match";visible:modelData.proposed;enabled:!s.readOnly;onClicked:transferCommand("transfer_unmatch",{link:modelData.id})}
                            }
                            ColumnLayout {
                                visible:s.candidateLink===modelData.id
                                Layout.fillWidth:true
                                Repeater {
                                    model:s.candidateLink===modelData.id?(s.transferCandidates||[]):[]
                                    delegate:RowLayout {
                                        required property var modelData
                                        Layout.fillWidth:true
                                        Label {text:modelData.label;textFormat:Text.PlainText;Layout.fillWidth:true;wrapMode:Text.Wrap}
                                        Button {text:"Use this match";onClicked:transferCommand("transfer_match",{link:s.candidateLink,batch:modelData.batch,ordinal:modelData.ordinal,allocation:modelData.allocation})}
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
        Item {id:reviewInspector;Layout.fillWidth:true;Layout.preferredHeight:reviewInspecting && !!selected.batch?330:0;visible:reviewInspecting && !!selected.batch;clip:true}
        RowLayout {
            Layout.fillWidth:true
            Button {text:"Transfer details…";visible:!!s.transfersSupported;onClicked:guard(function(){transferDialog.open()})}
            Button {text:"Close inspection";visible:reviewInspecting;onClicked:guard(function(){reviewInspecting=false})}
            Button {text:"Transfer suggestions…";objectName:"transferSuggestionSettings";onClicked:transferSuggestionSettings.open()}
            Button {text:"Undo";objectName:"reviewUndo";enabled:!!s.canUndoReview && !workflow.busy;onClicked:workflow.command("review_undo");ToolTip.text:Qt.platform.os==="osx"?"Cmd+Z":"Ctrl+Z";ToolTip.visible:hovered}
            Button {text:(s.acceptanceWarnings||[]).length>0?"Ignore warnings and accept":"Accept valid transactions";objectName:"reviewIgnoreCommit";visible:(s.newTransactions||0)>0;enabled:!!s.canIgnoreWarnings && !workflow.busy;onClicked:{reviewSheet.close();workflow.command("commit",{ignoreWarnings:true})}}
            Button {text:"Close";onClicked:{commitBeforeQuit=false;reviewSheet.close()}}
        }
    }
        }
                Item {
                    id:categoryPage;objectName:"categoryPage";anchors.fill:parent;visible:currentPage==="categories"
                    ColumnLayout {
                        anchors.fill:parent;spacing:8
                        Label {text:"Categories";font.pixelSize:20}
                        Label {text:"Use a colon for a subcategory, for example Auto: Gas makes Gas a sub-category of Auto";wrapMode:Text.Wrap;Layout.fillWidth:true}
                        RowLayout {
                            ClearableSearchField {
                                id:catalogSearch;objectName:"catalogSearch";placeholderText:"Search categories";Layout.fillWidth:true;maximumLength:256;enabled:!categorySaving;onTextEdited:categoryReveal=""
                                onClearRequested:{categoryReveal="";clearSearch()}
                            }
                            CheckBox {id:showArchivedCategories;objectName:"showArchivedCategories";text:"Show archived";enabled:!categorySaving}
                        }
                        RowLayout {
                            Button {objectName:"newCategory";text:"New category";enabled:!workflow.busy && !categorySaving;onClicked:guard(newCategory)}
                            Item {Layout.preferredWidth:16}
                            Button {objectName:"categorySortAscending";text:"A–Z ↑";highlighted:categorySort==="ascending";enabled:!workflow.busy && !categorySaving;onClicked:sortCategories("ascending")}
                            Button {objectName:"categorySortDescending";text:"Z–A ↓";highlighted:categorySort==="descending";enabled:!workflow.busy && !categorySaving;onClicked:sortCategories("descending")}
                            Button {objectName:"categorySortAdded";text:"Order added";highlighted:categorySort==="added";enabled:!workflow.busy && !categorySaving;onClicked:sortCategories("added")}
                        }
                        Rectangle {objectName:"categoryListDivider";Layout.fillWidth:true;Layout.preferredHeight:1;color:theme.border}
                        ListView {
                            id:catalogList;objectName:"catalogList";Layout.fillWidth:true;Layout.fillHeight:true;clip:true;reuseItems:false
                            boundsBehavior:Flickable.StopAtBounds
                            model:categoryRows()
                            onModelChanged:window.revealCategorySelection()
                            ScrollBar.vertical:UsableScrollBar {id:catalogBar;objectName:"catalogScrollBar"}
                            delegate:Rectangle {
                                id:categoryRow
                                required property var modelData
                                required property int index
                                width:catalogList.width-catalogBar.width;height:30
                                color:categorySelection===modelData.id && !modelData.pending?theme.selection:index%2?theme.alternate:theme.surface
                                property bool editing:categoryAction==="create"?!!modelData.pending:categoryAction==="rename" && modelData.id===categoryFormId
                                function focusEditor(){inlineCategory.forceActiveFocus();inlineCategory.selectAll()}
                                Rectangle {anchors.bottom:parent.bottom;width:parent.width;height:1;color:theme.divider}
                                Label {anchors.fill:parent;anchors.leftMargin:8;anchors.rightMargin:8;verticalAlignment:Text.AlignVCenter;horizontalAlignment:Text.AlignLeft;textFormat:Text.PlainText;elide:Text.ElideRight;color:theme.text;visible:!categoryRow.editing;text:modelData.path+(modelData.account?" [Account — edit in Accounts]"+(modelData.aliases?" · Aliases: "+modelData.aliases:""):"")+(modelData.deleted?" [Deleted]":modelData.archived?" [Archived]":"")}
                                MouseArea {
                                    anchors.fill:parent;acceptedButtons:Qt.LeftButton|Qt.RightButton;enabled:!categoryRow.editing && !workflow.busy && !categorySaving
                                    onClicked:function(mouse){
                                        let c=categoryRow.modelData
                                        let pt=mapToItem(window.contentItem,mouse.x,mouse.y)
                                        guard(function(){categorySelection=c.id;if(mouse.button===Qt.RightButton){categoryMenuEntry=c;categoryContext.popup(pt.x,pt.y)}})
                                    }
                                }
                                NavigableTextField {
                                    id:inlineCategory;objectName:categoryRow.editing?"categoryName":"";anchors.fill:parent;visible:categoryRow.editing;enabled:!categorySaving
                                    text:categoryRow.editing?categoryEditText:"";leftPadding:8;topPadding:2;bottomPadding:2;selectByMouse:true;Accessible.name:"Category name"
                                    onTextEdited:{categoryEditText=text}
                                    onAccepted:finishCategoryEdit()
                                    Keys.onEscapePressed:function(event){cancelCategoryEdit();event.accepted=true}
                                    onActiveFocusChanged:if(!activeFocus && categoryRow.editing && !categorySaving)Qt.callLater(function(){if(categoryRow.editing && !inlineCategory.activeFocus)finishCategoryEdit()})
                                }
                            }
                        }
                    }
                }
                ReportsWorkspace {id:reportsWorkspace;anchors.fill:parent;workflow:window.workflowBackend;guardAction:window.guard;visible:currentPage==="reports"}
                UsableScrollView {
                    id:rulesPage;objectName:"rulesPage";anchors.fill:parent;visible:currentPage==="rules";contentWidth:availableWidth;enabled:!workflow.busy
                    ColumnLayout {
                        width:rulesPage.availableWidth;spacing:8
                        Label {text:"Rules";font.pixelSize:20}
                        ColumnLayout {Layout.fillWidth:true;visible:!!s.rulesSupported
                        ClearableSearchField {id:ruleSearch;objectName:"ruleSearch";placeholderText:"Search rules";Layout.fillWidth:true;onClearRequested:clearSearch()}
                        RowLayout {
                            Button {objectName:"newRule";text:"New rule";enabled:{let generation=ruleEditGeneration;return newRuleIndex()<0} onClicked:newRule()}
                            CheckBox {id:showRemovedRules;objectName:"showRemovedRules";text:"Show removed"}
                        }
                        Column {
                            Layout.fillWidth:true
                            Repeater {
                                model:ruleModel
                                Item {
                                    id:ruleEntry
                                    required property int index
                                    required property string label
                                    required property string scope
                                    required property string input
                                    required property string expression
                                    required property string payeeTemplate
                                    required property string memoTemplate
                                    required property string categoryId
                                    required property bool inheritParent
                                    required property bool ruleOn
                                    property bool changed: {let generation=ruleEditGeneration;return ruleChanged(index)}
                                    property bool isNew: {let generation=ruleEditGeneration;return index>=0 && index<ruleModel.count && !savedRule(ruleModel.get(index).id)}
                                    property bool deletedDraft: {let generation=ruleEditGeneration;return index>=0 && index<ruleModel.count && ruleModel.get(index).pendingDelete}
                                    objectName:"ruleRow_"+index
                                    visible:index===ruleIndex || (label+" "+expression+" "+actionSummary(ruleEntry)).toLowerCase().indexOf(ruleSearch.text.toLowerCase())>=0
                                    width:rulesPage.availableWidth
                                    Rectangle {anchors.fill:parent;z:-1;color:ruleEntry.changed?theme.changed:"transparent";border.width:ruleIndex===ruleEntry.index?2:0;border.color:theme.focus}
                                    height:visible?ruleRowLayout.implicitHeight:0
                                    enabled:true
                                    RowLayout {
                                    id:ruleRowLayout;anchors.fill:parent
                                    Label {text:String(index+1);Layout.preferredWidth:28}
                                    CheckBox {objectName:"ruleEnabled_"+index;text:"Enabled";checked:ruleEntry.ruleOn;onClicked:toggleRule(index)}
                                    Button {objectName:"openRule_"+index;text:(label||("Rule "+(index+1)))+(ruleEntry.isNew?" · New":ruleEntry.deletedDraft?" · Deleted":ruleEntry.changed?" · Unsaved":"");highlighted:ruleIndex===index;onClicked:openRule(index);Layout.fillWidth:true}
                                    Label {text:(scope==="split"?"Split":"Transaction")+" · "+(scope==="split" && input==="payee"?"Parent payee":input);Layout.preferredWidth:170}
                                    Label {text:expression;elide:Text.ElideRight;Layout.preferredWidth:180;textFormat:Text.PlainText}
                                    Label {text:actionSummary(ruleEntry);elide:Text.ElideRight;Layout.preferredWidth:180;textFormat:Text.PlainText}
                                    Button {objectName:"moveRuleUp_"+index;text:"Up";enabled:index>0;onClicked:moveRule(index,-1)}
                                    Button {objectName:"moveRuleDown_"+index;text:"Down";enabled:index<ruleModel.count-1;onClicked:moveRule(index,1)}
                                    Button {objectName:"deleteRule_"+index;text:"Delete";enabled:!ruleEntry.deletedDraft;onClicked:deleteRule(index)}
                                    }
                                }
                            }
                        }
                        Column {
                            visible:showRemovedRules.checked
                            Layout.fillWidth:true
                            Label {text:"Removed rules";font.bold:true}
                            Repeater {
                                model:s.ruleTombstones||[]
                                RowLayout {
                                    required property var modelData
                                    width:rulesPage.availableWidth
                                    Label {text:(modelData.label||modelData.id)+" · v"+modelData.version+" · "+modelData.expression;Layout.fillWidth:true;elide:Text.ElideRight;textFormat:Text.PlainText}
                                    Button {objectName:"copyRemoved_"+modelData.id;text:"Copy into new rule";enabled:{let generation=ruleEditGeneration;return newRuleIndex()<0} onClicked:copyRemoved(modelData.id)}
                                }
                            }
                        }
                        ColumnLayout {
                            id:ruleEditor;objectName:"ruleEditor"
                            visible:ruleIndex>=0
                            Layout.fillWidth:true
                            RowLayout {
                                Layout.fillWidth:true
                                Label {text:"Rule name";font.bold:true;Layout.fillWidth:true}
                            Button {objectName:"saveRules";text:"Save";enabled:{let generation=ruleEditGeneration;return ruleChanged(ruleIndex)} onClicked:saveRules()}
                            Button {objectName:"discardRules";text:"Discard";enabled:{let generation=ruleEditGeneration;return ruleChanged(ruleIndex)} onClicked:discardRule()}
                            }
                            NavigableTextField {id:ruleLabel;objectName:"ruleLabel";placeholderText:"Rule name";Layout.fillWidth:true;onTextEdited:writeRule();Accessible.name:"Rule label"}
                            Label {text:"Match";font.bold:true}
                            RowLayout {
                                UsableComboBox {id:ruleScope;objectName:"ruleScope";Accessible.name:"Rule scope";model:["Transaction","Split"];onActivated:if(!ruleSync){writeRule()}}
                                UsableComboBox {id:ruleInput;objectName:"ruleInput";Accessible.name:"Rule source field";model:ruleScope.currentIndex===1?["Parent payee","Memo","Category"]:["Payee","Memo","Category"];onActivated:if(!ruleSync)writeRule()}
                            }
                            Label {text:"Pattern (regular expression)"}
                            NavigableTextField {id:ruleExpression;objectName:"ruleExpression";placeholderText:"Expression";Layout.fillWidth:true;onTextEdited:{ruleAssistNote.text="";syncAssist();writeRule()} Accessible.name:"Rule expression"}
                            Label {
                                objectName:"rulePatternKey";Layout.fillWidth:true;textFormat:Text.PlainText
                                font.pixelSize:12;fontSizeMode:Text.HorizontalFit;minimumPixelSize:10
                                property string firstLine:"^ start · $ end · . any character · .* any text"
                                property string secondLine:"* zero or more · + one or more (of the preceding item) · \\Q…\\E literal text"
                                text:firstLine+(width>=850?" · ":"\n")+secondLine
                                Accessible.name:text
                            }
                            RowLayout {
                                CheckBox {id:ruleCaseSensitive;objectName:"ruleCaseSensitive";text:"Case sensitive";onClicked:if(!ruleSync)setCase(checked)}
                                CheckBox {id:ruleWholeField;objectName:"ruleWholeField";text:"Whole field";onClicked:if(!ruleSync)setWhole(checked)}
                            }
                            Label {id:ruleAssistNote;objectName:"ruleAssistNote";text:"";visible:text.length>0;wrapMode:Text.Wrap;Layout.fillWidth:true;textFormat:Text.PlainText}
                            Label {text:"Changes";font.bold:true}
                            NavigableTextField {id:rulePayeeTemplate;objectName:"rulePayeeTemplate";visible:ruleScope.currentIndex===0;placeholderText:"Payee template. ${0} through ${n}, and $$";Layout.fillWidth:true;onTextEdited:writeRule();Accessible.name:"Payee template"}
                            NavigableTextField {id:ruleMemoTemplate;objectName:"ruleMemoTemplate";placeholderText:"Memo template. ${0} through ${n}, and $$";Layout.fillWidth:true;onTextEdited:writeRule();Accessible.name:"Memo template"}
                            RowLayout {
                                Label {text:"Category"}
                                CategoryCompletion {
                                    id:ruleCategoryField;objectName:"ruleCategory"
                                    Layout.fillWidth:true;Layout.minimumWidth:120;Layout.preferredWidth:categorySelectorWidth
                                    Layout.maximumWidth:Math.max(600,categorySelectorWidth);popupPreferredWidth:categorySelectorWidth
                                    placeholderText:"Choose a category";restoreOnFocusLoss:false
                                    model:sortedCategories((s.categories||[]).filter(c=>c.id===ruleCategoryId || (c.assignable && !c.archived && !c.deleted)))
                                    savedText:{let row=model.find(c=>c.id===ruleCategoryId);return row?row.path:""}
                                    onChoiceAccepted:function(choice){ruleCategoryId=choice.id;writeRule();ruleTransactionScope.forceActiveFocus()}
                                    onBackwardRequested:ruleMemoTemplate.forceActiveFocus()
                                    onEscapeRequested:resetText()
                                    Accessible.name:"Rule category"
                                }
                            }
                        }
                        RowLayout {
                            Label {text:"Include"}
                            UsableComboBox {id:ruleTransactionScope;objectName:"ruleTransactionScope";model:["All transactions in this account","New transactions only"];currentIndex:0;Layout.preferredWidth:340}
                        }
                        GridLayout {
                            columns:rulesPage.availableWidth<1000?1:4
                            Button {objectName:"previewThisRule";text:"Preview this rule";enabled:ruleIndex>=0 && !ruleModel.get(ruleIndex).pendingDelete;onClicked:previewThisRule()}
                            Button {objectName:"applyRules";text:{let generation=ruleEditGeneration;return ruleChanged(ruleIndex)?"Save and apply":"Apply to this account now"} enabled:applyReady();onClicked:applyRules()}
                        }
                        Label {objectName:"rulePreviewStatus";visible:currentRulePreview;text:(s.rulePreview||{}).label||"";font.bold:true;textFormat:Text.PlainText}
                        Label {objectName:"rulePreviewBody";visible:currentRulePreview;text:(s.rulePreview||{}).summaryText||"";wrapMode:Text.Wrap;Layout.fillWidth:true;textFormat:Text.PlainText}
                        Label {objectName:"ruleApplyHelp";visible:!applyReady();text:"Choose a rule to preview or apply.";wrapMode:Text.Wrap;Layout.fillWidth:true}
                        Button {objectName:"undoRules";text:"Undo rule application";enabled:!!s.canUndoRules && !workflow.busy;onClicked:workflow.command("rules_account_undo")}
                        RowLayout {
                            visible:currentRulePreview && ((s.rulePreview||{}).records||[]).length>0;Layout.fillWidth:true;Layout.rightMargin:18
                            Label {text:"Date";font.bold:true;Layout.preferredWidth:110}
                            Label {text:"Imported payee";font.bold:true;Layout.fillWidth:true}
                            Label {text:"Amount";font.bold:true;Layout.preferredWidth:160;horizontalAlignment:Text.AlignRight}
                        }
                        ListView {
                            id:ruleMatches;objectName:"ruleMatches";model:currentRulePreview?((s.rulePreview||{}).records||[]):[]
                            visible:count>0;Layout.fillWidth:true;Layout.preferredHeight:Math.min(360,Math.max(40,count*36))
                            clip:true;reuseItems:true;boundsBehavior:Flickable.StopAtBounds
                            ScrollBar.vertical:UsableScrollBar {}
                            delegate:RowLayout {
                                required property var modelData
                                required property int index
                                width:ruleMatches.width-18;height:36;spacing:8
                                Label {text:modelData.date;textFormat:Text.PlainText;Layout.preferredWidth:110;elide:Text.ElideRight}
                                Label {objectName:"ruleMatchPayee_"+index;text:String(modelData.payee||"").replace(/[\r\n\t]+/g," ");textFormat:Text.PlainText;Layout.fillWidth:true;Layout.minimumWidth:0;elide:Text.ElideRight}
                                Label {text:modelData.amount;textFormat:Text.PlainText;Layout.preferredWidth:160;horizontalAlignment:Text.AlignRight;elide:Text.ElideRight}
                            }
                        }
                    }
                    }
                }
                UsableScrollView {
                    id:historyPage;objectName:"committedHistoryPage";anchors.fill:parent;visible:currentPage==="history";contentWidth:availableWidth;enabled:!workflow.busy
                    ColumnLayout {
                        width:historyPage.availableWidth;spacing:10
                        Label {text:"Change history";font.pixelSize:20}
                        Label {text:"Corrections, removals and Undo operations. Balances below describe each operation when it was applied.";wrapMode:Text.Wrap;Layout.fillWidth:true}
                        Button {text:"Refresh history";onClicked:workflow.command("history")}
                        Label {text:"No changes to accepted transactions yet.";visible:(s.history||[]).length===0}
                        ListView {
                            id:historyIndex;objectName:"committedHistoryIndex";model:s.history||[];clip:true;reuseItems:true
                            Layout.fillWidth:true;Layout.preferredHeight:Math.min(200,contentHeight)
                            ScrollBar.vertical:UsableScrollBar {}
                            delegate:RowLayout {
                                required property var modelData
                                width:historyIndex.width
                                Label {textFormat:Text.PlainText;text:modelData.summary;Layout.fillWidth:true;Layout.minimumWidth:0;wrapMode:Text.Wrap}
                                Button {text:"Inspect history";onClicked:workflow.command("entry_history",{batch:modelData.batch,ordinal:modelData.ordinal})}
                            }
                        }
                        Button {objectName:"moreCommittedHistory";text:"Older changes";visible:!!s.historyMore;onClicked:workflow.command("history",{before:s.historyCursor,append:true})}
                        Label {textFormat:Text.PlainText;text:s.historyTitle||"";font.bold:true;wrapMode:Text.Wrap;Layout.fillWidth:true}
                        Button {text:"Inspect current record and source";visible:!!selected.committed;onClicked:navigate("ledger")}
                        ListView {
                            id:entryHistoryList;objectName:"entryHistoryList";model:s.entryHistory||[];clip:true;reuseItems:true;spacing:8
                            Layout.fillWidth:true;Layout.preferredHeight:Math.min(480,contentHeight)
                            ScrollBar.vertical:UsableScrollBar {}
                            delegate:Frame {
                                required property var modelData
                                width:entryHistoryList.width
                                contentItem:ColumnLayout {
                                    Label {textFormat:Text.PlainText;text:modelData.summary;wrapMode:Text.Wrap;Layout.fillWidth:true}
                                    UsableScrollView {
                                        Layout.fillWidth:true;Layout.preferredHeight:150;contentWidth:availableWidth
                                        TextArea {text:modelData.details;readOnly:true;selectByMouse:true;textFormat:TextEdit.PlainText;wrapMode:TextEdit.Wrap}
                                    }
                                    Label {textFormat:Text.PlainText;text:modelData.undoReason;visible:!modelData.undoEligible;wrapMode:Text.Wrap;Layout.fillWidth:true}
                                    Button {objectName:"undo_"+modelData.operation;text:"Review Undo…";enabled:modelData.undoEligible;onClicked:guardedPreview("preview_change",{kind:"undo",operation:modelData.operation,batch:modelData.batch,ordinal:modelData.ordinal})}
                                    Button {text:"Review group Undo…";visible:!!modelData.bulkGroup;onClicked:guardedPreview("preview_bulk",{kind:"undo",operation:modelData.bulkGroup})}
                                    Button {text:"Review group Undo…";visible:!!modelData.reapplyGroup;onClicked:guardedPreview("rules_reapply_preview",{undoOf:modelData.reapplyGroup})}
                                }
                            }
                        }
                    }
                }
                UsableScrollView {
                    id:activeSettings;objectName:"activeSettings";anchors.fill:parent;visible:currentPage==="settings";enabled:!workflow.busy;contentWidth:availableWidth
                    ColumnLayout {
                        width:activeSettings.availableWidth;spacing:12
                        Label {text:"Settings";font.pixelSize:20}
                        Label {text:"Window";font.bold:true}
                        CheckBox {objectName:"rememberWindowPosition";text:"Remember window size and position";checked:workflow.rememberWindowPosition;onToggled:workflow.rememberWindowPosition=checked}
                        Label {text:"";Layout.fillWidth:true;wrapMode:Text.Wrap}
                        Label {text:"File settings";font.bold:true}
                        Label {text:"Current file";font.bold:true}
                        Label {text:"Date format: "+(s.dateFormat||"YYYY-MM-DD")}
                        RowLayout {
                            CheckBox {objectName:"amountFormatting";text:"Use amount formatting";checked:s.amountFormatting===undefined?true:s.amountFormatting
                                onToggled:guard(function(){workflow.command("amount_format",{enabled:checked,style:s.amountStyle||"comma-dot"})})}
                            UsableComboBox {objectName:"amountFormatStyle";enabled:!!s.amountFormatting
                                model:["1,234.56","1.234,56","1 234,56"]
                                currentIndex:["comma-dot","dot-comma","space-comma"].indexOf(s.amountStyle||"comma-dot")
                                onActivated:guard(function(){workflow.command("amount_format",{enabled:true,style:["comma-dot","dot-comma","space-comma"][currentIndex]})})}
                        }
                        Label {text:"Use this style when entering amounts and calculations. Thousands separators are optional.";Layout.fillWidth:true;wrapMode:Text.Wrap}

                        Label {textFormat:Text.PlainText;text:s.path||"";Layout.fillWidth:true;wrapMode:Text.WrapAnywhere}
                        Label {text:"Close this file before opening another.";Layout.fillWidth:true;wrapMode:Text.Wrap}
                        Button {objectName:"closeDataset";text:"Close file…";onClicked:guard(function(){workflow.command("close")})}
                        Label {text:"Backup and recovery";font.bold:true}
                        Label {text:"Save a backup to a new file. To restore a backup, open it and save a copy to a new location.";Layout.fillWidth:true;wrapMode:Text.Wrap}
                        Button {objectName:"settingsBackup";text:"Save backup…";onClicked:guard(function(){backupDialog.open()})}
                        Button {objectName:"returnToLedger";text:"Return to account";onClicked:navigate("ledger")}
                    }
                }
                UsableScrollView {
                    id:ledgerScroll;objectName:"ledgerScroll";parent:window.reportLedger?reportsWorkspace.ledgerHost:workspaceBody;anchors.fill:parent;visible:currentPage==="ledger" || window.reportLedger;clip:true
                    // Keep controls reachable in a narrow window or with a wide sidebar.
                    contentWidth:Math.max(1080,availableWidth);contentHeight:availableHeight
                    ColumnLayout {
            id:ledgerPage;objectName:"ledgerPage"
            width:ledgerScroll.contentWidth;height:ledgerScroll.availableHeight;enabled:!workflow.busy || workflow.selecting
            RowLayout {
                ClearableSearchField {id:search;objectName:"ledgerSearch";clearObjectName:"clearFilter";hasAppliedFilter:appliedFilter.length>0;onClearRequested:guard(function(){search.clearSearch();query()});placeholderText:'Search · category:Auto · period:may-june · QIF: · regex:"pattern"';maximumLength:4096;Layout.fillWidth:true;onAccepted:guard(query);Keys.onEscapePressed:guard(function(){search.clear();query()});Accessible.name:"Ledger search"}
                Button {objectName:"applyFilter";text:"Apply";onClicked:guard(query)}
                Button {objectName:"addTransaction";visible:!window.reportLedger;text:"+ New transaction";enabled:!s.readOnly && !workflow.busy;onClicked:addTransaction();ToolTip.visible:hovered;ToolTip.text:"Add transaction";Accessible.name:"Add transaction"}
                Button {objectName:"undoEdit";text:s.editUndoLabel||"Undo edit";visible:!!s.canUndoEdit;enabled:!workflow.busy;onClicked:undoEdit();ToolTip.text:"Undo edits from this session. Other changes end this Undo sequence.";ToolTip.visible:hovered}
                Button {objectName:"undoTransferAssignment";text:"Undo category change";visible:!!s.canUndoTransferAssignment;enabled:!workflow.busy;onClicked:guard(function(){captureLedgerView("category");workflow.command("undo_transfer_assignment")})}
            }
            Row {
                Layout.fillWidth:true;spacing:0
                Repeater {model:window.reportLedger?["Date","Payee","Account","Category","Withdrawal","Deposit","In report"]:["Date","Payee","Category","Withdrawal","Deposit","Balance"]
                    Button {
                        required property string modelData;required property int index
                        objectName:"ledgerColumn_"+index
                        width:(ledger.width-20)*window.ledgerColumnWeights[index]
                        readonly property int sortColumn:window.reportLedger?(index===2 || index===6?-1:index>2?index-1:index):index
                        text:modelData+(sortColumn===(s.sort||0)?(s.descending?" ▼":" ▲"):"")
                        enabled:sortColumn>=0;onClicked:guard(function(){query(sortColumn,s.sort===sortColumn?!s.descending:false)})
                    }
                }
            }
            ListView {
                id:ledger;objectName:"durableLedger";Layout.fillWidth:true;Layout.fillHeight:true;clip:true;model:workflow;reuseItems:true;highlightFollowsCurrentItem:false
                // Cached offscreen delegates incubate asynchronously across model resets.
                // Visible delegates remain virtualized and reusable without that queue.
                cacheBuffer:0
                MouseArea {parent:ledger;anchors.fill:parent;z:-1;acceptedButtons:Qt.RightButton;enabled:!workflow.busy;onClicked:ledgerContext.popup()}
                Accessible.role:Accessible.List
                Accessible.name:"Transactions in "+(s.name||"account")
                Accessible.description:"Use Up and Down to select, Shift to extend selection, and Enter to review a transaction."
                activeFocusOnTab:true
                Keys.onPressed: function(event) {
                    if(event.key===Qt.Key_Up || event.key===Qt.Key_Down || event.key===Qt.Key_Home || event.key===Qt.Key_End) {
                        let current=s.selectionIndex===undefined?-1:s.selectionIndex
                        let next=event.key===Qt.Key_Home?0:event.key===Qt.Key_End?count-1:Math.max(0,Math.min(count-1,current+(event.key===Qt.Key_Down?1:-1)))
                        if(next>=0 && next<count){selectLedgerRow(next,event.modifiers);positionViewAtIndex(next,ListView.Contain)}
                        event.accepted=true
                    } else if(event.key===Qt.Key_Return || event.key===Qt.Key_Enter) {
                        let id=workflow.rowIdentity(s.selectionIndex===undefined?-1:s.selectionIndex)
                        if(id.batch)inspectRecord(id.batch,id.ordinal)
                        event.accepted=true
                    } else if(event.key===Qt.Key_Delete && s.bulkSupported && (s.selectionCount||0)>0) {
                        removeSelected();event.accepted=true
                    }
                }
                ScrollBar.vertical:UsableScrollBar {
                    objectName:"ledgerScrollBar";parent:ledgerScroll
                    x:ledgerScroll.width-width;y:ledger.y+ledgerPage.y;height:ledger.height
                }
                delegate:Rectangle {
                    required property var record
                    required property int index
                    id: ledgerRow
                    objectName:"ledgerRow_"+index
                    Accessible.role:Accessible.ListItem
                    Accessible.name:record.date+", "+record.payee+", "+record.category+", withdrawal "+record.withdrawal+", deposit "+record.deposit+", balance "+record.balance+" "+(s.currency||"")+", "+record.status
                    Accessible.selectable:true
                    Accessible.selected:!!(s.selectionKeys||{})[record.batch+":"+record.ordinal]
                    Accessible.focusable:true
                    Accessible.focused:ledger.activeFocus && index===s.selectionIndex
                    Accessible.onPressAction:{ledger.forceActiveFocus();selectLedgerRow(index,0)}
                    property bool expanded: !window.editorClosed && record.batch===selected.batch && record.ordinal===selected.ordinal
                    function syncEditorHost() {
                        if(expanded)window.ledgerEditorHost=ledgerRow
                        else if(window.ledgerEditorHost===ledgerRow)window.ledgerEditorHost=null
                    }
                    onExpandedChanged:syncEditorHost()
                    Component.onCompleted:syncEditorHost()
                    ListView.onReused:syncEditorHost()
                    Component.onDestruction:if(window.ledgerEditorHost===ledgerRow)window.ledgerEditorHost=null
                    property bool viewingSplits: !!window.splitViews[window.splitKey(record)] && record.splits.length>1
                    property alias editorSlot: rowEditorSlot
                    readonly property int rowHeight:34
                    width:ledger.width-20;height:rowHeight+(expanded?editor.height:splitPreview.height);color:(s.selectionKeys||{})[record.batch+":"+record.ordinal]?theme.selection:record.status==="Draft"?(index%2===0?theme.draft:theme.draftAlternate):(index%2===0?theme.row:theme.rowAlternate)
                    border.width:(s.selectionKeys||{})[record.batch+":"+record.ordinal]?1:0;border.color:theme.focus
                    MouseArea {width:parent.width;height:ledgerRow.rowHeight;acceptedButtons:Qt.LeftButton|Qt.RightButton
                        onClicked:function(mouse){ledger.forceActiveFocus();if(mouse.button===Qt.RightButton){if(!(s.selectionKeys||{})[record.batch+":"+record.ordinal])selectLedgerRow(ledgerRow.index,0);ledgerContext.popup()}else selectLedgerRow(ledgerRow.index,mouse.modifiers)}
                        onDoubleClicked:function(mouse){if(mouse.button!==Qt.LeftButton)return;if(record.batch.length)toggleRecord(record.batch,record.ordinal);else editOpening()}
                    }
                    Row {
                        width:parent.width;height:ledgerRow.rowHeight;spacing:0
                        Repeater {model:window.reportLedger?[record.date,record.payee,record.reportAccount,record.category,record.withdrawal,record.deposit,record.reportMatches?record.reportIncluded:"No longer included"]:[record.date,record.payee,record.category,record.withdrawal,record.deposit,record.balance]
                            Item {
                                required property string modelData
                                required property int index
                                width:(ledger.width-20)*window.ledgerColumnWeights[index];height:ledgerRow.rowHeight
                                TransferArrow {id:rowTransfer;anchors.verticalCenter:parent.verticalCenter;objectName:index===window.ledgerCategoryColumn?"ledgerTransferArrow_"+ledgerRow.index:"";destination:index===window.ledgerCategoryColumn?window.firstLedgerTransfer(record):null}
                                Label {anchors.fill:parent;anchors.leftMargin:index===window.ledgerCategoryColumn?32:0;textFormat:Text.PlainText;text:modelData;elide:Text.ElideRight;verticalAlignment:Text.AlignVCenter;horizontalAlignment:index>window.ledgerCategoryColumn?Text.AlignRight:Text.AlignLeft;visible:index!==window.ledgerCategoryColumn || record.splits.length<2}
                                LedgerAction {
                                    anchors.fill:parent;anchors.leftMargin:32;visible:index===window.ledgerCategoryColumn && record.splits.length>1
                                    textAlignment:Text.AlignLeft
                                    objectName:index===window.ledgerCategoryColumn && record.batch.length?"splitToggle_"+record.ordinal:""
                                    text:(ledgerRow.viewingSplits?"▾ ":"▸ ")+record.splits.length+" splits"
                                    enabled:!workflow.busy && !ledgerRow.expanded
                                    Accessible.name:(ledgerRow.viewingSplits?"Hide":"Show")+" saved splits for "+record.payee
                                    onClicked:window.toggleSplits(record)
                                }
                            }
                        }
                    }
                    Loader {
                        id:splitPreview;objectName:record.batch.length?"splitPreview_"+record.ordinal:""
                        x:12;y:ledgerRow.rowHeight;width:parent.width-24
                        active:ledgerRow.viewingSplits && !ledgerRow.expanded
                        height:item?Math.min(160,item.contentHeight):0
                        visible:active
                        sourceComponent:ListView {
                        clip:true;reuseItems:true;model:record.splits
                        ScrollBar.vertical:UsableScrollBar {}
                        header:Label {text:"Saved splits (read-only) · "+(s.currency||"");height:26}
                        delegate:Row {
                            required property var modelData
                            property bool includedInReport:window.reportLedger && (record.reportAllocations||[]).indexOf(modelData.allocationId)>=0
                            width:splitPreview.width;height:30;spacing:8
                            Item {
                                width:parent.width*0.25;height:30
                                TransferArrow {id:splitTransfer;objectName:"splitTransferArrow_"+modelData.allocationId;destination:window.ledgerTransfer(record,modelData)}
                                Label {anchors.fill:parent;anchors.leftMargin:splitTransfer.visible?32:0;verticalAlignment:Text.AlignVCenter;textFormat:Text.PlainText;text:(includedInReport?"● ":"")+(s.categoriesSupported?modelData.categoryDisplay:(modelData.category || "Uncategorized"));elide:Text.ElideRight}
                            }
                            Label {textFormat:Text.PlainText;text:modelData.memo;width:parent.width*0.5;elide:Text.ElideRight}
                            Label {textFormat:Text.PlainText;text:modelData.amount;width:parent.width*0.2;horizontalAlignment:Text.AlignRight}
                        }
                    }
                    }
                    Item {id:rowEditorSlot;y:ledgerRow.rowHeight;width:parent.width;height:ledgerRow.expanded?editor.height:0}
                }
            }
            Label {visible:!window.reportLedger;textFormat:Text.PlainText;text: "Filtered totals · withdrawals (signed): "+(s.withdrawals||"unavailable")+" · deposits: "+(s.deposits||"unavailable")}
            Item {
                id:reviewPanel;objectName:"reviewPanel"
                Layout.fillWidth:true;Layout.preferredHeight:editor.visible && !editor.inlineRow?editor.height:0
            }
                    }
                }
            }
        }
    }
    }
    Frame {
        id:editor;objectName:"transactionEditor"
        // One persistent editor, visually attached to the inspected transaction.
        // Its fields and split buffer never belong to a recycled delegate.
        property bool inlineRow: !!window.ledgerEditorHost && window.ledgerEditorHost.expanded
        parent:currentPage==="review"?reviewInspector:(inlineRow?window.ledgerEditorHost.editorSlot:reviewPanel)
        visible:!!s.active && !!selected.batch && !window.editorClosed && (currentPage!=="review" || reviewInspecting)
        Keys.priority:Keys.AfterItem
        Keys.onPressed:function(event) {
            if(event.key===Qt.Key_Return || event.key===Qt.Key_Enter)event.accepted=true
            else if(event.key===Qt.Key_Escape){discardEditor();event.accepted=true}
            else if(event.matches(StandardKey.Undo) && !dirty && (s.canUndoEdit || s.canUndoManual)){undoEdit();event.accepted=true}
        }
        enabled:!workflow.busy
        width:parent.width;height:330
        UsableScrollView {
            id:editorScroll;objectName:"editorScroll"
            anchors.fill:parent;contentWidth:availableWidth
            leftPadding:8;rightPadding:24
            ColumnLayout {
                width:editorScroll.availableWidth
                Label {textFormat:Text.PlainText;text:"Imported amount: "+(selected.original||"Unavailable");visible:!!selected.original;wrapMode:Text.Wrap;Layout.fillWidth:true}
                Label {textFormat:Text.PlainText;text:selected.diagnostic||"";color:theme.error;visible:text.length>0;wrapMode:Text.Wrap;Layout.fillWidth:true}
                Label {textFormat:Text.PlainText;text:selected.sourceClasses ? "Details from file — "+selected.sourceClasses : "";visible:text.length>0;wrapMode:Text.Wrap;Layout.fillWidth:true}
                Label {textFormat:Text.PlainText;text:selected.committed && !s.committedChangesSupported?"This file does not support editing accepted transactions.":"";visible:text.length>0;wrapMode:Text.Wrap;Layout.fillWidth:true}
                RowLayout {
                    visible:!!selected.committed
                    Button {objectName:"removeCommitted";text:"Remove transaction…";visible:!!selected.canCorrect;onClicked:guard(function(){workflow.command("preview_change",{kind:"remove"})})}
                    Button {objectName:"showEntryHistory";text:"History";visible:!!s.committedChangesSupported;onClicked:guard(function(){navigate("history");workflow.command("entry_history",{batch:selected.batch,ordinal:selected.ordinal})})}
                }
                RowLayout {
                    visible:!!selected.draft
                    NavigableTextField {id:editDate;KeyNavigation.tab:editPayee;objectName:"durableDate";placeholderText:s.dateFormat||"YYYY-MM-DD";readOnly:!editEnabled;Keys.onPressed:function(event){if(event.key===Qt.Key_Escape){discardEditor();event.accepted=true}else if(event.matches(StandardKey.Undo) && !dirty && (s.canUndoEdit || s.canUndoManual)){undoEdit();event.accepted=true}}
                    onAccepted:if(editEnabled)saveEditor();onTextEdited:dirty=true;Accessible.name:"Transaction date"}
                    CalendarPicker {objectName:"transactionCalendar";workflow:window.workflowBackend;field:editDate;enabled:editEnabled;onSelected:function(value){editDate.text=value;dirty=true}}
                    NavigableTextField {id:editPayee;KeyNavigation.tab:editTotal;KeyNavigation.backtab:editDate;objectName:"durablePayee";placeholderText:"Payee";readOnly:!editEnabled;Keys.onPressed:function(event){if(event.key===Qt.Key_Escape){discardEditor();event.accepted=true}else if(event.matches(StandardKey.Undo) && !dirty && (s.canUndoEdit || s.canUndoManual)){undoEdit();event.accepted=true}}
                    onAccepted:if(editEnabled)saveEditor();onTextEdited:{dirty=true;payeeTouched=true} Layout.fillWidth:true;Accessible.name:"Payee"}
                    NavigableTextField {id:editTotal;onEditingFinished:if(selected.draft)resolveAmount(editTotal,selected.draft.currencyDefinition);KeyNavigation.tab:editorSplits.count?editorSplits.itemAt(0).categoryField:editMemo;KeyNavigation.backtab:editPayee;objectName:"durableTotal";placeholderText:"Amount";readOnly:!editEnabled;Keys.onPressed:function(event){if(event.key===Qt.Key_Escape){discardEditor();event.accepted=true}else if(event.matches(StandardKey.Undo) && !dirty && (s.canUndoEdit || s.canUndoManual)){undoEdit();event.accepted=true}}
                    onAccepted:if(editEnabled)saveEditor();onTextEdited:dirty=true;Accessible.name:"Total"}
                }
                Repeater {
                    id:editorSplits
                    model:splitModel
                    RowLayout {
                        Layout.fillWidth:true
                        property alias categoryField: categoryInput
                        property alias amountField: splitAmount
                        required property int index
                        required property string category
                        required property string memo
                        required property string amount
                        Label {visible:window.reportLedger;text:index>=0 && index<splitModel.count && (s.reportRowInfo||{})[selected.batch+":"+selected.ordinal] && ((s.reportRowInfo||{})[selected.batch+":"+selected.ordinal].allocations||[]).indexOf(splitModel.get(index).allocationId)>=0?"●":"";Layout.preferredWidth:12;ToolTip.visible:splitReportHover.hovered;ToolTip.text:"Included in this report cell";HoverHandler {id:splitReportHover}}
                        CategoryCompletion {
                            id:categoryInput;objectName:"splitCategory_"+index
                            popupObjectName:"categoryDropdown_"+index
                            matchesObjectName:"categoryMatches_"+index
                            scrollBarObjectName:"categoryDropdownScrollBar"
                            visible:!!s.categoriesSupported;readOnly:!editEnabled && !originLink(index>=0 && index<splitModel.count?splitModel.get(index).allocationId:"")
                            Layout.preferredWidth:Math.min(categorySelectorWidth,Math.max(120,editorScroll.availableWidth-240))
                            popupPreferredWidth:categorySelectorWidth;placeholderText:"Category"
                            Accessible.name:"Split category"
                            savedText:index>=0 && index<splitModel.count ? (splitModel.get(index).categoryDisplay==="Uncategorized" ? "" : splitModel.get(index).categoryDisplay||"") : ""
                            property bool unresolvedTransfer:category.startsWith("[") && !originLink(index>=0 && index<splitModel.count?splitModel.get(index).allocationId:"")
                            allowUncategorized:!unresolvedTransfer
                            model:sortedCategories((s.categories||[]).concat(s.accountCategories||[]).filter(function(c){
                                return (!unresolvedTransfer || c.account) && (!selected.newManual || !c.account) && !c.deleted && !c.archived && (c.account ? c.accountId!==s.accountId : c.assignable)
                            }))
                            onChoiceAccepted:function(choice) {
                                // Move first so durable acceptance restores the intended next field.
                                if(splitModel.count===1)editMemo.forceActiveFocus()
                                else splitMemo.forceActiveFocus()
                                pickerIndex=index
                                if(choice.account && unresolvedTransfer)transferCommand("transfer_from_draft",{link:workflow.newIdentity(),destination:choice.accountId,allocation:splitModel.get(index).allocationId})
                                else if(choice.account)chooseAccountDestination(choice.accountId)
                                else chooseCategory(choice.id,choice.path)
                            }
                            onBackwardRequested: {
                                if(index>0)editorSplits.itemAt(index-1).amountField.forceActiveFocus();else editTotal.forceActiveFocus()
                            }
                            onEscapeRequested:discardEditor()
                            canUndoAssignment:!dirty && !!s.canUndoEdit
                            onUndoAssignmentRequested:undoEdit()
                        }
                        NavigableTextField {visible:!s.categoriesSupported;text:category;placeholderText:"Category";readOnly:!editEnabled;Keys.onPressed:function(event){if(event.key===Qt.Key_Escape){discardEditor();event.accepted=true}else if(event.matches(StandardKey.Undo) && !dirty && (s.canUndoEdit || s.canUndoManual)){undoEdit();event.accepted=true}}
                    onAccepted:if(editEnabled)saveEditor();onTextEdited:{splitModel.setProperty(index,"category",text);splitModel.setProperty(index,"categoryTouched",true);dirty=true} Accessible.name:"Split category"}
                        NavigableTextField {id:splitMemo;KeyNavigation.backtab:categoryInput;KeyNavigation.tab:splitAmount;objectName:"editSplitMemo_"+index;text:memo;placeholderText:"Split memo";Layout.fillWidth:true;readOnly:!editEnabled;Keys.onPressed:function(event){if(event.key===Qt.Key_Escape){discardEditor();event.accepted=true}else if(event.matches(StandardKey.Undo) && !dirty && (s.canUndoEdit || s.canUndoManual)){undoEdit();event.accepted=true}}
                    onAccepted:if(editEnabled)saveEditor();onTextEdited:{splitModel.setProperty(index,"memo",text);splitModel.setProperty(index,"memoTouched",true);dirty=true} Accessible.name:"Split memo"}
                        NavigableTextField {id:splitAmount;onEditingFinished:{if(selected.draft){resolveAmount(splitAmount,selected.draft.currencyDefinition);splitModel.setProperty(index,"amount",text)}} KeyNavigation.backtab:splitMemo;KeyNavigation.tab:index+1<editorSplits.count?editorSplits.itemAt(index+1).categoryField:editMemo;objectName:"editSplitAmount_"+index;text:amount;placeholderText:"Split amount";readOnly:!editEnabled;Keys.onPressed:function(event){if(event.key===Qt.Key_Escape){discardEditor();event.accepted=true}else if(event.matches(StandardKey.Undo) && !dirty && (s.canUndoEdit || s.canUndoManual)){undoEdit();event.accepted=true}}
                    onAccepted:if(editEnabled)saveEditor();onTextEdited:{splitModel.setProperty(index,"amount",text);if(index===automaticSplitIndex){manualSplitAmounts=true;automaticSplitIndex=-1};splitEditMessage="";dirty=true} Accessible.name:"Split amount"}
                        Label {objectName:"splitRemainder_"+index;visible:!manualSplitAmounts && automaticSplitIndex===index;text:"Automatic remainder"}
                        Button {objectName:"removeSplit_"+index;text:"Remove";visible:editEnabled && splitModel.count>1;enabled:splitModel.count>1 && !category.startsWith("[");onClicked:{if(!enabled || splitModel.count<=1)return;window.removeEditorSplit(index)}}
                    }
                }
                NavigableTextField {id:editMemo;KeyNavigation.backtab:editorSplits.count>1?editorSplits.itemAt(editorSplits.count-1).amountField:editorSplits.count?editorSplits.itemAt(0).categoryField:editTotal;objectName:"durableMemo";visible:!!selected.draft;placeholderText:"Memo";readOnly:!editEnabled;Keys.onPressed:function(event){if(event.key===Qt.Key_Escape){discardEditor();event.accepted=true}else if(event.matches(StandardKey.Undo) && !dirty && (s.canUndoEdit || s.canUndoManual)){undoEdit();event.accepted=true}}
                    onAccepted:if(editEnabled)saveEditor();onTextEdited:{dirty=true;memoTouched=true} Layout.fillWidth:true;Accessible.name:"Memo"}
                RowLayout {
                    visible:editEnabled || (!!s.rulesSupported && !!selected.batch)
                    Button {objectName:"transactionRuleAction";visible:!!s.rulesSupported && !!selected.batch && !selected.newManual;text:"Create rule…";enabled:!workflow.busy && ruleCreateChoices().length>0;onClicked:transactionRuleAction()}
                    Button {objectName:"editTransactionRule";visible:!!s.rulesSupported && !!selected.batch && !selected.newManual;text:"Edit rule…";enabled:!workflow.busy && ruleChoices().length>0;onClicked:editTransactionRule()}
                    Button {objectName:"addSplit";visible:editEnabled;text:"Add split";enabled:splitModel.count<1000;onClicked:{splitModel.append({category:"",memo:"",amount:"0",categoryId:"",bindingRevision:"",allocationId:"",categoryDisplay:"Uncategorized",categoryTouched:true,memoTouched:true});dirty=true}}
                    Label {visible:editEnabled;textFormat:Text.PlainText;text:splitEditMessage!==""?splitEditMessage:dirty?"Press Enter to save, or Escape to discard your changes.":"Saved · double-click the transaction to close";wrapMode:Text.Wrap;Layout.fillWidth:true}
                }
                RowLayout {
                    visible:currentPage==="review" && !!selected.editable && !selected.repair && !s.readOnly
                    Label {textFormat:Text.PlainText;text:selected.decision===0?"Unresolved match":selected.decision===2?(selected.removedDuplicate?"Keep removed: ":"Excluded duplicate: ")+(selected.duplicate||""):"Include as new"}
                    Button {objectName:"includeSeparate";text:"Include as separate transaction";onClicked:guard(function(){workflow.command("decide",{duplicate:false})})}
                    UsableComboBox {id:candidate;objectName:"duplicateCandidate";model:selected.candidates||[];textRole:"label";Layout.fillWidth:true;Accessible.name:"Matching transaction"}
                    Button {objectName:"inspectRemovalHistory";text:"Inspect removal history";visible:candidate.currentIndex>=0 && !!candidate.model[candidate.currentIndex].removed;onClicked:guard(function(){let c=candidate.model[candidate.currentIndex];navigate("history");workflow.command("entry_history",{batch:c.batch,ordinal:c.ordinal})})}
                    Button {objectName:"excludeDuplicate";text:candidate.currentIndex>=0 && candidate.model[candidate.currentIndex].removed?"Keep removed":"Already imported";enabled:candidate.currentIndex>=0;onClicked:guard(function(){let c=candidate.model[candidate.currentIndex];workflow.command("decide",{duplicate:true,batch:c.batch,ordinal:c.ordinal})})}
                }
                RowLayout {
                    visible:!!s.rulesSupported && !!selected.batch
                    Label {
                        objectName:"ruleWinners";Layout.fillWidth:true;wrapMode:Text.Wrap;textFormat:Text.PlainText
                        text:{
                            let lines=[]
                            for(let assignment of (selected.ruleAssignments||[])) {
                                if(!assignment.available)continue
                                let rule=(s.rules||[]).find(r=>r.id===assignment.rule)
                                let name=rule && rule.label?rule.label:"Unnamed rule"
                                let field=assignment.field
                                if(assignment.allocation!==null && assignment.allocation!==undefined && ((selected.draft||{}).splits||[]).length>1)
                                    field+=" (split "+(assignment.allocation+1)+")"
                                let line=field+" assigned by “"+name+"”"
                                if(lines.indexOf(line)<0)lines.push(line)
                            }
                            return lines.join("\n")
                        }
                    }
                    Label {text:"The rule that assigned this value was deleted.";visible:(selected.ruleAssignments||[]).some(c=>!c.available);wrapMode:Text.Wrap;Layout.fillWidth:true}
                }
                Label {visible:!!s.rulesSupported && !!selected.ruleMatchError;textFormat:Text.PlainText;text:selected.ruleMatchError||"";color:theme.error;wrapMode:Text.Wrap;Layout.fillWidth:true}
                Label {text:selected.newManual?"New transaction · not saved":"Manually entered";visible:!!selected.manual;textFormat:Text.PlainText}
                CheckBox {
                    id:sourceToggle;objectName:"sourceToggle";text:"Show original imported transaction";enabled:!selected.manual || !!selected.hasImportedOriginal
                    MouseArea {
                        parent:sourceToggle.indicator;anchors.fill:parent
                        onClicked:{sourceToggle.forceActiveFocus();sourceToggle.toggle()}
                    }
                }
                RowLayout {
                    visible:sourceToggle.enabled && sourceToggle.checked;Layout.fillWidth:true;spacing:12
                    Label {objectName:"sourceFormat";text:"Format: "+((selected.sourceFields||[]).find(f=>f.field==="format")||{value:"Unavailable"}).value;textFormat:Text.PlainText}
                    Button {
                        objectName:"restoreImportedValues";text:"Restore imported values"
                        enabled:editEnabled && !selected.manual && (selected.sourceFields||[]).some(f=>f.field==="payee")
                        onClicked:restoreImportedValues()
                    }
                    Item {Layout.fillWidth:true}
                }
                TextArea {objectName:"originalSnippet";visible:sourceToggle.enabled && sourceToggle.checked;text:selected.source||"";readOnly:true;selectByMouse:true;textFormat:TextEdit.PlainText;wrapMode:TextEdit.Wrap;Layout.fillWidth:true;Accessible.name:"Original imported transaction"}

            }
        }
    }
    function query(sort, descending) {
        if(workflow.busy) return
        let args={filter:search.text,sort:typeof sort==="number"?sort:(s.sort||0),descending:typeof descending==="boolean"?descending:(s.descending||false),projected:true}
        let change={filter:search.text}
        if(!searchReturn && search.text.length>0) {
            let index=ledger.indexAt(1,ledger.contentY+1)
            let item=ledger.itemAtIndex(index)
            change.start={selection:selected.batch && selected.ordinal!==undefined?{batch:selected.batch,ordinal:selected.ordinal}:{},
                selectionIds:s.selection||[],
                sort:s.sort||0,descending:s.descending||false,projected:s.mode!=="Committed",
                anchor:workflow.rowIdentity(index),offset:item?ledger.contentY-item.y:0,chosen:false}
        } else if(searchReturn && search.text.length===0) {
            change.clear=true
            if(!searchReturn.chosen) {
                args.sort=searchReturn.sort;args.descending=searchReturn.descending;args.projected=searchReturn.projected
                args.restoreSelection=searchReturn.selection
                args.restoreSelectionIds=searchReturn.selectionIds
            }
        }
        pendingSearch=change
        workflow.command("query",args)
    }
}
