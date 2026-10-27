import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
ApplicationWindow {
    id: window
    width: 1440; height: 850; minimumWidth:1100; minimumHeight:750; visible: true
    title: "SpentIt — durable QIF experiment (synthetic data only)"
    property var s: workflow.state
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
        if(!s.accountId || ledger.count===0)return
        let first=ledger.indexAt(1,ledger.contentY+1)
        if(first<0)first=0
        let item=ledger.itemAtIndex(first)
        let views=Object.assign({},accountViewports)
        views[s.accountId]={anchor:workflow.rowIdentity(first),offset:item?ledger.contentY-item.y:0}
        accountViewports=views
    }
    function showCounterpart(item) {
        transferDialog.close()
        guard(function(){
            rememberAccountViewport()
            pendingCounterpart={batch:item.counterpartBatch,ordinal:item.counterpartOrdinal}
            workflow.command("select_account",{id:item.counterpartAccount,reveal:true,batch:item.counterpartBatch,ordinal:item.counterpartOrdinal})
        })
    }
    Dialog {
        id:transferDialog;objectName:"transferDialog";title:"Transfers · "+(s.name||"")
        anchors.centerIn:parent;width:Math.min(window.width-60,1000);height:Math.min(window.height-60,680);modal:true;standardButtons:Dialog.Close
        contentItem:ScrollView {
            clip:true
            ColumnLayout {
                width:transferDialog.availableWidth;spacing:12;enabled:!workflow.busy
                Label {text:"Each account commits separately. Acceptance without a bank match preserves provenance and permits a later match.";wrapMode:Text.Wrap;Layout.fillWidth:true}
                GroupBox {
                    title:"Resolve an imported destination or create a transfer";Layout.fillWidth:true
                    ColumnLayout {
                        anchors.fill:parent
                        RowLayout {
                            Label {text:"Destination"}
                            ComboBox {id:transferDestination;objectName:"transferDestination";model:transferDestinations();textRole:"name";currentIndex:0;onModelChanged:currentIndex=0;Layout.fillWidth:true;Accessible.name:"Transfer destination account"}
                        }
                        RowLayout {
                            TextField {id:transferDate;objectName:"transferDate";text:Qt.formatDate(new Date(),"yyyy-MM-dd");placeholderText:"YYYY-MM-DD";Accessible.name:"Manual transfer date"}
                            TextField {id:transferAmount;objectName:"transferAmount";placeholderText:"Signed amount in "+(s.currency||"");Accessible.name:"Manual transfer amount"}
                            TextField {id:transferPayee;objectName:"transferPayee";placeholderText:"Payee";Layout.fillWidth:true;Accessible.name:"Manual transfer payee"}
                            Button {objectName:"createManualTransfer";text:"Create manual draft";enabled:transferDestination.currentIndex>0 && transferAmount.text.length>0;onClicked:transferCommand("transfer_manual",{link:workflow.newIdentity(),destination:transferDestination.model[transferDestination.currentIndex].id,date:transferDate.text,amount:transferAmount.text,payee:transferPayee.text,memo:transferMemo.text})}
                        }
                        Label {text:"QIF [Account] names are source evidence. Choose the actual destination explicitly, even when names match. If a transfer already exists below, match its bank allocation instead of creating another movement. Create missing accounts from Accounts first.";textFormat:Text.PlainText;wrapMode:Text.Wrap;Layout.fillWidth:true}
                        TextField {id:transferMemo;placeholderText:"Memo (optional)";Layout.fillWidth:true;Accessible.name:"Manual transfer memo"}
                        RowLayout {
                            Label {text:"Selected record allocation"}
                            ComboBox {id:transferAllocation;objectName:"transferAllocation";model:(selected.draft||{}).splits||[];textRole:"transferLabel";Layout.fillWidth:true;Accessible.name:"Selected transfer allocation"}
                            Button {objectName:"assignTransfer";text:"Use as transfer";enabled:!!selected.transferEligible && transferAllocation.currentIndex>=0 && transferDestination.currentIndex>0;onClicked:transferCommand("transfer_from_draft",{link:workflow.newIdentity(),destination:transferDestination.model[transferDestination.currentIndex].id,allocation:transferAllocation.model[transferAllocation.currentIndex].allocationId})}
                        }
                    }
                }
                Label {text:(s.transferReceipt||{}).summary||"";textFormat:Text.PlainText;wrapMode:Text.Wrap;Layout.fillWidth:true}
                Repeater {
                    model:s.transfers||[]
                    delegate:GroupBox {
                        required property var modelData
                        Layout.fillWidth:true;title:modelData.label
                        ColumnLayout {
                            anchors.fill:parent
                            Label {text:(modelData.committed?"Committed":"Draft")+(modelData.protected?" · Protected by the other account":"")+(modelData.provisional?" · Provisional date":"")+(modelData.evidenced?" · Bank evidence retained":modelData.accepted?" · Accepted without bank match":" · Needs review")+(modelData.proposed?" · Match proposed; commit its import to attach":"");wrapMode:Text.Wrap;Layout.fillWidth:true}
                            RowLayout {
                                Button {text:"Accept without bank match";visible:!modelData.evidenced && !modelData.accepted;enabled:!modelData.proposed;onClicked:transferCommand("transfer_accept",{link:modelData.id})}
                                Button {text:"Commit this account";visible:modelData.local && !modelData.committed;enabled:modelData.accepted && !modelData.proposed && !!s.confirmed;onClicked:{transferCommit.item=modelData;transferCommit.open()} }
                                Button {text:"Discard manual origin";visible:modelData.isOrigin && modelData.local && !modelData.committed && !modelData.protected;onClicked:transferCommand("transfer_clear",{link:modelData.id})}
                                Button {text:"Show counterpart ↗";onClicked:showCounterpart(modelData)}
                            }
                            RowLayout {
                                Button {text:"Find bank candidates";visible:!modelData.evidenced && !modelData.proposed;onClicked:workflow.command("transfer_candidates",{link:modelData.id})}
                                Button {text:"Match selected allocation";visible:!modelData.evidenced && !modelData.proposed;enabled:!!selected.transferEligible && transferAllocation.currentIndex>=0;onClicked:transferCommand("transfer_match",{link:modelData.id,allocation:transferAllocation.model[transferAllocation.currentIndex].allocationId})}
                                Button {text:"Drop match proposal";visible:modelData.proposed;onClicked:transferCommand("transfer_unmatch",{link:modelData.id})}
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
                                        Button {text:"Confirm match proposal";onClicked:transferCommand("transfer_match",{link:s.candidateLink,batch:modelData.batch,ordinal:modelData.ordinal,allocation:modelData.allocation})}
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    Dialog {
        id:transferCommit;width:520;property var item:({});title:"Commit transfer in this account?";modal:true;anchors.centerIn:parent;standardButtons:Dialog.Ok|Dialog.Cancel
        onAccepted:transferCommand("transfer_commit",{link:item.id})
        Label {text:(transferCommit.item.label||"")+"\nOnly "+(s.name||"")+" commits. The opposite draft stays uncommitted and protected.";wrapMode:Text.Wrap;width:480}
    }
    Dialog {
        id:protectedCommit;width:520;property var item:({});title:"Commit protected drafts?";modal:true;anchors.centerIn:parent;standardButtons:Dialog.Ok|Dialog.Cancel
        onAccepted:guard(function(){workflow.command("commit",{batch:protectedCommit.item.id})})
        Label {text:"Commit retained drafts from "+(protectedCommit.item.id||"")+" in "+(s.name||"")+"?\nChange: "+(protectedCommit.item.change||"")+" · resulting balance: "+(protectedCommit.item.after||"")+"\nOther accounts and other pending imports remain uncommitted.";wrapMode:Text.Wrap;width:480}
    }
    Menu {
        id:ledgerContext
        MenuItem {text:"Delete selected…";enabled:!!s.bulkSupported && !workflow.busy && (s.selectionCount||0)>0;onTriggered:guard(function(){workflow.command("preview_bulk",{kind:"remove"})})}
        MenuItem {text:"Change category…";enabled:!!s.bulkSupported && !workflow.busy && (s.selectionCount||0)>0;onTriggered:{bulkCategory.forceActiveFocus();bulkCategory.popup.open()}}
    }
    function restoreViewport(saved) {
        let index=workflow.occurrenceIndex(saved.anchor.batch || "",saved.anchor.ordinal===undefined?-1:saved.anchor.ordinal)
        if(index>=0) {
            ledger.positionViewAtIndex(index,ListView.Beginning)
            ledger.contentY+=saved.offset
        } else ledger.positionViewAtBeginning()
    }
    // Navigation is local to this document; pages stay alive to retain ledger view state.
    property string currentPage: "ledger"
    property var navigation: ["ledger"]
    property int navigationIndex: 0
    property var navigationAccounts: [""]
    property var pendingAccountNavigation: null
    property bool addingAccount: false
    property string accountCreationId: ""
    property string accountCreationDate: ""
    property bool sidebarVisible: true
    function resetNavigation() {
        accountViewports={};pendingCounterpart=null;currentPage="ledger"; navigation=["ledger"]; navigationAccounts=[s.accountId||""]; navigationIndex=0;pendingAccountNavigation=null
    }
    function navigate(page) {
        if(workflow.busy || !s.active || page===currentPage) return
        guard(function(){
            let entries=navigation.slice(0,navigationIndex+1)
            entries.push(page);navigation=entries;let accounts=navigationAccounts.slice(0,navigationIndex+1);accounts.push(s.accountId);navigationAccounts=accounts;navigationIndex=entries.length-1;currentPage=page
        })
    }
    function chooseAccount(id) {
        if(workflow.busy) return
        if(id===s.accountId) {navigate("ledger");return}
        guard(function(){rememberAccountViewport();workflow.command("select_account",{id:id})})
    }
    function revealHistoryOperation(operation) {
        let entries=s.entryHistory||[]
        for(let i=0;i<entries.length;i++) if(entries[i].operation===operation) {
            entryHistoryList.positionViewAtIndex(i,ListView.Contain);return
        }
    }
    function traverseNavigation(delta) {
        let destination=navigationIndex+delta
        if(workflow.busy || !s.active || destination<0 || destination>=navigation.length) return
        guard(function(){
            if(navigationAccounts[destination]!==s.accountId) {
                rememberAccountViewport()
                pendingAccountNavigation={index:destination,page:navigation[destination]}
                workflow.command("select_account",{id:navigationAccounts[destination]})
            } else {navigationIndex=destination;currentPage=navigation[destination]}
        })
    }
    Shortcut {sequence:"Ctrl+Shift+B";enabled:!!s.active;onActivated:sidebarVisible=!sidebarVisible}
    Shortcut {sequences:[StandardKey.Back];enabled:!!s.active && !workflow.busy;onActivated:traverseNavigation(-1)}
    Shortcut {sequences:[StandardKey.Forward];enabled:!!s.active && !workflow.busy;onActivated:traverseNavigation(1)}
    property bool payeeTouched: false
    property bool memoTouched: false
    property bool categoryDirty: false
    property bool rulesDirty: false
    property int ruleIndex: -1
    property string ruleCategoryId: ""
    property bool guardHandoff: false
    property bool ruleSync: false
    property string categoryAction: ""
    property string categoryFormId: ""
    property string categoryFormRevision: ""
    property int pickerIndex: -1
    function loadCategoryForm() {
        let c=s.categorySelected||{}
        categoryFormId=c.id||"";categoryFormRevision=s.catalogRevision||""
        categoryName.text=c.name||"";categoryParent.currentIndex=0
        for(let i=0;i<categoryParent.model.length;i++) if(categoryParent.model[i].id===(c.parent||""))categoryParent.currentIndex=i
        categoryKind.currentIndex=c.group?1:0;categoryAction="";categoryDirty=false
    }
    function newCategory() {
        categoryFormId="";categoryFormRevision=s.catalogRevision||"";categoryName.clear();categoryParent.currentIndex=0;categoryKind.currentIndex=0;categoryAction="create";categoryDirty=true
    }
    function categoryParents() {
        return [{id:"",path:"Standalone / no parent"}].concat((s.categories||[]).filter(function(c){return c.group && !c.archived && !c.deleted}))
    }
    function saveCategoryForm(action) {
        workflow.command("preview_category",{action:action||categoryAction,id:categoryFormId,revision:categoryFormRevision,
            name:categoryName.text,parent:categoryKind.currentIndex===1?"":categoryParent.currentIndex>=0?categoryParent.model[categoryParent.currentIndex].id:"",group:categoryKind.currentIndex===1})
    }
    function openCategoryPicker(index) {
        pickerIndex=index;let row=splitModel.get(index);unmappedCategory.text=row.category;categorySearch.clear();categoryPicker.open()
    }
    function chooseCategory(id,path) {
        if(pickerIndex===-2) { ruleCategoryId=id||""; writeRule(); categoryPicker.close(); return }
        if(pickerIndex<0 || pickerIndex>=splitModel.count)return
        splitModel.setProperty(pickerIndex,"category",path)
        splitModel.setProperty(pickerIndex,"categoryId",id)
        splitModel.setProperty(pickerIndex,"bindingRevision",id?(s.catalogRevision||""):"")
        splitModel.setProperty(pickerIndex,"categoryDisplay",id?path:path?"Unmapped: "+path:"Uncategorized")
        splitModel.setProperty(pickerIndex,"categoryTouched",true);dirty=true;categoryPicker.close()
    }
    property bool committedEditing: false
    property bool editEnabled: !!selected.editable || (committedEditing && !!selected.canCorrect)
    property bool dirty: false
    property var pendingAction: null
    property bool allowClose: false
    property bool closingRequested: false
    property bool settingUp: false
    function guard(action) { if (dirty) { pendingAction=action; unsaved.open() } else if(categoryDirty) {pendingAction=action;categoryUnsaved.open()} else if(rulesDirty) {pendingAction=action;rulesUnsaved.open()} else action() }
    function dismiss(dialog) { guardHandoff=true; dialog.close(); Qt.callLater(function(){guardHandoff=false}) }
    function loadEditor() {
        let d=selected.draft
        splitModel.clear()
        if (d) {
            editDate.text=d.date; editPayee.text=d.payee; editMemo.text=d.memo; editTotal.text=d.total
            for(let a of d.splits) splitModel.append(a)
        }
        dirty=false;committedEditing=false;payeeTouched=false;memoTouched=false
    }
    function wholeRecognized(expr) { let body=expr.startsWith("(?-i)")?expr.slice(5):expr; return body.startsWith("^(?:") && body.endsWith(")$") && body.length>=6 }
    function ruleRow(rule) {
        let copy=Object.assign({payeeTemplate:"",memoTemplate:"",categoryId:"",inheritParent:false,label:"",expression:"",scope:"transaction",input:"payee"}, rule||{})
        copy.ruleOn=!!rule.enabled
        delete copy.enabled
        return copy
    }
    function loadRulesForm() {
        ruleSync=true; ruleModel.clear()
        for(let rule of (s.rules||[])) ruleModel.append(ruleRow(rule))
        ruleIndex=-1; ruleCategoryId=""; rulesDirty=false; ruleSync=false
    }
    function showRulesPage() {
        if(currentPage==="rules") return
        let entries=navigation.slice(0,navigationIndex+1); entries.push("rules"); navigation=entries; navigationIndex=entries.length-1; currentPage="rules"
    }
    function rulesImage() {
        let rows=[]
        for(let i=0;i<ruleModel.count;i++) { let r=ruleModel.get(i); rows.push({id:r.id,label:r.label||"",enabled:!!r.ruleOn,scope:r.scope||"transaction",input:r.input||"payee",expression:r.expression||"",payeeTemplate:r.payeeTemplate||"",memoTemplate:r.memoTemplate||"",categoryId:r.categoryId||"",inheritParent:!!r.inheritParent}) }
        return rows
    }
    function writeRule() {
        if(ruleSync || ruleIndex<0 || ruleIndex>=ruleModel.count) return
        let inherit=ruleScope.currentIndex===1 && ruleInherit.checked
        ruleModel.setProperty(ruleIndex,"label",ruleLabel.text)
        ruleModel.setProperty(ruleIndex,"expression",ruleExpression.text)
        ruleModel.setProperty(ruleIndex,"scope",ruleScope.currentIndex===1?"split":"transaction")
        ruleModel.setProperty(ruleIndex,"input",ruleInput.currentIndex===1?"memo":ruleInput.currentIndex===2?"category":"payee")
        ruleModel.setProperty(ruleIndex,"payeeTemplate",ruleScope.currentIndex===1?"":rulePayeeTemplate.text)
        ruleModel.setProperty(ruleIndex,"memoTemplate",ruleMemoTemplate.text)
        ruleModel.setProperty(ruleIndex,"inheritParent",inherit)
        ruleModel.setProperty(ruleIndex,"categoryId",inherit?"":ruleCategoryId)
        rulesDirty=true
    }
    function openRule(index) {
        if(index<0 || index>=ruleModel.count) {ruleIndex=-1; return}
        ruleIndex=index; let r=ruleModel.get(index); ruleSync=true
        ruleLabel.text=r.label||""; ruleExpression.text=r.expression||""
        ruleScope.currentIndex=r.scope==="split"?1:0
        ruleInput.currentIndex=r.input==="memo"?1:r.input==="category"?2:0
        rulePayeeTemplate.text=r.payeeTemplate||""; ruleMemoTemplate.text=r.memoTemplate||""
        ruleCategoryId=r.categoryId||""; ruleInherit.checked=!!r.inheritParent
        ruleCaseSensitive.checked=(r.expression||"").startsWith("(?-i)")
        ruleWholeField.checked=wholeRecognized(r.expression||"")
        ruleAssistNote.text=""; ruleSync=false
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
        writeRule()
    }
    function setWhole(on) {
        let expr=ruleExpression.text
        let prefix=expr.startsWith("(?-i)")?"(?-i)":""
        let body=prefix?expr.slice(5):expr
        let wrapped=body.startsWith("^(?:") && body.endsWith(")$") && body.length>=6
        if(on && !wrapped) expr=prefix+"^(?:"+body+")$"
        else if(!on && wrapped) expr=prefix+body.slice(4,body.length-2)
        if(expr!==ruleExpression.text) ruleExpression.text=expr
        writeRule()
    }
    function syncAssist() { ruleSync=true; ruleCaseSensitive.checked=ruleExpression.text.startsWith("(?-i)"); ruleWholeField.checked=wholeRecognized(ruleExpression.text); ruleSync=false }
    function newRule() {
        if(ruleModel.count>=128) return
        ruleModel.append(ruleRow({id:workflow.newIdentity(),enabled:true}))
        rulesDirty=true; openRule(ruleModel.count-1)
    }
    function moveRule(index, delta) {
        let next=index+delta
        if(next<0 || next>=ruleModel.count) return
        ruleModel.move(index,next,1)
        if(ruleIndex===index) ruleIndex=next; else if(ruleIndex===next) ruleIndex=index
        rulesDirty=true
    }
    function toggleRule(index) { let row=ruleModel.get(index); if(!row) return; ruleModel.setProperty(index,"ruleOn",!row.ruleOn); rulesDirty=true }
    function deleteRule(index) {
        ruleModel.remove(index)
        if(ruleIndex===index) ruleIndex=-1; else if(ruleIndex>index) ruleIndex-=1
        rulesDirty=true
    }
    function copyRemoved(id) {
        if(ruleModel.count>=128) return
        let tomb=null
        for(let item of (s.ruleTombstones||[])) if(item.id===id) tomb=item
        if(!tomb) return
        ruleModel.append(ruleRow({id:workflow.newIdentity(),label:tomb.label||"",enabled:true,scope:tomb.scope,input:tomb.input,expression:tomb.expression||"",payeeTemplate:tomb.payeeTemplate||"",memoTemplate:tomb.memoTemplate||"",categoryId:tomb.categoryId||"",inheritParent:!!tomb.inheritParent}))
        rulesDirty=true; openRule(ruleModel.count-1)
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
    function saveRules() { workflow.command("rules_save",{rules:rulesImage()}) }
    function previewRules() {
        let pending=s.pending||{}
        let args={replaceManual:replaceManualFields.checked,unsaved:rulesDirty,batch:pending.id||"",batchRevision:String(pending.revision===undefined?"":pending.revision)}
        if(rulesDirty) args.rules=rulesImage()
        workflow.command("rules_preview",args)
    }
    function applyReady() {
        if(workflow.busy || rulesDirty) return false
        let p=s.rulePreview||{}; let pending=s.pending||{}
        if(!p.operation || p.unsaved) return false
        if(String(p.batchRevision)!==String(pending.revision)) return false
        if(String(p.rulesetRevision)!==String(s.ruleRevision||"")) return false
        if(String(p.catalogRevision)!==String(s.catalogRevision||"")) return false
        if(!!p.replaceManual!==replaceManualFields.checked) return false
        return true
    }
    function applyRules() {
        let p=s.rulePreview||{}
        workflow.command("rules_apply",{batch:p.batch,batchRevision:String(p.batchRevision),rulesetRevision:String(p.rulesetRevision),catalogRevision:String(p.catalogRevision),replaceManual:!!p.replaceManual})
    }
    function createFromSource(kind, allocation) {
        if(workflow.busy || !s.rulesSupported) return
        if(dirty) { pendingAction=function(){createFromSource(kind,allocation)}; unsaved.open(); return }
        if(categoryDirty) { pendingAction=function(){createFromSource(kind,allocation)}; categoryUnsaved.open(); return }
        let args={kind:kind}; if(allocation!==undefined) args.allocation=allocation
        workflow.command("rules_prefill",args)
    }
    function editMatched(id) {
        if(workflow.busy) return
        if(dirty) { pendingAction=function(){editMatched(id)}; unsaved.open(); return }
        if(categoryDirty) { pendingAction=function(){editMatched(id)}; categoryUnsaved.open(); return }
        if(!rulesDirty) loadRulesForm()
        for(let i=0;i<ruleModel.count;i++) if(ruleModel.get(i).id===id) { openRule(i); showRulesPage(); matchedRuleDialog.close(); return }
    }
    function openMatched() {
        let ids=[]
        for(let item of (selected.ruleExplanations||[])) if(ids.indexOf(item.rule)<0) ids.push(item.rule)
        if(ids.length===1) editMatched(ids[0]); else matchedRuleDialog.open()
    }
    function acceptPrefill() {
        if(!rulesDirty) loadRulesForm()
        if(ruleModel.count>=128) return
        let row=s.rulePrefill||{}; if(!row.id) return
        ruleModel.append(ruleRow(row))
        rulesDirty=true; openRule(ruleModel.count-1); showRulesPage()
    }
    function openRuleCategory() { pickerIndex=-2; categorySearch.clear(); categoryPicker.open() }
    function inspectRecord(batch, ordinal) { guard(function(){workflow.command("inspect",{batch:batch,ordinal:ordinal})}) }
    function selectLedgerRow(row, modifiers) {
        guard(function(){workflow.command("select",{row:row,range:!!(modifiers & Qt.ShiftModifier),toggle:!!(modifiers & (Qt.ControlModifier|Qt.MetaModifier)),firstSearchSelection:!!searchReturn && !searchReturn.chosen})})
    }
    function copySource(field) {
        if (!editEnabled || workflow.busy) return
        if (field.split >= 0) {
            if (field.split >= splitModel.count) return
            splitModel.setProperty(field.split, field.field, field.value)
            if(field.field==="category") {splitModel.setProperty(field.split,"categoryId","");splitModel.setProperty(field.split,"bindingRevision","");splitModel.setProperty(field.split,"categoryDisplay",field.value?"Unmapped: "+field.value:"Uncategorized");splitModel.setProperty(field.split,"categoryTouched",true)}
            if(field.field==="memo")splitModel.setProperty(field.split,"memoTouched",true)
        } else {
            switch (field.field) {
            case "date": editDate.text=field.value; break
            case "payee": editPayee.text=field.value;payeeTouched=true; break
            case "memo": editMemo.text=field.value;memoTouched=true; break
            case "total": editTotal.text=field.value; break
            default: return
            }
        }
        dirty=true
    }
    function saveEditor() {
        let splits=[]
        for(let i=0;i<splitModel.count;i++) {let a=splitModel.get(i);splits.push({category:a.category,memo:a.memo,amount:a.amount,categoryId:a.categoryId||"",bindingRevision:a.bindingRevision||"",allocationId:a.allocationId||"",categoryTouched:!!a.categoryTouched,memoTouched:!!a.memoTouched})}
        workflow.command(committedEditing?"preview_change":"save", {kind:"correct",date:editDate.text,payee:editPayee.text,memo:editMemo.text,total:editTotal.text,splits:splits,payeeTouched:payeeTouched,memoTouched:memoTouched})
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
            if(action==="select")ledger.currentIndex=s.selectionIndex===undefined?-1:s.selectionIndex
            if (action==="inspect" || action==="import")
                Qt.callLater(function(){if(ledger.currentIndex>=0) ledger.positionViewAtIndex(ledger.currentIndex,ListView.Contain)})
            if(!success && (action==="save" || action==="preview_change" || action==="apply_change" || action==="preview_category" || action==="apply_category" || action==="rules_save" || action==="rules_apply" || action==="rules_discard" || action==="rules_preview")) {pendingAction=null;projected.checked=s.mode!=="Committed"}
            if (!s.active || (success && (action==="create" || action==="open" || action==="unlock"))) splitViews=({})
            if(!s.active) {addingAccount=false;bulkDialog.close();changePreview.close();categoryChangePreview.close();categoryPicker.close();rulesUnsaved.close();matchedRuleDialog.close();pendingAction=null;dirty=false;categoryDirty=false;committedEditing=false}
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
            }
            if(!success && action==="select_account"){pendingAccountNavigation=null;pendingCounterpart=null}
            if(success) {
                if(action==="select_account" || action==="create_account") {
                    addingAccount=false;splitViews={};searchReturn=null;pendingSearch=null;appliedFilter=s.filter||"";search.text=appliedFilter
                    if(pendingCounterpart){pendingCounterpart=null;Qt.callLater(function(){if(workflow.selectedRowIndex()>=0)ledger.positionViewAtIndex(workflow.selectedRowIndex(),ListView.Contain)})}
                    else if(accountViewports[s.accountId])Qt.callLater(function(){restoreViewport(accountViewports[s.accountId])})
                    if(pendingAccountNavigation) {navigationIndex=pendingAccountNavigation.index;currentPage=pendingAccountNavigation.page;pendingAccountNavigation=null}
                    else {let entries=navigation.slice(0,navigationIndex+1);entries.push("ledger");navigation=entries;let accounts=navigationAccounts.slice(0,navigationIndex+1);accounts.push(s.accountId);navigationAccounts=accounts;navigationIndex=entries.length-1;currentPage="ledger"}
                    loadEditor();loadRulesForm();loadCategoryForm()
                    if(currentPage==="history")Qt.callLater(function(){workflow.command("history")})
                }
                if(action.indexOf("transfer_")===0)loadEditor()
                if(action==="preview_bulk")bulkDialog.open()
                if(action==="apply_bulk"){bulkDialog.close();loadEditor()}
                projected.checked=s.mode!=="Committed"
                if(action==="preview_category") categoryChangePreview.open()
                if(action==="category_select" || action==="apply_category")loadCategoryForm()
                if(action==="apply_category") {let next=pendingAction;pendingAction=null;dismiss(categoryChangePreview);categoryDirty=false;if(!dirty)loadEditor();if(next)guard(next)}
                if(action==="preview_change") changePreview.open()
                if(action==="apply_change") dismiss(changePreview)
                if(["inspect","entry_history","import","resume","save","decide","opening","commit","discard","evidence","create","open","unlock","close","apply_change"].indexOf(action)>=0) loadEditor()
                if(action==="save" || action==="apply_change") {
                    dirty=false
                    if(pendingAction) {let next=pendingAction;pendingAction=null;guard(next)}
                }
                if(action==="rules_load" && !rulesDirty) loadRulesForm()
                if(action==="rules_save" || action==="rules_discard") {loadRulesForm();if(pendingAction){let next=pendingAction;pendingAction=null;guard(next)}}
                if(action==="rules_apply" && !dirty) loadEditor()
                if(action==="rules_prefill") acceptPrefill()
                if((action==="create" || action==="open" || action==="unlock") && !rulesDirty) loadRulesForm()
                if(action==="create" || action==="unlock") {password.text="";confirmation.text="";unlockPassword.text=""}
                if(action==="close") {if(closingRequested){allowClose=true;Qt.callLater(function(){window.close()})} search.clear();splitModel.clear();editDate.clear();editPayee.clear();editMemo.clear();editTotal.clear();dirty=false}
            }
        }
    }
    Component.onCompleted: {
        if(s.startup) workflow.command("open",{path:s.path})
    }
    ListModel {id:splitModel}
    ListModel {id:ruleModel}
    FileDialog { id:openDialog; title:"Choose an experimental dataset"; onAccepted:{location.text=workflow.localPath(selectedFile);workflow.command("open",{path:location.text})} }
    FileDialog { id:createDialog; fileMode:FileDialog.SaveFile; title:"Choose a NEW dataset file"; onAccepted:location.text=workflow.localPath(selectedFile) }
    FileDialog { id:importDialog; title:"Import synthetic QIF"; nameFilters:["QIF files (*.qif)","All files (*)"]; onAccepted:workflow.command("import",{path:workflow.localPath(selectedFile),profile:profile.currentText}) }
    FileDialog { id:backupDialog; fileMode:FileDialog.SaveFile; title:"Save backup / restored copy to a NEW file"; onAccepted:workflow.command("backup",{path:workflow.localPath(selectedFile)}) }
    Dialog {
        id:unsaved;objectName:"unsavedDialog"; title:committedEditing?"Unsaved committed correction":"Unsaved draft edit"; modal:true; anchors.centerIn:parent
        ColumnLayout {
            Label {textFormat:Text.PlainText;text:"Save the edit, discard its changes, or stay here?"}
            RowLayout {
                Button {objectName:"guardSave";text:committedEditing?"Review correction":"Save";onClicked:{let next=pendingAction;dismiss(unsaved);pendingAction=next;saveEditor()}}
                Button {objectName:"guardDiscard";text:"Discard";onClicked:{let next=pendingAction;pendingAction=null;dismiss(unsaved);loadEditor();if(next)guard(next)}}
                Button {objectName:"guardStay";text:"Stay";onClicked:{unsaved.close();pendingAction=null;projected.checked=s.mode!=="Committed"}}
            }
        }
        onRejected:if(!guardHandoff){pendingAction=null;projected.checked=s.mode!=="Committed"}
    }
    Dialog {
        id:changePreview;objectName:"committedChangePreview";modal:true;anchors.centerIn:parent
        width:Math.min(window.width-48,820);height:Math.min(window.height-80,650)
        title:(s.changePreview||{}).title||"Review committed change"
        onRejected:if(!guardHandoff){pendingAction=null;workflow.command("cancel_change")}
        ColumnLayout {
            anchors.fill:parent
            Label {textFormat:Text.PlainText;text:"This changes the current ledger interpretation. Original evidence and earlier history are retained; no new bank payment is created.";wrapMode:Text.Wrap;Layout.fillWidth:true}
            Label {objectName:"changeBalanceImpact";textFormat:Text.PlainText;text:(s.changePreview||{}).balanceImpact||"";font.bold:true;wrapMode:Text.Wrap;Layout.fillWidth:true}
            ScrollView {
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
                                ScrollBar.vertical:ScrollBar {}
                                delegate:Label {required property var modelData;width:previewAllocations.width;textFormat:Text.PlainText;text:modelData.category+" · "+modelData.memo+" · "+modelData.amount;wrapMode:Text.Wrap}
                            }
                        }
                    }
                }
            }
            Label {textFormat:Text.PlainText;text:workflow.error;visible:text.length>0;color:"#a02020";wrapMode:Text.Wrap;Layout.fillWidth:true}
            RowLayout {
                Button {objectName:"applyCommittedChange";text:(s.changePreview||{}).applyLabel||"Apply";enabled:!workflow.busy && !!(s.changePreview||{}).id;onClicked:workflow.command("apply_change",{preview:s.changePreview.id})}
                Button {objectName:"cancelCommittedChange";text:"Cancel";enabled:!workflow.busy;onClicked:{changePreview.close();pendingAction=null;workflow.command("cancel_change")}}
            }
        }
    }
    Dialog {
        id:bulkDialog;objectName:"bulkDialog";title:(s.bulkPreview||{}).title||"Bulk preview";modal:true;anchors.centerIn:parent;width:Math.min(850,window.width-80);height:Math.min(600,window.height-80)
        ColumnLayout {
            anchors.fill:parent
            Label {text:(s.bulkPreview||{}).impact||"";textFormat:Text.PlainText;wrapMode:Text.Wrap;Layout.fillWidth:true}
            Label {text:"All listed changes and their history are saved together. Original import evidence is retained.";wrapMode:Text.Wrap;Layout.fillWidth:true}
            ListView {
                id:bulkLines;objectName:"bulkPreviewLines";model:(s.bulkPreview||{}).lines||[];clip:true;reuseItems:true;Layout.fillWidth:true;Layout.fillHeight:true
                ScrollBar.vertical:ScrollBar {}
                delegate:Label {required property string modelData;width:bulkLines.width;text:modelData;textFormat:Text.PlainText;wrapMode:Text.Wrap}
            }
            Label {text:workflow.error;textFormat:Text.PlainText;visible:text.length>0;color:"#a02020";wrapMode:Text.Wrap;Layout.fillWidth:true}
            RowLayout {
                Button {objectName:"applyBulk";text:"Apply all";enabled:!workflow.busy && !!(s.bulkPreview||{}).id;onClicked:workflow.command("apply_bulk",{preview:s.bulkPreview.id})}
                Button {objectName:"cancelBulk";text:"Cancel";enabled:!workflow.busy;onClicked:{bulkDialog.close();workflow.command("cancel_bulk")}}
            }
        }
    }
    Dialog {
        id:categoryUnsaved;objectName:"categoryUnsavedDialog";title:"Unsaved category edit";modal:true;anchors.centerIn:parent
        standardButtons:Dialog.NoButton
        RowLayout {
            Button {objectName:"categoryGuardSave";text:"Review save";onClicked:{let next=pendingAction;dismiss(categoryUnsaved);pendingAction=next;saveCategoryForm()}}
            Button {objectName:"categoryGuardDiscard";text:"Discard";onClicked:{let next=pendingAction;pendingAction=null;dismiss(categoryUnsaved);loadCategoryForm();if(next)guard(next)}}
            Button {objectName:"categoryGuardStay";text:"Stay";onClicked:{categoryUnsaved.close();pendingAction=null}}
        }
        onRejected:if(!guardHandoff)pendingAction=null
    }
    Dialog {
        id:rulesUnsaved;objectName:"rulesUnsavedDialog";title:"Unsaved rules";modal:true;anchors.centerIn:parent;standardButtons:Dialog.NoButton
        RowLayout {
            Button {objectName:"rulesGuardSave";text:"Save";onClicked:{let next=pendingAction;dismiss(rulesUnsaved);pendingAction=next;saveRules()}}
            Button {objectName:"rulesGuardDiscard";text:"Discard";onClicked:{let next=pendingAction;pendingAction=null;dismiss(rulesUnsaved);workflow.command("rules_discard");pendingAction=next}}
            Button {objectName:"rulesGuardStay";text:"Stay";onClicked:{rulesUnsaved.close();pendingAction=null}}
        }
        onRejected:if(!guardHandoff)pendingAction=null
    }
    Dialog {
        id:matchedRuleDialog;objectName:"matchedRuleDialog";title:"Rules that match this record";modal:true;anchors.centerIn:parent;width:Math.min(window.width-48,640)
        ColumnLayout {
            width:matchedRuleDialog.availableWidth
            Label {text:"These are this record's own explanations.";wrapMode:Text.Wrap;Layout.fillWidth:true}
            Repeater {
                model:selected.ruleExplanations||[]
                Button {required property var modelData;objectName:"matchedRule_"+modelData.rule;text:modelData.label+" · "+modelData.field+" · "+modelData.effect;onClicked:editMatched(modelData.rule)}
            }
        }
    }
    Dialog {
        id:categoryChangePreview;objectName:"categoryChangePreview";title:"Review category change";modal:true;anchors.centerIn:parent;width:Math.min(window.width-48,680)
        onRejected:if(!guardHandoff){pendingAction=null;workflow.command("cancel_category_preview")}
        ColumnLayout {
            width:categoryChangePreview.availableWidth
            Label {textFormat:Text.PlainText;text:(s.categoryPreview||{}).summary||"";wrapMode:Text.Wrap;Layout.fillWidth:true}
            Label {text:"Renaming or moving changes the current category names and grouping used for past transactions. Original source and recorded history remain unchanged. Archiving hides choices for new assignments and retains existing assignments.";wrapMode:Text.Wrap;Layout.fillWidth:true}
            Label {textFormat:Text.PlainText;text:workflow.error;visible:text.length>0;wrapMode:Text.Wrap;Layout.fillWidth:true;color:"#a02020"}
            RowLayout {
                Button {objectName:"applyCategoryChange";text:"Apply category change";enabled:!workflow.busy && !!(s.categoryPreview||{}).id;onClicked:workflow.command("apply_category",{preview:s.categoryPreview.id})}
                Button {objectName:"cancelCategoryChange";text:"Cancel";enabled:!workflow.busy;onClicked:{categoryChangePreview.close();pendingAction=null;workflow.command("cancel_category_preview")}}
            }
        }
    }
    Dialog {
        id:categoryPicker;objectName:"categoryPicker";title:"Choose allocation category";modal:true;anchors.centerIn:parent;width:Math.min(window.width-48,700);height:Math.min(window.height-80,650)
        ColumnLayout {
            anchors.fill:parent
            Label {textFormat:Text.PlainText;text:pickerIndex>=0 && pickerIndex<splitModel.count?"Current: "+splitModel.get(pickerIndex).categoryDisplay:"";wrapMode:Text.Wrap;Layout.fillWidth:true}
            TextField {id:categorySearch;objectName:"categoryPickerSearch";placeholderText:"Search parent or leaf name";Layout.fillWidth:true;maximumLength:256;Accessible.name:"Category search"}
            ListView {
                id:pickerChoices;Layout.fillWidth:true;Layout.fillHeight:true;clip:true;reuseItems:true
                model:(s.categories||[]).filter(function(c){return !c.deleted && !c.archived && c.path.toLowerCase().indexOf(categorySearch.text.toLowerCase())>=0})
                ScrollBar.vertical:ScrollBar {}
                delegate:Button {required property var modelData;width:pickerChoices.width;text:modelData.path+(modelData.group?" (group)":"");enabled:modelData.assignable;objectName:"pickCategory_"+modelData.id;onClicked:chooseCategory(modelData.id,modelData.path)}
            }
            Button {objectName:"chooseUncategorized";text:"Uncategorized";onClicked:chooseCategory("","")}
            TextField {id:unmappedCategory;objectName:"unmappedCategoryText";placeholderText:"Explicit unmapped classification text";Layout.fillWidth:true;Accessible.name:"Unmapped category text"}
            Button {objectName:"chooseUnmapped";text:"Keep as unmapped text";enabled:unmappedCategory.text.length>0;onClicked:chooseCategory("",unmappedCategory.text)}
            Label {text:"Groups cannot receive allocations. Use Transfers to assign an allocation to another account.";wrapMode:Text.Wrap;Layout.fillWidth:true}
            Button {text:"Cancel";onClicked:categoryPicker.close()}
        }
    }
    Dialog {
        id:discardDialog;objectName:"discardImportDialog";width:Math.min(window.width-48,640);title:"Discard pending import?";modal:true;anchors.centerIn:parent
        standardButtons:Dialog.Ok|Dialog.Cancel
        Label {width:discardDialog.availableWidth;wrapMode:Text.Wrap;textFormat:Text.PlainText;text:processingIncomplete?"Discard this incomplete import. Encrypted source evidence and history are retained.":"Remove the draft projection. Encrypted source evidence and history are retained."}
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
            Button {objectName:"sidebarToggle";text:sidebarVisible?"Hide sidebar":"Show sidebar";visible:!!s.active;onClicked:sidebarVisible=!sidebarVisible;Accessible.name:text+" (Ctrl+Shift+B)"}
            Button {objectName:"navigationBack";text:"Back";visible:!!s.active;enabled:!workflow.busy && navigationIndex>0;onClicked:traverseNavigation(-1)}
            Button {objectName:"navigationForward";text:"Forward";visible:!!s.active;enabled:!workflow.busy && navigationIndex+1<navigation.length;onClicked:traverseNavigation(1)}
            Label {textFormat:Text.PlainText;text:s.active ? s.name+" · "+s.currency : "Dataset setup and unlocking";font.pixelSize:20;elide:Text.ElideRight;Layout.fillWidth:true}
            BusyIndicator {running:workflow.busy;visible:running;implicitHeight:30}
            Button {objectName:"settingsNavigation";text:"Settings";visible:!!s.active;enabled:!workflow.busy;onClicked:navigate("settings")}
        }
        RowLayout {
            visible:workflow.busy;Layout.fillWidth:true
            Label {objectName:"operationPhase";textFormat:Text.PlainText;text:workflow.phase;Layout.fillWidth:true;wrapMode:Text.Wrap;Accessible.name:text}
            Label {text:"Cancellation requested…";visible:workflow.cancellationRequested}
            Button {objectName:"cancelOperation";text:"Cancel";visible:workflow.canCancel;enabled:!workflow.cancellationRequested;onClicked:workflow.requestCancel()}
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
        SplitView {
            id:shell;objectName:"durableShell"
            visible:!!s.active;Layout.fillWidth:true;Layout.fillHeight:true
            orientation:Qt.Horizontal
            Pane {
                id:sidebar;objectName:"accountSidebar";visible:sidebarVisible
                SplitView.preferredWidth:200;SplitView.minimumWidth:140;SplitView.maximumWidth:Math.max(140,Math.min(360,shell.width*0.35))
                ColumnLayout {
                    anchors.fill:parent
                    Label {text:"Accounts";font.bold:true}
                    ScrollView {
                        id:accountScroll;Layout.fillWidth:true;Layout.fillHeight:true;contentWidth:availableWidth;clip:true
                        ColumnLayout {
                            width:accountScroll.availableWidth
                            Repeater {
                                model:s.accounts||[]
                                Button {
                                    required property var modelData
                                    objectName:modelData.id===s.accountId?"accountNavigation":"accountNavigation_"+modelData.id
                                    Layout.fillWidth:true;text:modelData.name;enabled:!workflow.busy
                                    highlighted:modelData.id===s.accountId && currentPage==="ledger"
                                    contentItem:Label {text:parent.text;color:parent.highlighted?"#ffffff":"#18232d";textFormat:Text.PlainText;elide:Text.ElideRight;verticalAlignment:Text.AlignVCenter}
                                    Accessible.name:"Open account "+modelData.name
                                    onClicked:chooseAccount(modelData.id)
                                }
                            }
                            TextField {
                                id:newAccountName;objectName:"newAccountName";visible:addingAccount;Layout.fillWidth:true;enabled:!workflow.busy
                                placeholderText:"Account name · Enter";Accessible.name:"New account name"
                                onAccepted:if(text.length>0)workflow.command("create_account",{id:accountCreationId,name:text,currency:s.homeCurrency,date:accountCreationDate})
                                Keys.onEscapePressed:addingAccount=false
                            }
                            Button {text:"Cancel";visible:addingAccount;enabled:!workflow.busy;onClicked:addingAccount=false}
                            Button {objectName:"newAccount";text:"+ New account";Layout.fillWidth:true;visible:!addingAccount;enabled:!workflow.busy && !!s.nextAccountId;onClicked:guard(function(){accountCreationId=s.nextAccountId;accountCreationDate=Qt.formatDate(new Date(),"yyyy-MM-dd");newAccountName.clear();addingAccount=true;Qt.callLater(function(){newAccountName.forceActiveFocus();accountScroll.ScrollBar.vertical.position=1})})}
                        }
                    }
                    Label {textFormat:Text.PlainText;text:(s.mode==="Projected"?"Projected":"Committed")+" balance";Layout.fillWidth:true;wrapMode:Text.Wrap}
                    Label {textFormat:Text.PlainText;text:(s.currency||"")+" "+(s.mode==="Projected"?(s.projected||"Unavailable"):(s.committed||""));Layout.fillWidth:true;wrapMode:Text.Wrap;font.bold:true}
                    Button {objectName:"categoryNavigation";text:"Categories";Layout.fillWidth:true;visible:!!s.categoriesSupported;enabled:!workflow.busy;onClicked:guard(function(){navigate("categories");workflow.command("category_list")})}
                    Button {objectName:"rulesNavigation";text:"Rules";Layout.fillWidth:true;visible:!!s.rulesSupported;enabled:!workflow.busy;highlighted:currentPage==="rules";onClicked:guard(function(){navigate("rules");if(!rulesDirty)workflow.command("rules_load")})}
                    Button {objectName:"historyNavigation";text:"Committed changes";Layout.fillWidth:true;visible:!!s.committedChangesSupported;enabled:!workflow.busy;onClicked:guard(function(){navigate("history");workflow.command("history")})}
                    Button {objectName:"sidebarSettings";text:"Settings";Layout.fillWidth:true;enabled:!workflow.busy;highlighted:currentPage==="settings";onClicked:navigate("settings")}
                }
            }
            Item {
                SplitView.fillWidth:true
                ScrollView {
                    id:categoryPage;objectName:"categoryPage";anchors.fill:parent;visible:currentPage==="categories";contentWidth:availableWidth;enabled:!workflow.busy
                    ColumnLayout {
                        width:categoryPage.availableWidth;spacing:10
                        Label {text:"Categories";font.pixelSize:20}
                        Label {text:"Create your own categories. Groups contain leaves; standalone leaves are also assignable. No starter names are added automatically.";wrapMode:Text.Wrap;Layout.fillWidth:true}
                        RowLayout {
                            TextField {id:catalogSearch;placeholderText:"Search categories";Layout.fillWidth:true;maximumLength:256}
                            CheckBox {id:showArchivedCategories;text:"Show archived / deleted"}
                            Button {objectName:"newCategory";text:"New category";onClicked:guard(newCategory)}
                        }
                        ListView {
                            id:catalogList;objectName:"catalogList";Layout.fillWidth:true;Layout.preferredHeight:220;clip:true;reuseItems:true
                            model:(s.categories||[]).filter(function(c){return (showArchivedCategories.checked || (!c.archived && !c.deleted)) && c.path.toLowerCase().indexOf(catalogSearch.text.toLowerCase())>=0})
                            ScrollBar.vertical:ScrollBar {}
                            delegate:Button {required property var modelData;width:catalogList.width;text:modelData.path+(modelData.group?" (group)":"")+(modelData.deleted?" [Deleted]":modelData.archived?" [Archived]":"");objectName:"editCategory_"+modelData.id;onClicked:guard(function(){workflow.command("category_select",{id:modelData.id})})}
                        }
                        Label {textFormat:Text.PlainText;text:(s.categorySelected||{}).path||"New category";font.bold:true;Layout.fillWidth:true;wrapMode:Text.Wrap}
                        RowLayout {
                            visible:categoryFormId.length>0 && !(s.categorySelected||{}).deleted
                            Button {objectName:"renameCategory";text:"Rename";enabled:!categoryDirty;onClicked:categoryAction="rename"}
                            Button {objectName:"moveCategory";text:"Move";visible:!(s.categorySelected||{}).group;enabled:!categoryDirty;onClicked:categoryAction="move"}
                            Button {objectName:"archiveCategory";text:(s.categorySelected||{}).ownArchived?"Unarchive":"Archive";enabled:!categoryDirty;onClicked:saveCategoryForm((s.categorySelected||{}).ownArchived?"unarchive":"archive")}
                            Button {objectName:"deleteCategory";text:"Delete unused category";enabled:!categoryDirty;onClicked:saveCategoryForm("delete")}
                        }
                        TextField {id:categoryName;objectName:"categoryName";visible:categoryAction==="create" || categoryAction==="rename";placeholderText:"Category name";Layout.fillWidth:true;onTextEdited:categoryDirty=true;Accessible.name:"Category name"}
                        ComboBox {id:categoryKind;objectName:"categoryKind";visible:categoryAction==="create";model:["Assignable leaf","Non-assignable group"];onActivated:categoryDirty=true}
                        ComboBox {id:categoryParent;objectName:"categoryParent";visible:categoryAction==="move" || (categoryAction==="create" && categoryKind.currentIndex===0);model:categoryParents();textRole:"path";Layout.fillWidth:true;onActivated:categoryDirty=true}
                        RowLayout {
                            visible:categoryAction.length>0
                            Button {objectName:"saveCategory";text:"Review category save";enabled:categoryDirty;onClicked:saveCategoryForm()}
                            Button {objectName:"cancelCategoryEdit";text:"Cancel edit";onClicked:loadCategoryForm()}
                        }
                        Label {textFormat:Text.PlainText;text:(s.categoryReceipt||{}).summary||"";wrapMode:Text.Wrap;Layout.fillWidth:true}
                        Label {text:"Definition history";visible:(s.categoryHistory||[]).length>0;font.bold:true}
                        ListView {
                            id:categoryAudit;model:s.categoryHistory||[];Layout.fillWidth:true;Layout.preferredHeight:Math.min(180,contentHeight);clip:true;reuseItems:true;ScrollBar.vertical:ScrollBar {}
                            delegate:Label {required property string modelData;width:categoryAudit.width;text:modelData;textFormat:Text.PlainText;wrapMode:Text.Wrap}
                        }
                    }
                }
                ScrollView {
                    id:rulesPage;objectName:"rulesPage";anchors.fill:parent;visible:currentPage==="rules";contentWidth:availableWidth;enabled:!workflow.busy
                    ColumnLayout {
                        width:rulesPage.availableWidth;spacing:8
                        Label {text:"Rules";font.pixelSize:20}
                        Label {text:"Edit the whole list, then Save or Discard. Preview can use unsaved rules. Apply uses only the saved ruleset.";wrapMode:Text.Wrap;Layout.fillWidth:true}
                        RowLayout {
                            Button {objectName:"newRule";text:"New rule";onClicked:newRule()}
                            Button {objectName:"saveRules";text:"Save";enabled:rulesDirty;onClicked:saveRules()}
                            Button {objectName:"discardRules";text:"Discard";enabled:rulesDirty;onClicked:workflow.command("rules_discard")}
                            CheckBox {id:showRemovedRules;objectName:"showRemovedRules";text:"Show removed"}
                        }
                        Column {
                            Layout.fillWidth:true
                            Repeater {
                                model:ruleModel
                                RowLayout {
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
                                    width:rulesPage.availableWidth
                                    // The rule's own flag must not disable the row. Re-enable after the role is applied.
                                    enabled:true
                                    Label {text:String(index+1);Layout.preferredWidth:28}
                                    CheckBox {objectName:"ruleEnabled_"+index;checked:ruleEntry.ruleOn;onClicked:toggleRule(index)}
                                    Button {objectName:"openRule_"+index;text:label||("Rule "+(index+1));onClicked:openRule(index);Layout.fillWidth:true}
                                    Label {text:(scope==="split"?"Split":"Transaction")+" · "+(scope==="split" && input==="payee"?"Parent payee":input);Layout.preferredWidth:170}
                                    Label {text:expression;elide:Text.ElideRight;Layout.preferredWidth:180;textFormat:Text.PlainText}
                                    Label {text:actionSummary(ruleEntry);elide:Text.ElideRight;Layout.preferredWidth:180;textFormat:Text.PlainText}
                                    Button {objectName:"moveRuleUp_"+index;text:"Up";enabled:index>0;onClicked:moveRule(index,-1)}
                                    Button {objectName:"moveRuleDown_"+index;text:"Down";enabled:index<ruleModel.count-1;onClicked:moveRule(index,1)}
                                    Button {objectName:"deleteRule_"+index;text:"Delete";onClicked:deleteRule(index)}
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
                                    Button {objectName:"copyRemoved_"+modelData.id;text:"Copy into new rule";onClicked:copyRemoved(modelData.id)}
                                }
                            }
                        }
                        ColumnLayout {
                            visible:ruleIndex>=0
                            Layout.fillWidth:true
                            Label {text:"Expression text is what Save compiles. Checkboxes only recognize the forms they write.";wrapMode:Text.Wrap;Layout.fillWidth:true}
                            TextField {id:ruleLabel;objectName:"ruleLabel";placeholderText:"Label";Layout.fillWidth:true;onTextEdited:writeRule();Accessible.name:"Rule label"}
                            RowLayout {
                                ComboBox {id:ruleScope;objectName:"ruleScope";model:["Transaction","Split"];onActivated:if(!ruleSync){if(currentIndex===0){ruleSync=true;ruleInherit.checked=false;ruleSync=false} writeRule()}}
                                ComboBox {id:ruleInput;objectName:"ruleInput";model:ruleScope.currentIndex===1?["Parent payee","Memo","Category"]:["Payee","Memo","Category"];onActivated:if(!ruleSync)writeRule()}
                            }
                            TextField {id:ruleExpression;objectName:"ruleExpression";placeholderText:"Expression";Layout.fillWidth:true;onTextEdited:{syncAssist();writeRule()} Accessible.name:"Rule expression"}
                            RowLayout {
                                CheckBox {id:ruleCaseSensitive;objectName:"ruleCaseSensitive";text:"Case sensitive";onClicked:if(!ruleSync)setCase(checked)}
                                CheckBox {id:ruleWholeField;objectName:"ruleWholeField";text:"Whole field";onClicked:if(!ruleSync)setWhole(checked)}
                            }
                            Label {id:ruleAssistNote;objectName:"ruleAssistNote";text:"";visible:text.length>0;wrapMode:Text.Wrap;Layout.fillWidth:true;textFormat:Text.PlainText}
                            TextField {id:rulePayeeTemplate;objectName:"rulePayeeTemplate";visible:ruleScope.currentIndex===0;placeholderText:"Payee template. ${0} through ${n}, and $$";Layout.fillWidth:true;onTextEdited:writeRule();Accessible.name:"Payee template"}
                            TextField {id:ruleMemoTemplate;objectName:"ruleMemoTemplate";placeholderText:"Memo template. ${0} through ${n}, and $$";Layout.fillWidth:true;onTextEdited:writeRule();Accessible.name:"Memo template"}
                            RowLayout {
                                Button {objectName:"ruleCategory";text:ruleCategoryId?(((s.categoryById||{})[ruleCategoryId]||{}).path||ruleCategoryId):"No category";enabled:!ruleInherit.checked;onClicked:openRuleCategory()}
                                Button {objectName:"clearRuleCategory";text:"Clear category";onClicked:{ruleCategoryId="";writeRule()}}
                                CheckBox {id:ruleInherit;objectName:"ruleInherit";visible:ruleScope.currentIndex===1;text:"Inherit parent category";onClicked:if(!ruleSync){if(checked)ruleCategoryId="";writeRule()}}
                            }
                            Label {text:"A split rule cannot set the transaction payee. Inherit parent clears the category action. Save is refused until the rule has an action.";wrapMode:Text.Wrap;Layout.fillWidth:true}
                        }
                        RowLayout {
                            Button {objectName:"previewRules";text:"Preview";onClicked:previewRules()}
                            CheckBox {id:replaceManualFields;objectName:"replaceManualFields";text:"Replace manual fields, including blanks"}
                            Button {objectName:"applyRules";text:"Apply";visible:currentPage==="rules" && !!s.pending && !s.pending.processingIncomplete && !selected.committed;enabled:!rulesDirty && replaceManualFields.checked===!!((s.rulePreview||{}).replaceManual) && applyReady();onClicked:applyRules()}
                        }
                        Label {objectName:"rulesCommittedNotice";visible:currentPage==="rules" && (!!selected.committed || !s.pending);text:"Committed records are not changed by Apply.";wrapMode:Text.Wrap;Layout.fillWidth:true;textFormat:Text.PlainText}
                        Label {objectName:"rulePreviewStatus";text:(s.rulePreview||{}).label||"";font.bold:true;textFormat:Text.PlainText}
                        Label {objectName:"rulePreviewBody";text:(s.rulePreview||{}).summaryText||"";wrapMode:Text.Wrap;Layout.fillWidth:true;textFormat:Text.PlainText}
                        Label {text:(s.rulePreview||{}).overlap||"";wrapMode:Text.Wrap;Layout.fillWidth:true;textFormat:Text.PlainText}
                        Label {textFormat:Text.PlainText;text:(s.ruleReceipt||{}).summary||"";wrapMode:Text.Wrap;Layout.fillWidth:true}
                        Label {objectName:"ruleLimits";text:s.ruleLimits||"";wrapMode:Text.Wrap;Layout.fillWidth:true;textFormat:Text.PlainText}
                    }
                }
                ScrollView {
                    id:historyPage;objectName:"committedHistoryPage";anchors.fill:parent;visible:currentPage==="history";contentWidth:availableWidth;enabled:!workflow.busy
                    ColumnLayout {
                        width:historyPage.availableWidth;spacing:10
                        Label {text:"Committed changes";font.pixelSize:20}
                        Label {text:"Corrections, removals and Undo operations. Balances below describe each operation when it was applied.";wrapMode:Text.Wrap;Layout.fillWidth:true}
                        Button {text:"Refresh history";onClicked:workflow.command("history")}
                        Label {text:"No committed corrections or removals yet.";visible:(s.history||[]).length===0}
                        ListView {
                            id:historyIndex;objectName:"committedHistoryIndex";model:s.history||[];clip:true;reuseItems:true
                            Layout.fillWidth:true;Layout.preferredHeight:Math.min(200,contentHeight)
                            ScrollBar.vertical:ScrollBar {}
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
                            ScrollBar.vertical:ScrollBar {}
                            delegate:Frame {
                                required property var modelData
                                width:entryHistoryList.width
                                contentItem:ColumnLayout {
                                    Label {textFormat:Text.PlainText;text:modelData.summary;wrapMode:Text.Wrap;Layout.fillWidth:true}
                                    ScrollView {
                                        Layout.fillWidth:true;Layout.preferredHeight:150;contentWidth:availableWidth
                                        TextArea {text:modelData.details;readOnly:true;selectByMouse:true;textFormat:TextEdit.PlainText;wrapMode:TextEdit.Wrap}
                                    }
                                    Label {textFormat:Text.PlainText;text:modelData.undoReason;visible:!modelData.undoEligible;wrapMode:Text.Wrap;Layout.fillWidth:true}
                                    Button {objectName:"undo_"+modelData.operation;text:"Review Undo…";enabled:modelData.undoEligible;onClicked:workflow.command("preview_change",{kind:"undo",operation:modelData.operation,batch:modelData.batch,ordinal:modelData.ordinal})}
                                    Button {text:"Review group Undo…";visible:!!modelData.bulkGroup;onClicked:workflow.command("preview_bulk",{kind:"undo",operation:modelData.bulkGroup})}
                                }
                            }
                        }
                    }
                }
                ScrollView {
                    id:activeSettings;objectName:"activeSettings";anchors.fill:parent;visible:currentPage==="settings";enabled:!workflow.busy;contentWidth:availableWidth
                    ColumnLayout {
                        width:activeSettings.availableWidth;spacing:12
                        Label {text:"Dataset settings";font.pixelSize:20}
                        Label {text:"Active dataset";font.bold:true}
                        Label {textFormat:Text.PlainText;text:s.path||"";Layout.fillWidth:true;wrapMode:Text.WrapAnywhere}
                        Label {text:"Close this dataset before opening another. Saved drafts and committed records remain in the dataset.";Layout.fillWidth:true;wrapMode:Text.Wrap}
                        Button {objectName:"closeDataset";text:"Close dataset / choose another…";onClicked:guard(function(){workflow.command("close")})}
                        Label {text:"Backup and recovery";font.bold:true}
                        Label {text:"Save a validated backup or working copy to a new file. To restore, close this dataset, open the backup, and save a working copy to a new location.";Layout.fillWidth:true;wrapMode:Text.Wrap}
                        Button {objectName:"settingsBackup";text:"Backup / restore copy…";onClicked:guard(function(){backupDialog.open()})}
                        Button {objectName:"returnToLedger";text:"Return to account";onClicked:navigate("ledger")}
                    }
                }
                ScrollView {
                    id:ledgerScroll;anchors.fill:parent;visible:currentPage==="ledger";clip:true
                    // Keep controls reachable in a narrow window or with a wide sidebar.
                    contentWidth:Math.max(1080,availableWidth);contentHeight:availableHeight
                    ColumnLayout {
            id:ledgerPage;objectName:"ledgerPage"
            width:ledgerScroll.contentWidth;height:ledgerScroll.availableHeight;enabled:!workflow.busy || workflow.selecting
            Label {textFormat:Text.PlainText;text:s.path || "";elide:Text.ElideMiddle;Layout.fillWidth:true}
            RowLayout {
                Label {textFormat:Text.PlainText;text:"Committed: "+(s.committed||"")+"   Projected: "+(s.projected||"");Layout.fillWidth:true;Layout.minimumWidth:0;wrapMode:Text.Wrap}
                ComboBox {id:profile;Layout.preferredWidth:200;model:["Choose date order","DMY","MDY"];Accessible.name:"QIF date order"}
                Button {text:"Import QIF…";enabled:profile.currentIndex>0 && !s.pending;onClicked:guard(function(){importDialog.open()})}
                Button {objectName:"openTransfers";text:"Transfers…";visible:!!s.transfersSupported;onClicked:guard(function(){transferDialog.open()})}
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
                Button {objectName:"reviewRecord";text:"Review record";enabled:batchPicker.currentIndex>=0 && batchPicker.model[batchPicker.currentIndex].count>0 && !batchPicker.model[batchPicker.currentIndex].processingIncomplete;onClicked:inspectRecord(batchPicker.model[batchPicker.currentIndex].id,recordNumber.value-1)}
                Button {objectName:"viewSourceFile";text:"Source file";enabled:batchPicker.currentIndex>=0;onClicked:guard(function(){workflow.command("evidence",{batch:batchPicker.model[batchPicker.currentIndex].id});sourceToggle.checked=true})}
                Label {visible:!processingIncomplete;textFormat:Text.PlainText;text:batchPicker.currentIndex>=0?batchPicker.model[batchPicker.currentIndex].status:"No imports";Layout.minimumWidth:0;elide:Text.ElideRight}
                Item {Layout.fillWidth:true}
                Button {objectName:"resumeImport";text:"Resume import";visible:processingIncomplete;onClicked:guard(function(){workflow.command("resume",{batch:s.pending.id,revision:s.pending.revision})})}
                Button {objectName:"discardImport";text:"Discard batch…";enabled:!!s.pending;onClicked:guard(function(){discardDialog.open()})}
                Button {objectName:"commitImport";text:"Commit batch…";enabled:!!s.pending && !processingIncomplete && !!s.projectionAvailable && !!s.confirmed;onClicked:guard(function(){commitDialog.open()})}
            }
            Repeater {model:s.protectedImports||[];delegate:RowLayout {required property var modelData;Layout.fillWidth:true;Label {text:"Protected drafts · "+modelData.id+" · "+modelData.change;Layout.fillWidth:true;textFormat:Text.PlainText} Button {text:"Commit protected drafts…";enabled:!!s.confirmed;onClicked:{protectedCommit.item=modelData;protectedCommit.open()}}}}
            Label {
                objectName:"incompleteImportNotice";visible:processingIncomplete;Layout.fillWidth:true;wrapMode:Text.Wrap
                textFormat:Text.PlainText;color:"#855100"
                text:processingIncomplete?"Import processing is incomplete. Original source evidence is saved ("+s.pending.count+" parsed records). No draft amounts have been added and the projected balance is unavailable. Resume to prepare the draft for review, inspect the source file, or discard this import while retaining its evidence.":""
            }
            Label {textFormat:Text.PlainText;visible:!!s.pending && !!s.pending.blocked && !processingIncomplete;text:s.pending && s.pending.blocked ? "BLOCKED — no partial commit. Inspect each record. Date and split-total repairs are available in new datasets; other errors require discard/reimport.\n"+(s.pending.diagnostics||""):"";color:"#a02020";wrapMode:Text.Wrap;Layout.fillWidth:true;maximumLineCount:5;elide:Text.ElideRight}
            Label {objectName:"committedChangeReceipt";textFormat:Text.PlainText;text:(s.changeReceipt||{}).summary||"";visible:text.length>0;wrapMode:Text.Wrap;Layout.fillWidth:true}
            Label {textFormat:Text.PlainText;visible:!!s.receipt && !!s.receipt.operation;text:s.receipt && s.receipt.operation ? "Committed "+s.receipt.operation+" · included "+s.receipt.included+", excluded "+s.receipt.excluded+" · "+s.receipt.before+" + ("+s.receipt.change+") = "+s.receipt.after:"";wrapMode:Text.Wrap;Layout.fillWidth:true}
            RowLayout {
                CheckBox {id:projected;checked:true;text:"Projected ledger";onClicked:guard(query)}
                TextField {id:search;objectName:"ledgerSearch";placeholderText:'Search · period:may-june · QIF: · regex:"pattern"';maximumLength:4096;Layout.fillWidth:true;onAccepted:guard(query);Keys.onEscapePressed:guard(function(){search.clear();query()});Accessible.name:"Ledger search"}
                Button {objectName:"applyFilter";text:"Apply filter";onClicked:guard(query)}
                Button {objectName:"clearFilter";text:"Clear";enabled:search.text.length>0 || appliedFilter.length>0;onClicked:guard(function(){search.clear();query()})}
                Label {text:(s.selectionCount||0)+" selected";textFormat:Text.PlainText}
                Label {textFormat:Text.PlainText;text:"Mode: "+(s.mode||"");Layout.minimumWidth:0;elide:Text.ElideRight}
            }
            RowLayout {
                Layout.fillWidth:true;visible:!!s.bulkSupported
                Button {objectName:"bulkRemove";text:"Delete selected…";enabled:(s.selectionCount||0)>0;onClicked:guard(function(){workflow.command("preview_bulk",{kind:"remove"})})}
                ComboBox {id:bulkCategory;objectName:"bulkCategory";model:[{id:"",path:"Uncategorized"}].concat((s.categories||[]).filter(function(c){return c.assignable}));textRole:"path";Layout.preferredWidth:220;Accessible.name:"Category for selected transactions"}
                Button {objectName:"bulkCategorize";text:"Change category…";enabled:(s.selectionCount||0)>0 && bulkCategory.currentIndex>=0;onClicked:guard(function(){workflow.command("preview_bulk",{kind:"category",category:bulkCategory.model[bulkCategory.currentIndex].id})})}
                Label {textFormat:Text.PlainText;text:(s.bulkReceipt||{}).summary||"Committed rows only · every allocation";wrapMode:Text.Wrap;Layout.fillWidth:true}
                Button {objectName:"undoBulk";text:"Undo group…";visible:!!(s.bulkReceipt||{}).operation;onClicked:guard(function(){workflow.command("preview_bulk",{kind:"undo",operation:s.bulkReceipt.operation})})}
            }
            RowLayout {
                Repeater {model:["Date","Payee","Category","Withdrawal","Deposit","Balance"]
                    Button {required property string modelData;required property int index;text:modelData+(index===(s.sort||0)?(s.descending?" ▼":" ▲"):"");Layout.fillWidth:true;onClicked:guard(function(){query(index,s.sort===index?!s.descending:false)})}
                }
            }
            ListView {
                id:ledger;objectName:"durableLedger";Layout.fillWidth:true;Layout.fillHeight:true;clip:true;model:workflow;reuseItems:true;highlightFollowsCurrentItem:false
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
                        guard(function(){workflow.command("preview_bulk",{kind:"remove"})});event.accepted=true
                    }
                }
                ScrollBar.vertical:ScrollBar {}
                delegate:Rectangle {
                    required property var record
                    required property int index
                    id: ledgerRow
                    property bool expanded: record.batch===selected.batch && record.ordinal===selected.ordinal
                    property bool viewingSplits: !!window.splitViews[window.splitKey(record)] && record.splits.length>1
                    property alias editorSlot: rowEditorSlot
                    width:ledger.width;height:34+(expanded?editor.height:splitPreview.height);color:(s.selectionKeys||{})[record.batch+":"+record.ordinal]?"#c7e3fb":record.status==="Draft"?"#fff6de":"#f3f5f7"
                    border.width:(s.selectionKeys||{})[record.batch+":"+record.ordinal]?1:0;border.color:"#2870a8"
                    MouseArea {width:parent.width;height:34;acceptedButtons:Qt.LeftButton|Qt.RightButton
                        onClicked:function(mouse){ledger.forceActiveFocus();if(mouse.button===Qt.RightButton){if(!(s.selectionKeys||{})[record.batch+":"+record.ordinal])selectLedgerRow(ledgerRow.index,0);ledgerContext.popup()}else selectLedgerRow(ledgerRow.index,mouse.modifiers)}
                        onDoubleClicked:function(mouse){if(mouse.button===Qt.LeftButton && record.batch.length)inspectRecord(record.batch,record.ordinal)}
                    }
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
                            Label {textFormat:Text.PlainText;text:s.categoriesSupported?modelData.categoryDisplay:(modelData.category || "Uncategorized");width:parent.width*0.25;elide:Text.ElideRight}
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
                Label {textFormat:Text.PlainText;text:(selected.ordinal===undefined?"Source file":"Record "+(selected.ordinal+1))+" · "+(selected.batch||"")+" · "+(committedEditing?"Committed correction":selected.removed?"Removed record":selected.editable?"Durable draft editor":"Read only")+" · original "+(selected.original||"");wrapMode:Text.Wrap;Layout.fillWidth:true}
                Label {textFormat:Text.PlainText;text:selected.diagnostic||"";color:"#a02020";visible:text.length>0;wrapMode:Text.Wrap;Layout.fillWidth:true}
                Label {textFormat:Text.PlainText;text:selected.committed && !s.committedChangesSupported?"Committed changes require a new schema-4 experimental dataset. This dataset has not been migrated.":"";visible:text.length>0;wrapMode:Text.Wrap;Layout.fillWidth:true}
                RowLayout {
                    visible:!!selected.committed && !committedEditing
                    Button {objectName:"editCommitted";text:"Edit committed record";visible:!!selected.canCorrect;onClicked:committedEditing=true}
                    Button {objectName:"removeCommitted";text:"Remove transaction…";visible:!!selected.canCorrect;onClicked:workflow.command("preview_change",{kind:"remove"})}
                    Button {objectName:"showEntryHistory";text:"History";visible:!!s.committedChangesSupported;onClicked:guard(function(){navigate("history");workflow.command("entry_history",{batch:selected.batch,ordinal:selected.ordinal})})}
                }
                RowLayout {
                    visible:!!selected.draft
                    TextField {id:editDate;objectName:"durableDate";placeholderText:"YYYY-MM-DD";readOnly:!editEnabled;onAccepted:if(editEnabled)saveEditor();onTextEdited:dirty=true;Accessible.name:"Transaction date"}
                    TextField {id:editPayee;objectName:"durablePayee";placeholderText:"Payee";readOnly:!editEnabled;onAccepted:if(editEnabled)saveEditor();onTextEdited:{dirty=true;payeeTouched=true} Layout.fillWidth:true;Accessible.name:"Payee"}
                    TextField {id:editTotal;objectName:"durableTotal";placeholderText:"Exact total";readOnly:!editEnabled;onAccepted:if(editEnabled)saveEditor();onTextEdited:dirty=true;Accessible.name:"Total"}
                }
                TextField {id:editMemo;visible:!!selected.draft;placeholderText:"Memo";readOnly:!editEnabled;onAccepted:if(editEnabled)saveEditor();onTextEdited:{dirty=true;memoTouched=true} Layout.fillWidth:true;Accessible.name:"Memo"}
                Repeater {
                    model:splitModel
                    RowLayout {
                        required property int index
                        required property string category
                        required property string memo
                        required property string amount
                        Button {objectName:"splitCategory_"+index;visible:!!s.categoriesSupported;text:(index>=0 && index<splitModel.count ? splitModel.get(index).categoryDisplay : "")||"Uncategorized";enabled:editEnabled;onClicked:{if(category.startsWith("[")){let id=splitModel.get(index).allocationId;guard(function(){let choices=(selected.draft||{}).splits||[];transferAllocation.currentIndex=choices.findIndex(function(a){return a.allocationId===id});transferDialog.open()})}else openCategoryPicker(index)}}
                        TextField {visible:!s.categoriesSupported;text:category;placeholderText:"Category";readOnly:!editEnabled;onAccepted:if(editEnabled)saveEditor();onTextEdited:{splitModel.setProperty(index,"category",text);splitModel.setProperty(index,"categoryTouched",true);dirty=true} Accessible.name:"Split category"}
                        TextField {text:memo;placeholderText:"Split memo";Layout.fillWidth:true;readOnly:!editEnabled;onAccepted:if(editEnabled)saveEditor();onTextEdited:{splitModel.setProperty(index,"memo",text);splitModel.setProperty(index,"memoTouched",true);dirty=true} Accessible.name:"Split memo"}
                        Button {objectName:"createRuleFromMemo_"+index;visible:!!s.rulesSupported;enabled:index>=0 && index<splitModel.count && !!splitModel.get(index) && splitModel.get(index).sourceOrdinal>=0;text:enabled?"Create rule from this split memo":"Manual splits are not rule sources";onClicked:createFromSource("memo",index)}
                        TextField {objectName:"editSplitAmount_"+index;text:amount;placeholderText:"Exact split amount";readOnly:!editEnabled;onAccepted:if(editEnabled)saveEditor();onTextEdited:{splitModel.setProperty(index,"amount",text);dirty=true} Accessible.name:"Split amount"}
                        Button {text:"Remove";visible:editEnabled;enabled:!category.startsWith("[");onClicked:{splitModel.remove(index);dirty=true}}
                    }
                }
                RowLayout {
                    visible:editEnabled
                    Button {text:"Add split";enabled:splitModel.count<1000;onClicked:{splitModel.append({category:"",memo:"",amount:"0",categoryId:"",bindingRevision:"",allocationId:"",categoryDisplay:"Uncategorized",categoryTouched:true,memoTouched:true});dirty=true}}
                    Button {objectName:"saveDraft";text:committedEditing?"Review correction…":selected.repair?"Save repair":"Save draft";onClicked:saveEditor()}
                    Button {text:"Cancel edit";onClicked:loadEditor()}
                    Label {textFormat:Text.PlainText;text:dirty?"Unsaved changes":committedEditing?"No correction applied":selected.repair?"Repair not yet saved":"Last saved revision"}
                }
                RowLayout {
                    visible:!!selected.editable && !selected.repair
                    Label {textFormat:Text.PlainText;text:selected.decision===0?"Unresolved match":selected.decision===2?(selected.removedDuplicate?"Keep removed: ":"Excluded duplicate: ")+(selected.duplicate||""):"Include as new"}
                    Button {objectName:"includeSeparate";text:"Include as separate transaction";onClicked:guard(function(){workflow.command("decide",{duplicate:false})})}
                    ComboBox {id:candidate;objectName:"duplicateCandidate";model:selected.candidates||[];textRole:"label";Layout.fillWidth:true;Accessible.name:"Duplicate occurrence"}
                    Button {objectName:"inspectRemovalHistory";text:"Inspect removal history";visible:candidate.currentIndex>=0 && !!candidate.model[candidate.currentIndex].removed;onClicked:guard(function(){let c=candidate.model[candidate.currentIndex];navigate("history");workflow.command("entry_history",{batch:c.batch,ordinal:c.ordinal})})}
                    Button {objectName:"excludeDuplicate";text:candidate.currentIndex>=0 && candidate.model[candidate.currentIndex].removed?"Keep removed":"Exclude against selected occurrence";enabled:candidate.currentIndex>=0;onClicked:guard(function(){let c=candidate.model[candidate.currentIndex];workflow.command("decide",{duplicate:true,batch:c.batch,ordinal:c.ordinal})})}
                }
                RowLayout {
                    visible:!!s.rulesSupported && !!selected.batch
                    Label {objectName:"ruleWinners";text:(selected.winnerLines||[]).join("\n");visible:text.length>0;wrapMode:Text.Wrap;Layout.fillWidth:true;textFormat:Text.PlainText}
                    Button {objectName:"editMatchedRule";text:"Edit rule…";visible:(selected.ruleExplanations||[]).length>0;onClicked:openMatched()}
                    Button {objectName:"createRuleFromPayee";text:"Create rule from source payee";onClicked:createFromSource("payee")}
                    Button {objectName:"createRuleWhenNone";text:"Create rule…";visible:(selected.ruleExplanations||[]).length===0;onClicked:createFromSource("payee")}
                }
                Label {visible:!!s.rulesSupported && !!selected.ruleMatchError;textFormat:Text.PlainText;text:selected.ruleMatchError||"";color:"#a02020";wrapMode:Text.Wrap;Layout.fillWidth:true}
                CheckBox {id:sourceToggle;objectName:"sourceToggle";text:"Show immutable QIF source"}
                Label {
                    visible:sourceToggle.checked && (selected.sourceFields||[]).length>0;Layout.fillWidth:true;wrapMode:Text.Wrap
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
                            MouseArea {anchors.fill:parent;enabled:editEnabled;onDoubleClicked:copySource(sourceField)}
                        }
                        Button {
                            text:"Copy to edit";objectName:"copySource_"+sourceField.field+"_"+sourceField.split
                            enabled:editEnabled && (sourceField.split<0 || sourceField.split<splitModel.count)
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
