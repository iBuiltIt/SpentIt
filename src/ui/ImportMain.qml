import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
ApplicationWindow {
    id: window
    width: 1220; height: 850; minimumWidth:1100; minimumHeight:750; visible: true
    title: "SpentIt — durable QIF experiment (synthetic data only)"
    property var s: workflow.state
    property var selected: s.selected || ({})
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
    function restoreViewport(saved) {
        let index=workflow.occurrenceIndex(saved.anchor.batch || "",saved.anchor.ordinal===undefined?-1:saved.anchor.ordinal)
        if(index>=0) {
            ledger.positionViewAtIndex(index,ListView.Beginning)
            ledger.contentY+=saved.offset
        } else ledger.positionViewAtBeginning()
    }
    property bool dirty: false
    property var pendingAction: null
    property bool allowClose: false
    property bool closingRequested: false
    property bool settingUp: false
    function guard(action) { if (dirty) { pendingAction=action; unsaved.open() } else action() }
    function loadEditor() {
        let d=selected.draft
        splitModel.clear()
        if (d) {
            editDate.text=d.date; editPayee.text=d.payee; editMemo.text=d.memo; editTotal.text=d.total
            for(let a of d.splits) splitModel.append(a)
        }
        dirty=false
    }
    function inspectRecord(batch, ordinal) { guard(function(){workflow.command("inspect",{batch:batch,ordinal:ordinal})}) }
    function copySource(field) {
        if (!selected.editable || workflow.busy) return
        if (field.split >= 0) {
            if (field.split >= splitModel.count) return
            splitModel.setProperty(field.split, field.field, field.value)
        } else {
            switch (field.field) {
            case "date": editDate.text=field.value; break
            case "payee": editPayee.text=field.value; break
            case "memo": editMemo.text=field.value; break
            case "total": editTotal.text=field.value; break
            default: return
            }
        }
        dirty=true
    }
    function saveEditor() {
        let splits=[]
        for(let i=0;i<splitModel.count;i++) {let a=splitModel.get(i);splits.push({category:a.category,memo:a.memo,amount:a.amount})}
        workflow.command("save", {date:editDate.text,payee:editPayee.text,memo:editMemo.text,total:editTotal.text,splits:splits})
    }
    onClosing: function(close) {
        if(allowClose) return
        close.accepted=false
        if(workflow.busy) return
        guard(function(){closingRequested=true;workflow.command("close")})
    }
    Connections {
        target: workflow
        // Drop the old delegate index before a smaller filtered model replaces it.
        // The occurrence selection remains in the workflow and is restored below.
        function onModelAboutToBeReset() { ledger.currentIndex=-1 }
        function onCompleted(action,success) {
            ledger.currentIndex=workflow.selectedRowIndex()
            if (action==="inspect" || action==="import")
                Qt.callLater(function(){if(ledger.currentIndex>=0) ledger.positionViewAtIndex(ledger.currentIndex,ListView.Contain)})
            if(!success && action==="save") {pendingAction=null;projected.checked=s.mode!=="Committed"}
            if (!s.active || (success && (action==="create" || action==="open" || action==="unlock"))) splitViews=({})
            if(action==="query" && pendingSearch) {
                let change=pendingSearch;pendingSearch=null
                if(success) {
                    appliedFilter=change.filter
                    if(change.start) searchReturn=change.start
                    if(change.clear) {
                        let saved=searchReturn;searchReturn=null
                        if(!saved.chosen) loadEditor()
                        Qt.callLater(function(){
                            if(saved.chosen) {if(ledger.currentIndex>=0) ledger.positionViewAtIndex(ledger.currentIndex,ListView.Contain)}
                            else restoreViewport(saved)
                        })
                    }
                }
            }
            if(success && action==="inspect" && searchReturn && workflow.selectedRowIndex()>=0)
                searchReturn.chosen=true
            if(!s.active || (success && (action==="create" || action==="open" || action==="unlock"))) {
                searchReturn=null;pendingSearch=null;appliedFilter="";search.clear()
            }
            if(success) {
                projected.checked=s.mode!=="Committed"
                if(action==="save") {
                    dirty=false
                    if(pendingAction) {let next=pendingAction;pendingAction=null;next()}
                }
                if(action!=="query" && action!=="backup") loadEditor()
                if(action==="create" || action==="unlock") {password.text="";confirmation.text="";unlockPassword.text=""}
                if(action==="close") {if(closingRequested){allowClose=true;Qt.callLater(function(){window.close()})} search.clear();splitModel.clear();editDate.clear();editPayee.clear();editMemo.clear();editTotal.clear();dirty=false}
            }
        }
    }
    Component.onCompleted: {
        if(s.startup) workflow.command("open",{path:s.path})
    }
    ListModel {id:splitModel}
    FileDialog { id:openDialog; title:"Choose an experimental dataset"; onAccepted:{location.text=workflow.localPath(selectedFile);workflow.command("open",{path:location.text})} }
    FileDialog { id:createDialog; fileMode:FileDialog.SaveFile; title:"Choose a NEW dataset file"; onAccepted:location.text=workflow.localPath(selectedFile) }
    FileDialog { id:importDialog; title:"Import synthetic QIF"; nameFilters:["QIF files (*.qif)","All files (*)"]; onAccepted:workflow.command("import",{path:workflow.localPath(selectedFile),profile:profile.currentText}) }
    FileDialog { id:backupDialog; fileMode:FileDialog.SaveFile; title:"Save backup / restored copy to a NEW file"; onAccepted:workflow.command("backup",{path:workflow.localPath(selectedFile)}) }
    Dialog {
        id:unsaved;objectName:"unsavedDialog"; title:"Unsaved draft edit"; modal:true; anchors.centerIn:parent
        ColumnLayout {
            Label {textFormat:Text.PlainText;text:"Save the edit, discard its changes, or stay here?"}
            RowLayout {
                Button {objectName:"guardSave";text:"Save";onClicked:{unsaved.close();saveEditor()}}
                Button {objectName:"guardDiscard";text:"Discard";onClicked:{unsaved.close();loadEditor();let next=pendingAction;pendingAction=null;if(next)next()}}
                Button {objectName:"guardStay";text:"Stay";onClicked:{unsaved.close();pendingAction=null;projected.checked=s.mode!=="Committed"}}
            }
        }
        onRejected:{pendingAction=null;projected.checked=s.mode!=="Committed"}
    }
    Dialog {
        id:discardDialog;title:"Discard pending import?";modal:true;anchors.centerIn:parent
        standardButtons:Dialog.Ok|Dialog.Cancel
        Label {textFormat:Text.PlainText;text:"Remove the draft projection. Encrypted source evidence and history are retained."}
        onAccepted:workflow.command("discard")
    }
    Dialog {
        id:commitDialog;title:"Commit the whole reviewed import?";modal:true;anchors.centerIn:parent
        standardButtons:Dialog.Ok|Dialog.Cancel
        Label {textFormat:Text.PlainText;text:"Publish all included records atomically and freeze the opening balance.\nExcluded duplicate occurrences remain in history."}
        onAccepted:workflow.command("commit")
    }
    ColumnLayout {
        anchors.fill:parent;anchors.margins:12;spacing:8
        RowLayout {
            Label {textFormat:Text.PlainText;text:s.active ? s.name+" · "+s.currency : "Dataset setup and unlocking";font.pixelSize:20;Layout.fillWidth:true}
            BusyIndicator {running:workflow.busy;visible:running;implicitHeight:30}
            Button {text:"Settings / close dataset";visible:!!s.active;enabled:!workflow.busy;onClicked:guard(function(){workflow.command("close")})}
        }
        Label {textFormat:Text.PlainText;text:"Experimental · synthetic inputs only · no password reset";color:"#855100"}
        Label {textFormat:Text.PlainText;text:workflow.error;visible:text.length>0;color:"#a02020";wrapMode:Text.Wrap;Layout.fillWidth:true;Accessible.name:text}
        ScrollView {
            visible:!s.active;Layout.fillWidth:true;Layout.fillHeight:true;enabled:!workflow.busy
            ColumnLayout {
                width:Math.min(window.width-50,720);spacing:12
                Label {textFormat:Text.PlainText;text:"Settings — selected dataset path"}
                TextField {id:location;objectName:"datasetPath";text:s.path || workflow.suggestedPath;Layout.fillWidth:true;placeholderText:"Dataset file path";Accessible.name:"Dataset path"}
                RowLayout {
                    Button {text:"Browse existing…";onClicked:openDialog.open()}
                    Button {text:"Open selected dataset";onClicked:{settingUp=false;workflow.command("open",{path:location.text})}}
                    Button {text:"New dataset…";onClicked:{settingUp=true;protection.checked=true;password.clear();confirmation.clear();currency.currentIndex=-1}}
                }
                Label {textFormat:Text.PlainText;text:"A successful open becomes the startup dataset. Close it before choosing another.\nTo restore a backup, open it and save a copy to a new file.";wrapMode:Text.Wrap;Layout.fillWidth:true}
                GroupBox {
                    title:"Unlock selected dataset";visible:!!s.unlockRequired;Layout.fillWidth:true
                    RowLayout {
                        anchors.fill:parent
                        TextField {id:unlockPassword;objectName:"unlockPassword";echoMode:TextInput.Password;placeholderText:"Password";Layout.fillWidth:true;onAccepted:workflow.command("unlock",{path:location.text,password:text});Accessible.name:"Unlock password"}
                        Button {text:"Unlock";onClicked:workflow.command("unlock",{path:location.text,password:unlockPassword.text})}
                    }
                }
                GroupBox {
                    title:"Create a new experimental dataset";visible:settingUp;Layout.fillWidth:true
                    ColumnLayout {
                        anchors.fill:parent
                        Button {text:"Browse new location…";onClicked:createDialog.open()}
                        TextField {id:accountName;text:"Synthetic account";placeholderText:"Account name";Layout.fillWidth:true;Accessible.name:"Account name"}
                        RowLayout {Label {textFormat:Text.PlainText;text:"Home / account currency"} ComboBox {id:currency;model:workflow.currencies;currentIndex:-1;displayText:currentIndex<0?"Choose currency":currentText;Accessible.name:"Currency"}}
                        CheckBox {id:protection;text:"Password protection";checked:true}
                        TextField {id:password;echoMode:TextInput.Password;visible:protection.checked;placeholderText:"Password";Layout.fillWidth:true;Accessible.name:"New password"}
                        TextField {id:confirmation;echoMode:TextInput.Password;visible:protection.checked;placeholderText:"Confirm password";Layout.fillWidth:true;Accessible.name:"Confirm password"}
                        Label {textFormat:Text.PlainText;visible:!protection.checked;text:"No confidentiality: anyone with this file or its backups can read it using the public format key.";wrapMode:Text.Wrap;Layout.fillWidth:true;color:"#a02020"}
                        Button {text:"Create dataset";enabled:currency.currentIndex>=0;onClicked:workflow.command("create",{path:location.text,name:accountName.text,currency:currency.currentText,protected:protection.checked,password:password.text,confirmation:confirmation.text})}
                    }
                }
            }
        }
        ColumnLayout {
            visible:!!s.active;Layout.fillWidth:true;Layout.fillHeight:true;enabled:!workflow.busy
            Label {textFormat:Text.PlainText;text:s.path || "";elide:Text.ElideMiddle;Layout.fillWidth:true}
            RowLayout {
                Label {textFormat:Text.PlainText;text:"Committed: "+(s.committed||"")+"   Projected: "+(s.projected||"");Layout.fillWidth:true}
                ComboBox {id:profile;Layout.preferredWidth:200;model:["Choose date order","DMY","MDY"];Accessible.name:"QIF date order"}
                Button {text:"Import QIF…";enabled:profile.currentIndex>0 && !s.pending;onClicked:guard(function(){importDialog.open()})}
                Button {text:"Backup / restore copy…";onClicked:guard(function(){backupDialog.open()})}
            }
            RowLayout {
                visible:!s.frozen
                Label {textFormat:Text.PlainText;text:"Opening balance (confirm before commit)"}
                TextField {id:openingDate;text:s.openingDate||"";placeholderText:"YYYY-MM-DD";Accessible.name:"Opening date"}
                TextField {id:openingAmount;text:s.opening||"";placeholderText:"Exact amount";Accessible.name:"Opening amount"}
                Button {text:s.confirmed?"Update opening":"Confirm opening";onClicked:guard(function(){workflow.command("opening",{date:openingDate.text,amount:openingAmount.text})})}
            }
            RowLayout {
                ComboBox {id:batchPicker;model:s.batches||[];textRole:"id";Layout.preferredWidth:240;Accessible.name:"Import batch"}
                SpinBox {id:recordNumber;from:1;to:batchPicker.currentIndex>=0?Math.max(1,batchPicker.model[batchPicker.currentIndex].count):1;editable:true;Accessible.name:"Source record number"}
                Button {text:"Review record";enabled:batchPicker.currentIndex>=0 && batchPicker.model[batchPicker.currentIndex].count>0;onClicked:inspectRecord(batchPicker.model[batchPicker.currentIndex].id,recordNumber.value-1)}
                Button {text:"Source file";enabled:batchPicker.currentIndex>=0;onClicked:guard(function(){workflow.command("evidence",{batch:batchPicker.model[batchPicker.currentIndex].id});sourceToggle.checked=true})}
                Label {textFormat:Text.PlainText;text:batchPicker.currentIndex>=0?batchPicker.model[batchPicker.currentIndex].status:"No imports"}
                Item {Layout.fillWidth:true}
                Button {text:"Discard batch…";enabled:!!s.pending;onClicked:guard(function(){discardDialog.open()})}
                Button {text:"Commit batch…";enabled:!!s.pending && !!s.projectionAvailable && !!s.confirmed;onClicked:guard(function(){commitDialog.open()})}
            }
            Label {textFormat:Text.PlainText;visible:!!s.pending && !!s.pending.blocked;text:s.pending && s.pending.blocked ? "BLOCKED — no partial commit. Inspect each record. Date and split-total repairs are available in new datasets; other errors require discard/reimport.\n"+(s.pending.diagnostics||""):"";color:"#a02020";wrapMode:Text.Wrap;Layout.fillWidth:true;maximumLineCount:5;elide:Text.ElideRight}
            Label {textFormat:Text.PlainText;visible:!!s.receipt && !!s.receipt.operation;text:s.receipt && s.receipt.operation ? "Committed "+s.receipt.operation+" · included "+s.receipt.included+", excluded "+s.receipt.excluded+" · "+s.receipt.before+" + ("+s.receipt.change+") = "+s.receipt.after:"";wrapMode:Text.Wrap;Layout.fillWidth:true}
            RowLayout {
                CheckBox {id:projected;checked:true;text:"Projected ledger";onClicked:guard(query)}
                TextField {id:search;objectName:"ledgerSearch";placeholderText:"Literal whole-record filter";maximumLength:256;Layout.fillWidth:true;onAccepted:guard(query);Keys.onEscapePressed:guard(function(){search.clear();query()});Accessible.name:"Ledger filter"}
                Button {objectName:"applyFilter";text:"Apply filter";onClicked:guard(query)}
                Button {objectName:"clearFilter";text:"Clear";enabled:search.text.length>0 || appliedFilter.length>0;onClicked:guard(function(){search.clear();query()})}
                Label {textFormat:Text.PlainText;text:"Mode: "+(s.mode||"")}
            }
            RowLayout {
                Repeater {model:["Date","Payee","Category","Withdrawal","Deposit","Balance"]
                    Button {required property string modelData;required property int index;text:modelData+(index===(s.sort||0)?(s.descending?" ▼":" ▲"):"");Layout.fillWidth:true;onClicked:guard(function(){query(index,s.sort===index?!s.descending:false)})}
                }
            }
            ListView {
                id:ledger;objectName:"durableLedger";Layout.fillWidth:true;Layout.fillHeight:true;clip:true;model:workflow;reuseItems:true;highlightFollowsCurrentItem:false
                ScrollBar.vertical:ScrollBar {}
                delegate:Rectangle {
                    required property var record
                    id: ledgerRow
                    property bool expanded: record.batch===selected.batch && record.ordinal===selected.ordinal
                    property bool viewingSplits: !!window.splitViews[window.splitKey(record)] && record.splits.length>1
                    property alias editorSlot: rowEditorSlot
                    width:ledger.width;height:34+(expanded?editor.height:splitPreview.height);color:record.status==="Draft"?"#fff6de":"#f3f5f7"
                    MouseArea {width:parent.width;height:34;onDoubleClicked:if(record.batch.length)inspectRecord(record.batch,record.ordinal)}
                    Row {
                        width:parent.width;height:34;spacing:4
                        Repeater {model:[record.date,record.payee+(record.status==="Draft"?" [Draft]":""),record.category,record.withdrawal,record.deposit,record.balance]
                            Item {
                                required property string modelData
                                required property int index
                                width:(ledger.width-24)/6;height:34
                                Label {anchors.fill:parent;textFormat:Text.PlainText;text:modelData;elide:Text.ElideRight;verticalAlignment:Text.AlignVCenter;visible:index!==2 || record.splits.length<2}
                                Button {
                                    anchors.fill:parent;visible:index===2 && record.splits.length>1
                                    objectName:index===2 && record.batch.length?"splitToggle_"+record.ordinal:""
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
                        x:12;y:34;width:parent.width-24
                        active:ledgerRow.viewingSplits && !ledgerRow.expanded
                        height:item?Math.min(160,item.contentHeight):0
                        visible:active
                        sourceComponent:ListView {
                        clip:true;reuseItems:true;model:record.splits
                        ScrollBar.vertical:ScrollBar {}
                        header:Label {text:"Saved splits (read-only) · "+(s.currency||"");height:26}
                        delegate:Row {
                            required property var modelData
                            width:splitPreview.width;height:30;spacing:8
                            Label {textFormat:Text.PlainText;text:modelData.category || "Uncategorized";width:parent.width*0.25;elide:Text.ElideRight}
                            Label {textFormat:Text.PlainText;text:modelData.memo;width:parent.width*0.5;elide:Text.ElideRight}
                            Label {textFormat:Text.PlainText;text:modelData.amount;width:parent.width*0.2;horizontalAlignment:Text.AlignRight}
                        }
                    }
                    }
                    Item {id:rowEditorSlot;y:34;width:parent.width;height:ledgerRow.expanded?editor.height:0}
                }
            }
            Label {textFormat:Text.PlainText;text: "Filtered totals · withdrawals (signed): "+(s.withdrawals||"unavailable")+" · deposits: "+(s.deposits||"unavailable")}
            Item {
                id:reviewPanel;objectName:"reviewPanel"
                Layout.fillWidth:true;Layout.preferredHeight:editor.visible && !editor.inlineRow?editor.height:0
            }
        }
    }
    Frame {
        id:editor;objectName:"transactionEditor"
        // One persistent editor, visually reparented into the current row.
        // Its fields and split buffer never belong to a recycled delegate.
        property bool inlineRow: !!ledger.currentItem && ledger.currentItem.expanded
        parent:inlineRow?ledger.currentItem.editorSlot:reviewPanel
        visible:!!s.active && !!selected.batch
        enabled:!workflow.busy
        width:parent.width;height:330
        ScrollView {
            anchors.fill:parent;contentWidth:availableWidth
            ColumnLayout {
                width:editor.availableWidth
                Label {textFormat:Text.PlainText;text:(selected.ordinal===undefined?"Source file":"Record "+(selected.ordinal+1))+" · "+(selected.batch||"")+" · "+(selected.editable?"Durable draft editor":"Read only")+" · original "+(selected.original||"");wrapMode:Text.Wrap;Layout.fillWidth:true}
                Label {textFormat:Text.PlainText;text:selected.diagnostic||"";color:"#a02020";visible:text.length>0;wrapMode:Text.Wrap;Layout.fillWidth:true}
                RowLayout {
                    visible:!!selected.draft
                    TextField {id:editDate;objectName:"durableDate";placeholderText:"YYYY-MM-DD";readOnly:!selected.editable;onAccepted:if(selected.editable)saveEditor();onTextEdited:dirty=true;Accessible.name:"Transaction date"}
                    TextField {id:editPayee;objectName:"durablePayee";placeholderText:"Payee";readOnly:!selected.editable;onAccepted:if(selected.editable)saveEditor();onTextEdited:dirty=true;Layout.fillWidth:true;Accessible.name:"Payee"}
                    TextField {id:editTotal;objectName:"durableTotal";placeholderText:"Exact total";readOnly:!selected.editable;onAccepted:if(selected.editable)saveEditor();onTextEdited:dirty=true;Accessible.name:"Total"}
                }
                TextField {id:editMemo;visible:!!selected.draft;placeholderText:"Memo";readOnly:!selected.editable;onAccepted:if(selected.editable)saveEditor();onTextEdited:dirty=true;Layout.fillWidth:true;Accessible.name:"Memo"}
                Repeater {
                    model:splitModel
                    RowLayout {
                        required property int index
                        required property string category
                        required property string memo
                        required property string amount
                        TextField {text:category;placeholderText:"Category";readOnly:!selected.editable;onAccepted:if(selected.editable)saveEditor();onTextEdited:{splitModel.setProperty(index,"category",text);dirty=true} Accessible.name:"Split category"}
                        TextField {text:memo;placeholderText:"Split memo";Layout.fillWidth:true;readOnly:!selected.editable;onAccepted:if(selected.editable)saveEditor();onTextEdited:{splitModel.setProperty(index,"memo",text);dirty=true} Accessible.name:"Split memo"}
                        TextField {text:amount;placeholderText:"Exact split amount";readOnly:!selected.editable;onAccepted:if(selected.editable)saveEditor();onTextEdited:{splitModel.setProperty(index,"amount",text);dirty=true} Accessible.name:"Split amount"}
                        Button {text:"Remove";visible:!!selected.editable;onClicked:{splitModel.remove(index);dirty=true}}
                    }
                }
                RowLayout {
                    visible:!!selected.editable
                    Button {text:"Add split";enabled:splitModel.count<1000;onClicked:{splitModel.append({category:"",memo:"",amount:"0"});dirty=true}}
                    Button {objectName:"saveDraft";text:selected.repair?"Save repair":"Save draft";onClicked:saveEditor()}
                    Button {text:"Cancel edit";onClicked:loadEditor()}
                    Label {textFormat:Text.PlainText;text:dirty?"Unsaved changes":selected.repair?"Repair not yet saved":"Last saved revision"}
                }
                RowLayout {
                    visible:!!selected.editable && !selected.repair
                    Label {textFormat:Text.PlainText;text:selected.decision===0?"Unresolved match":selected.decision===2?"Excluded duplicate: "+(selected.duplicate||""):"Include as new"}
                    Button {text:"Include as new";onClicked:guard(function(){workflow.command("decide",{duplicate:false})})}
                    ComboBox {id:candidate;model:selected.candidates||[];textRole:"label";Layout.fillWidth:true;Accessible.name:"Duplicate occurrence"}
                    Button {text:"Exclude against selected occurrence";enabled:candidate.currentIndex>=0;onClicked:guard(function(){let c=candidate.model[candidate.currentIndex];workflow.command("decide",{duplicate:true,batch:c.batch,ordinal:c.ordinal})})}
                }
                CheckBox {id:sourceToggle;objectName:"sourceToggle";text:"Show immutable QIF source"}
                Label {
                    visible:sourceToggle.checked;Layout.fillWidth:true;wrapMode:Text.Wrap
                    text:"Parsed source fields — double-click or use Copy to edit. Dates use the import date order; amounts use the account scale. Copies are unsaved and must pass validation. Split destinations are numbered by their current editor position."
                    textFormat:Text.PlainText
                }
                Repeater {
                    model:sourceToggle.checked?(selected.sourceFields||[]):[]
                    RowLayout {
                        required property var modelData
                        property var sourceField:modelData
                        Layout.fillWidth:true
                        Label {text:sourceField.label; textFormat:Text.PlainText;Layout.preferredWidth:300;wrapMode:Text.Wrap}
                        Label {
                            text:sourceField.value.length?sourceField.value:"(empty)";textFormat:Text.PlainText
                            Layout.fillWidth:true;wrapMode:Text.Wrap
                            MouseArea {anchors.fill:parent;enabled:!!selected.editable;onDoubleClicked:copySource(sourceField)}
                        }
                        Button {
                            text:"Copy to edit";objectName:"copySource_"+sourceField.field+"_"+sourceField.split
                            enabled:!!selected.editable && (sourceField.split<0 || sourceField.split<splitModel.count)
                            Accessible.name:"Copy "+sourceField.label+" to edit"
                            onClicked:copySource(sourceField)
                        }
                    }
                }
                TextArea {visible:sourceToggle.checked;text:selected.source||"";readOnly:true;selectByMouse:true;textFormat:TextEdit.PlainText;wrapMode:TextEdit.Wrap;Layout.fillWidth:true;Accessible.name:"Immutable QIF source"}
            }
        }
    }
    function query(sort, descending) {
        if(workflow.busy) return
        let args={filter:search.text,sort:typeof sort==="number"?sort:(s.sort||0),descending:typeof descending==="boolean"?descending:(s.descending||false),projected:projected.checked}
        let change={filter:search.text}
        if(!searchReturn && search.text.length>0) {
            let index=ledger.indexAt(1,ledger.contentY+1)
            let item=ledger.itemAtIndex(index)
            change.start={selection:selected.batch && selected.ordinal!==undefined?{batch:selected.batch,ordinal:selected.ordinal}:{},
                sort:s.sort||0,descending:s.descending||false,projected:s.mode!=="Committed",
                anchor:workflow.rowIdentity(index),offset:item?ledger.contentY-item.y:0,chosen:false}
        } else if(searchReturn && search.text.length===0) {
            change.clear=true
            if(!searchReturn.chosen) {
                args.sort=searchReturn.sort;args.descending=searchReturn.descending;args.projected=searchReturn.projected
                args.restoreSelection=searchReturn.selection
            }
        }
        pendingSearch=change
        workflow.command("query",args)
    }
}
