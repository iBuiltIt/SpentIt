import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window

Item {
    id:root;objectName:"reportsWorkspace"
    required property var workflow
    property var guardAction: function(action) { action() }
    property alias ledgerHost: detailLedgerHost
    Theme { id: reportTheme }
    readonly property var state:workflow.state
    property var drafts:[]
    property var savedDefinitions:[]
    property int selectedIndex:-1
    property int generation:0
    property string mode:"workspace"
    property string feedback:""
    property bool savingAll:false
    property string showAfterSave:""
    property var leaveAction:null
    property bool leaving:false
    property alias leaveWindow: leaveDialog
    signal leaveShown()
    signal leaveHiding()
    signal helpRequested(string target)
    readonly property var current:selectedIndex>=0 && selectedIndex<drafts.length?drafts[selectedIndex]:null
    readonly property bool dirty:{generation;return JSON.stringify(drafts)!==JSON.stringify(savedDefinitions)}
    readonly property var presetIds:["ytd","this_year","last_year","this_quarter","last_quarter","this_month","last_month","six_months","previous_year","custom"]
    readonly property var presetNames:["Year to date","This year","Last year","This quarter","Last quarter","This month","Previous calendar month","Last 6 complete months","Same period last year","Custom"]
    readonly property var accountChoices:(state.accounts||[]).concat(state.archivedAccounts||[])
    readonly property var categoryChoices:[{id:"",name:"Uncategorized"}].concat((state.categories||[]).filter(c=>!c.deleted && !c.group).sort((a,b)=>Number(!!a.income)-Number(!!b.income) || a.name.toLowerCase().localeCompare(b.name.toLowerCase()) || a.id.localeCompare(b.id)))
    readonly property var result:state.reportResult||{}
    function copy(v){return JSON.parse(JSON.stringify(v))}
    function changed(index) {generation;return JSON.stringify(drafts[index])!==JSON.stringify(savedDefinitions.find(r=>r.id===drafts[index].id))}
    function touch(){generation++;feedback=""}
    function load() {savedDefinitions=copy(state.reports||[]);drafts=copy(savedDefinitions);selectedIndex=Math.min(selectedIndex,drafts.length-1);if(selectedIndex<0 && drafts.length)selectedIndex=0;generation++}
    function reset(){if(leaving)leaveHiding();drafts=[];savedDefinitions=[];selectedIndex=-1;mode="workspace";feedback="";leaving=false;leaveAction=null;showAfterSave="";generation++}
    function newReport(duplicate) {
        if(drafts.some(r=>!savedDefinitions.some(b=>b.id===r.id))){feedback="Save or discard the new report first.";return}
        let value=duplicate && current?copy(current):{name:"New report",graph:"bar",accounts:accountChoices.map(a=>a.id),categories:categoryChoices.map(c=>c.id),periods:[{preset:"ytd",start:"",end:""}]}
        value.id=workflow.newIdentity();if(duplicate)value.name+=" copy"
        drafts=drafts.concat([value]);selectedIndex=drafts.length-1;touch()
    }
    function updatePeriod(index,value){if(current){current.periods[index]=value;touch()}}
    function setField(name,value){if(current){current[name]=value;touch()}}
    function selectAll(field,values){if(current){current[field]=values;touch()}}
    function check(field,id,enabled) {
        if(!current)return
        let values=current[field].slice();const index=values.indexOf(id)
        if(enabled && index<0)values.push(id);if(!enabled && index>=0)values.splice(index,1)
        if(field==="categories" && enabled && id){let parent=categoryChoices.find(c=>c.id===id);if(parent)for(const c of categoryChoices)if(workflow.categoryNameMatches(c.name,parent.name,true) && values.indexOf(c.id)<0)values.push(c.id)}
        current[field]=values;touch()
    }
    function categoryGroup(income) {return categoryChoices.filter(c=>c.id && !!c.income===income)}
    function groupCheckState(income) {
        generation
        const ids=categoryGroup(income).map(c=>c.id)
        const count=current?ids.filter(id=>current.categories.indexOf(id)>=0).length:0
        return count===0?Qt.Unchecked:count===ids.length?Qt.Checked:Qt.PartiallyChecked
    }
    function checkGroup(income,enabled) {
        if(!current)return
        const ids=categoryGroup(income).map(c=>c.id)
        let values=current.categories.filter(id=>ids.indexOf(id)<0)
        if(enabled)values=values.concat(ids)
        current.categories=values;touch()
    }
    function choiceRows(field,query) {
        const matches=c=>c.name.toLowerCase().indexOf(query.toLowerCase())>=0
        if(field==="accounts")return accountChoices.filter(matches)
        let rows=categoryChoices.filter(c=>!c.id && matches(c))
        for(const income of [false,true]) {
            const choices=categoryGroup(income).filter(matches)
            if(choices.length)rows=rows.concat([{section:true,income:income,name:income?"Income":"Expenses"}],choices)
        }
        return rows
    }
    function save(all,show) {
        if(workflow.busy)return
        savingAll=all;showAfterSave=show && current?current.id:""
        let image=all?copy(drafts):copy(savedDefinitions)
        if(!all && current){const index=image.findIndex(r=>r.id===current.id);if(index>=0)image[index]=copy(current);else image.push(copy(current))}
        workflow.command("report_save",{revision:state.reportRevision,reports:image})
    }
    function discard() {
        if(!current)return
        const id=current.id;const original=savedDefinitions.find(r=>r.id===id);let next=copy(drafts)
        if(original)next[selectedIndex]=copy(original);else next.splice(selectedIndex,1)
        drafts=next;selectedIndex=Math.min(selectedIndex,drafts.length-1);touch()
    }
    function deleteCurrent(){if(!current)return;drafts=drafts.filter(r=>r.id!==current.id);selectedIndex=Math.min(selectedIndex,drafts.length-1);touch()}
    function show(index){selectedIndex=index;if(changed(index))save(false,true);else workflow.command("report_show",{id:current.id})}
    function requestLeave(action){leaveAction=action;leaving=true;leaveShown()}
    function stay(){if(leaving)leaveHiding();leaving=false;leaveAction=null}
    function proceed(){if(leaving)leaveHiding();let action=leaveAction;leaveAction=null;leaving=false;if(action)action()}
    function back(){guardAction(function(){if(root.mode==="detail")root.workflow.command("report_leave");else root.mode="workspace"})}
    function drill(currency,period,row){workflow.command("report_drill",{currency:currency,period:period,row:row})}
    Connections {
        target:root.workflow
        function onCompleted(action,success){
            if(["create","open","unlock","close"].indexOf(action)>=0 && success){root.reset();if(root.state.active)root.load()}
            if(action==="report_load" && success && !root.dirty)root.load()
            if(action==="report_save"){
                if(!success){root.showAfterSave="";root.stay();return}
                const saved=root.copy(root.state.reports||[])
                let id=root.current?root.current.id:""
                let merged=root.savingAll?root.copy(saved):root.copy(root.drafts)
                if(!root.savingAll){const index=merged.findIndex(r=>r.id===id);const stored=saved.find(r=>r.id===id);if(index>=0 && stored)merged[index]=root.copy(stored)}
                root.drafts=merged;root.savedDefinitions=saved;root.generation++
                if(root.leaving)root.proceed()
                else if(root.showAfterSave){let id=root.showAfterSave;root.showAfterSave="";root.workflow.command("report_show",{id:id})}
            }
            if(action==="report_show" && success)root.mode="result"
            if(action==="report_drill" && success)root.mode="detail"
            if(action==="report_leave" && success)root.mode="result"
        }
    }
    Window {
        id:leaveDialog;objectName:"reportsLeaveDialog";visible:root.leaving
        modality:Qt.ApplicationModal;flags:Qt.Dialog|Qt.WindowTitleHint|Qt.WindowCloseButtonHint
        title:"Save changes before leaving Reports?";width:460;height:150
        onVisibleChanged:if(visible){x=root.Window.window.x+(root.Window.window.width-width)/2;y=root.Window.window.y+(root.Window.window.height-height)/2;requestActivate()}else if(root.Window.window)root.Window.window.requestActivate()
        onClosing:function(event){if(root.leaving){event.accepted=false;if(!root.workflow.busy)root.stay()}}
        Shortcut {sequence:"Escape";enabled:root.leaving && !root.workflow.busy;onActivated:root.stay()}
        Shortcut {sequences:[StandardKey.HelpContents];context:Qt.WindowShortcut;enabled:root.leaving;onActivated:root.helpRequested("help:answer-with-reports#keep-and-change-your-reports")}
        ColumnLayout {
            anchors.fill:parent;anchors.margins:20
            Label {textFormat:Text.PlainText;text:"Save your changes to Reports before leaving?";wrapMode:Text.Wrap;Layout.fillWidth:true}
            RowLayout {Layout.alignment:Qt.AlignRight
                Button {objectName:"reportsGuardHelp";text:"Help";focusPolicy:Qt.TabFocus;onClicked:root.helpRequested("help:answer-with-reports#keep-and-change-your-reports")}
                Button {objectName:"reportsGuardSave";enabled:!root.workflow.busy;text:"Save";onClicked:root.save(true,false)}
                Button {objectName:"reportsGuardDiscard";enabled:!root.workflow.busy;text:"Discard";onClicked:{root.load();root.proceed()}}
                Button {objectName:"reportsGuardStay";enabled:!root.workflow.busy;text:"Stay";onClicked:root.stay()}
            }
        }
    }
    ColumnLayout {
        anchors.fill:parent;anchors.margins:12;spacing:8
        RowLayout {
            Label {textFormat:Text.PlainText;text:root.mode==="workspace"?"Reports":root.mode==="detail"?"Report transactions":root.result.name||"Report";font.pixelSize:22;Layout.fillWidth:true;Layout.minimumWidth:80;elide:Text.ElideRight}
            Button {visible:root.mode!=="workspace";text:root.mode==="detail"?"Back to report":"Back to Reports";onClicked:root.back()}
            Button {visible:root.mode==="result";text:"Refresh";enabled:!root.workflow.busy;onClicked:root.workflow.command("report_show",{id:root.result.id})}
        }
        Label {textFormat:Text.PlainText;visible:root.feedback.length>0;text:root.feedback;wrapMode:Text.Wrap;Layout.fillWidth:true}
        Button {objectName:"reportFeedbackHelp";text:"Help with reports";focusPolicy:Qt.TabFocus;visible:root.feedback.length>0;onClicked:root.helpRequested("help:answer-with-reports#if-the-result-needs-attention")}
        ScrollView {
            id:editorScroll;visible:root.mode==="workspace";Layout.fillWidth:true;Layout.fillHeight:true;contentWidth:availableWidth;enabled:!root.workflow.busy
            ColumnLayout {
                width:editorScroll.availableWidth;spacing:10
                RowLayout {
                    ClearableSearchField {id:reportSearch;placeholderText:"Search reports";Layout.fillWidth:true}
                    Button {text:"New report";objectName:"newReport";onClicked:root.newReport(false)}
                    Button {text:"Duplicate";enabled:!!root.current;onClicked:root.newReport(true)}
                }
                Repeater {
                    model:root.drafts
                    RowLayout {
                        required property var modelData;required property int index
                        visible:{root.generation;const item=root.drafts[index];return !!item && (index===root.selectedIndex || item.name.toLowerCase().indexOf(reportSearch.text.toLowerCase())>=0)}
                        Layout.fillWidth:true
                        Button {text:{root.generation;return root.drafts[index]?root.drafts[index].name+(root.changed(index)?" *":""):""} highlighted:index===root.selectedIndex;Layout.fillWidth:true;onClicked:root.selectedIndex=index
                            TapHandler {acceptedButtons:Qt.RightButton;onTapped:{root.selectedIndex=index;reportContext.popup()}}
                        }
                        Button {text:"Show";onClicked:root.show(index)}
                    }
                }
                Menu {id:reportContext;MenuItem {text:"Rename";onTriggered:reportName.forceActiveFocus()} MenuItem {text:"Delete";onTriggered:root.deleteCurrent()}}
                Label {textFormat:Text.PlainText;visible:!root.drafts.length;text:"Create a report to compare your spending across periods.";wrapMode:Text.Wrap;Layout.fillWidth:true}
                ColumnLayout {
                    visible:!!root.current;Layout.fillWidth:true;spacing:10
                    Label {textFormat:Text.PlainText;text:"Report details";font.pixelSize:18}
                    TextField {id:reportName;objectName:"reportName";text:root.current?root.current.name:"";placeholderText:"Report name";Layout.fillWidth:true;onTextEdited:root.setField("name",text)}
                    RowLayout {
                        Label {textFormat:Text.PlainText;text:"Graph"}
                        ComboBox {objectName:"reportGraph";model:["None","Bar — separate periods","Bar — grouped periods","Pie"];currentIndex:root.current?["none","bar","grouped_bar","pie"].indexOf(root.current.graph):0;onActivated:root.setField("graph",["none","bar","grouped_bar","pie"][currentIndex])}
                    }
                    GridLayout {
                        Layout.fillWidth:true;columns:root.width<700?1:2
                        Repeater {
                            model:[{key:"accounts",label:"Accounts"},{key:"categories",label:"Categories"}]
                            ColumnLayout {
                                id:choiceColumn
                                required property var modelData
                                readonly property var choices:modelData.key==="accounts"?root.accountChoices:root.categoryChoices
                                Layout.fillWidth:true;Layout.preferredWidth:300
                                RowLayout {
                                    Label {textFormat:Text.PlainText;text:{root.generation;return modelData.label+" ("+(root.current?root.current[modelData.key].length:0)+")"} Layout.fillWidth:true}
                                    Button {text:"All";onClicked:root.selectAll(modelData.key,choices.map(c=>c.id))}
                                    Button {text:"None";onClicked:root.selectAll(modelData.key,[])}
                                }
                                ClearableSearchField {id:choiceSearch;objectName:"reportChoiceSearch_"+choiceColumn.modelData.key;placeholderText:"Search "+modelData.label.toLowerCase();Layout.fillWidth:true}
                                ScrollView {objectName:"reportChoiceScroll_"+choiceColumn.modelData.key;Layout.fillWidth:true;Layout.preferredHeight:180;contentWidth:availableWidth
                                    ColumnLayout {width:parent.width
                                        Repeater {model:root.choiceRows(choiceColumn.modelData.key,choiceSearch.text)
                                            CheckBox {
                                                required property var modelData
                                                objectName:modelData.section?(modelData.income?"reportIncomeGroup":"reportExpensesGroup"):"reportChoice_"+choiceColumn.modelData.key+"_"+modelData.id
                                                text:modelData.name+(modelData.currency?" · "+modelData.currency:"")+(modelData.archived?" (archived)":"")
                                                font.bold:!!modelData.section
                                                leftPadding:modelData.section || !modelData.id || modelData.currency?0:16
                                                tristate:!!modelData.section
                                                checkState:{root.generation;return modelData.section?root.groupCheckState(modelData.income):root.current && root.current[choiceColumn.modelData.key].indexOf(modelData.id)>=0?Qt.Checked:Qt.Unchecked}
                                                nextCheckState:function(){return checkState===Qt.Checked?Qt.Unchecked:Qt.Checked}
                                                onClicked:if(modelData.section)root.checkGroup(modelData.income,checkState===Qt.Checked);else root.check(choiceColumn.modelData.key,modelData.id,checked)
                                                ToolTip.visible:hovered && !!modelData.section
                                                ToolTip.text:"Select or clear all "+(modelData.income?"income":"expense")+" categories, including those hidden by search."
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                    Repeater {
                        model:root.current?root.current.periods:[]
                        ColumnLayout {
                            id:periodRow;required property var modelData;required property int index
                            readonly property var choices:root.presetIds.filter(id=>index>0 || id!=="previous_year")
                            property var period:{root.generation;return root.current && root.current.periods[index]?root.current.periods[index]:{preset:"ytd",start:"",end:""}}
                            property var resolved:{root.generation;return root.workflow.reportPeriod(period,root.current?root.current.periods[0]:{})}
                            Layout.fillWidth:true
                            function setDate(field,value){
                                const parsed=value.length?root.workflow.calendarPage(value):{error:"Enter a date."}
                                let valuePeriod=root.copy(period)
                                valuePeriod.preset="custom"
                                valuePeriod.start=resolved.start||period.start;valuePeriod.end=resolved.end||period.end
                                valuePeriod[field]=parsed.error?"!"+value:parsed.iso
                                root.updatePeriod(index,valuePeriod)
                            }
                            RowLayout {
                                Label {textFormat:Text.PlainText;text:periodRow.index===0?"Main period":"Comparison "+periodRow.index}
                                ComboBox {model:periodRow.choices.map(id=>root.presetNames[root.presetIds.indexOf(id)]);currentIndex:periodRow.choices.indexOf(periodRow.period.preset);onActivated:{let value=root.copy(periodRow.period);value.start=periodRow.resolved.start||value.start;value.end=periodRow.resolved.end||value.end;value.preset=periodRow.choices[currentIndex];root.updatePeriod(periodRow.index,value)}}
                                Button {text:"Remove";visible:periodRow.index>0;onClicked:{let next=root.copy(root.drafts);next[root.selectedIndex].periods.splice(periodRow.index,1);root.drafts=next;root.touch()}}
                            }
                            RowLayout {
                                Label {textFormat:Text.PlainText;text:"From"}
                                TextField {id:startField;objectName:"reportStartDate"+periodRow.index;text:periodRow.resolved.startDisplay||(periodRow.period.start||"").replace(/^!/, "");placeholderText:root.state.dateFormat||"YYYY-MM-DD";onTextEdited:periodRow.setDate("start",text);Layout.fillWidth:true}
                                CalendarPicker {workflow:root.workflow;field:startField;onSelected:function(value){periodRow.setDate("start",value)}}
                                Label {textFormat:Text.PlainText;text:"through"}
                                TextField {id:endField;objectName:"reportEndDate"+periodRow.index;text:periodRow.resolved.endDisplay||(periodRow.period.end||"").replace(/^!/, "");placeholderText:root.state.dateFormat||"YYYY-MM-DD";onTextEdited:periodRow.setDate("end",text);Layout.fillWidth:true}
                                CalendarPicker {workflow:root.workflow;field:endField;onSelected:function(value){periodRow.setDate("end",value)}}
                            }
                            Label {textFormat:Text.PlainText;visible:!!periodRow.resolved.error;text:periodRow.resolved.error||"";wrapMode:Text.Wrap;Layout.fillWidth:true}
                        }
                    }
                    Button {text:"Add comparison";onClicked:{let next=root.copy(root.drafts);next[root.selectedIndex].periods.push({preset:"previous_year",start:"",end:""});root.drafts=next;root.touch()}}
                    RowLayout {
                        Button {text:"Save";objectName:"saveReport";onClicked:root.save(false,false)}
                        Button {text:"Discard";onClicked:root.discard()}
                        Button {text:"Save and show";objectName:"showReport";onClicked:root.save(false,true)}
                        Button {text:"Delete";onClicked:root.deleteCurrent()}
                    }
                }
                Button {visible:root.dirty;text:"Save all changes";onClicked:root.save(true,false)}
            }
        }
        Label {textFormat:Text.PlainText;visible:root.mode!=="workspace";text:"Net spending: outflows are positive; refunds reduce spending; income is negative.";wrapMode:Text.Wrap;Layout.fillWidth:true}
        Label {textFormat:Text.PlainText;visible:root.mode!=="workspace" && (root.state.importFailures||0)>0;text:"This file has "+(root.state.importFailures||0)+" imported records needing repair. They have no reportable amount. Open Review to repair them.";wrapMode:Text.Wrap;Layout.fillWidth:true}
        Button {objectName:"staleReportHelp";text:"Help with this result";focusPolicy:Qt.TabFocus;visible:root.mode!=="workspace" && !!root.result.stale;onClicked:root.helpRequested("help:answer-with-reports#if-the-result-needs-attention")}
        Label {textFormat:Text.PlainText;visible:root.mode!=="workspace" && !!root.result.stale;text:"Transactions changed. Refresh the report to see current amounts.";wrapMode:Text.Wrap;Layout.fillWidth:true}
        ScrollView {
            id:resultScroll;visible:root.mode==="result";Layout.fillWidth:true;Layout.fillHeight:true;contentWidth:availableWidth
            ColumnLayout {width:resultScroll.availableWidth;spacing:24
                Label {textFormat:Text.PlainText;visible:!(root.result.currencies||[]).length;text:"No accounts selected. Edit the report to choose accounts."}
                Repeater {
                    model:root.result.currencies||[]
                    ColumnLayout {
                        id:currencyBlock;required property var modelData;required property int index
                        Layout.fillWidth:true
                        Label {textFormat:Text.PlainText;text:currencyBlock.modelData.currency;font.pixelSize:20}
                        Flickable {
                            id:graphs;objectName:"reportGraphs";visible:root.result.graph!=="none";Layout.fillWidth:true;Layout.preferredHeight:visible?charts.implicitHeight+16:0;clip:true
                            contentWidth:charts.implicitWidth;contentHeight:height;flickableDirection:Flickable.HorizontalFlick
                            ScrollBar.horizontal:ScrollBar {}
                            RowLayout {id:charts;spacing:20
                                Repeater {model:root.result.graph==="grouped_bar"?[root.result.periods[0]]:(root.result.periods||[])
                                    ReportChart {required property var modelData;required property int index;result:currencyBlock.modelData;period:modelData;viewportWidth:graphs.width;periods:root.result.periods||[];periodIndex:index;graph:root.result.graph;enabled:!root.workflow.busy && !root.result.stale;onDrill:function(row,period){root.drill(currencyBlock.index,period,row)}}
                                }
                            }
                        }
                        Item {
                            id:table;objectName:"reportCategoryTable";Layout.fillWidth:true;Layout.preferredHeight:64+(currencyBlock.modelData.rows.length+1)*34+16
                            Rectangle {width:parent.width;height:64;color:reportTheme.surface}
                            Label {textFormat:Text.PlainText;text:"Category";width:220;height:64;leftPadding:8;verticalAlignment:Text.AlignVCenter}
                            Flickable {
                                x:220;width:Math.max(0,parent.width-220);height:64;clip:true;interactive:false
                                contentWidth:(root.result.periods||[]).length*220;contentX:tableBody.contentX
                                Row {Repeater {model:root.result.periods||[]
                                    Label {required property var modelData;textFormat:Text.PlainText;text:modelData.label+"\n"+modelData.start+" – "+modelData.end;width:220;height:64;verticalAlignment:Text.AlignVCenter;horizontalAlignment:Text.AlignHCenter;wrapMode:Text.Wrap}
                                }}
                            }
                            Column {
                                y:64;width:parent.width
                                Repeater {model:currencyBlock.modelData.rows.length+1
                                    Rectangle {required property int index;width:table.width;height:34;color:index%2?reportTheme.row:reportTheme.rowAlternate}
                                }
                            }
                            Column {
                                y:64;width:220;spacing:0
                                Label {text:"Total";font.bold:true;width:220;height:34;leftPadding:8;verticalAlignment:Text.AlignVCenter}
                                Repeater {model:currencyBlock.modelData.rows
                                    Item {required property var modelData;width:220;height:34
                                        RowLayout {anchors.fill:parent;anchors.leftMargin:8;anchors.rightMargin:8
                                            Rectangle {visible:root.result.graph!=="none";width:10;height:10;color:Qt.hsla(modelData.hue/360,0.60,0.48,1)}
                                            Label {textFormat:Text.PlainText;text:modelData.name;elide:Text.ElideRight;Layout.fillWidth:true;ToolTip.visible:categoryHover.hovered;ToolTip.text:text;HoverHandler {id:categoryHover}}
                                        }
                                    }
                                }
                            }
                            Flickable {
                                id:tableBody;x:220;y:64;width:Math.max(0,parent.width-220);height:parent.height-64;clip:true
                                contentWidth:(root.result.periods||[]).length*220;contentHeight:height;flickableDirection:Flickable.HorizontalFlick
                                ScrollBar.horizontal:ScrollBar {}
                                Row {
                                    Repeater {model:root.result.periods||[]
                                        Column {id:periodColumn;required property int index;width:220;spacing:0
                                            Button {text:currencyBlock.modelData.totals[periodColumn.index];flat:true;font.bold:true;width:220;height:34;enabled:!root.workflow.busy && !root.result.stale;onDoubleClicked:root.drill(currencyBlock.index,periodColumn.index,-1);ToolTip.visible:hovered;ToolTip.text:"Double-click to edit included transactions"}
                                            Repeater {model:currencyBlock.modelData.rows
                                                Button {required property var modelData;required property int index;text:modelData.cells[periodColumn.index].amount;flat:true;width:220;height:34;enabled:!root.workflow.busy && !root.result.stale;onDoubleClicked:root.drill(currencyBlock.index,periodColumn.index,index);ToolTip.visible:hovered;ToolTip.text:"Double-click to edit included transactions"}
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
        ColumnLayout {
            visible:root.mode==="detail";Layout.fillWidth:true;Layout.fillHeight:true
            Label {textFormat:Text.PlainText;text:(root.state.reportDetails||{}).title||"";font.pixelSize:18}
            Label {textFormat:Text.PlainText;text:(root.state.reportDetails||{}).period||""}
            Label {textFormat:Text.PlainText;text:"Total: "+((root.state.reportDetails||{}).currency||"")+" "+((root.state.reportDetails||{}).total||"");font.bold:true}
            Label {text:"Transactions opened from this report. Saved changes update the report; transactions remain here until you go back.";wrapMode:Text.Wrap;Layout.fillWidth:true}
            Label {visible:!!(root.state.reportDetails||{}).refreshError;text:(root.state.reportDetails||{}).refreshError||"";wrapMode:Text.Wrap;Layout.fillWidth:true}
            Item {id:detailLedgerHost;Layout.fillWidth:true;Layout.fillHeight:true}

        }
    }
}
