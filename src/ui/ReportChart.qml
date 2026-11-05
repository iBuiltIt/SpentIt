import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    id:root
    required property var result
    required property var period
    required property int periodIndex
    required property string graph
    property var periods: []
    property real viewportWidth:640
    readonly property bool grouped: graph==="grouped_bar"
    signal drill(int row,int period)
    readonly property bool negative: result.rows.some(r=>r.cells[periodIndex].value<0)
    readonly property bool empty: !result.rows.some(r=>grouped?r.cells.some(c=>c.value!==0):r.cells[periodIndex].value!==0)
    Layout.preferredWidth:grouped?Math.max(viewportWidth,640,result.rows.length*Math.max(1,periods.length)*18+24):360
    Label {text:root.grouped?"Period comparison":root.period.label;font.bold:true;Layout.alignment:Qt.AlignHCenter}
    Label {visible:!root.grouped;text:root.period.start+" – "+root.period.end;Layout.alignment:Qt.AlignHCenter}
    Label {visible:!root.grouped;text:root.result.currency+" "+root.result.totals[root.periodIndex];font.bold:true;Layout.alignment:Qt.AlignHCenter}
    Label {visible:root.graph==="pie" && root.negative;text:"This period has negative amounts. Choose Bar to display them.";wrapMode:Text.Wrap;Layout.fillWidth:true}
    Label {visible:root.empty;text:"No amounts in this period.";Layout.alignment:Qt.AlignHCenter}
    Label {visible:root.graph==="bar" || root.grouped;text:"Shared scale: "+root.result.minimumLabel+" to "+root.result.maximumLabel;Layout.alignment:Qt.AlignHCenter}
    Canvas {
        id:plot;Layout.fillWidth:true;Layout.preferredHeight:260
        visible:!root.empty && !(root.graph==="pie" && root.negative)
        property var chartData:root.result
        property string kind:root.graph
        onChartDataChanged:requestPaint()
        onKindChanged:requestPaint()
        onWidthChanged:requestPaint()
        onVisibleChanged:requestPaint()
        MouseArea {
            anchors.fill:parent;hoverEnabled:true
            property int categoryIndex:-1
            property int chartPeriod:root.periodIndex
            onPositionChanged:function(mouse){
                if(root.graph==="pie"){categoryIndex=-1;return}
                const step=(width-24)/Math.max(1,root.result.rows.length)
                categoryIndex=Math.floor((mouse.x-12)/step)
                if(categoryIndex<0 || categoryIndex>=root.result.rows.length){categoryIndex=-1;return}
                const count=root.grouped?root.periods.length:1
                const groupWidth=Math.min(step*0.82,count*50),slot=groupWidth/count
                chartPeriod=root.grouped?Math.max(0,Math.min(count-1,Math.floor((mouse.x-12-categoryIndex*step-(step-groupWidth)/2)/slot))):root.periodIndex
            }
            onExited:categoryIndex=-1
            onDoubleClicked:if(categoryIndex>=0)root.drill(categoryIndex,chartPeriod)
            ToolTip.visible:containsMouse && categoryIndex>=0
            ToolTip.text:categoryIndex<0?"":root.result.rows[categoryIndex].name+" · "+(root.periods[chartPeriod]||root.period).label+": "+root.result.currency+" "+root.result.rows[categoryIndex].cells[chartPeriod].amount
        }
        onPaint: {
            const ctx=getContext("2d");ctx.reset()
            const rows=root.result.rows
            if(root.graph==="pie") {
                const total=rows.reduce((n,r)=>n+r.cells[root.periodIndex].value,0)
                let angle=-Math.PI/2
                for(let i=0;i<rows.length;i++) {
                    const end=angle+2*Math.PI*rows[i].cells[root.periodIndex].value/total
                    ctx.beginPath();ctx.moveTo(width/2,height/2);ctx.arc(width/2,height/2,Math.min(width,height)/2-10,angle,end);ctx.closePath()
                    ctx.fillStyle=Qt.hsla(rows[i].hue/360,0.60,0.48,1);ctx.fill();angle=end
                }
            } else {
                const minimum=root.result.minimum,maximum=root.result.maximum
                const span=maximum-minimum || 1, top=12,bottom=height-12
                const y=v=>bottom-(v-minimum)/span*(bottom-top)
                const baseline=y(0),step=(width-24)/Math.max(1,rows.length)
                ctx.strokeStyle="#888888";ctx.beginPath();ctx.moveTo(12,baseline);ctx.lineTo(width-12,baseline);ctx.stroke()
                const count=root.grouped?root.periods.length:1
                for(let i=0;i<rows.length;i++) for(let p=0;p<count;p++) {
                    const period=root.grouped?p:root.periodIndex
                    const value=rows[i].cells[period].value,edge=y(value)
                    const light=count===1?0.48:0.36+0.26*p/(count-1)
                    ctx.fillStyle=Qt.hsla(rows[i].hue/360,0.60,light,1)
                    const groupWidth=Math.min(step*0.82,count*50),slot=groupWidth/count,barWidth=Math.max(1,slot-2)
                    ctx.fillRect(12+i*step+(step-groupWidth)/2+p*slot,Math.min(edge,baseline),barWidth,Math.abs(edge-baseline))
                }
            }
        }
    }
    Flow {
        visible:root.grouped;Layout.fillWidth:true;spacing:12
        Repeater {model:root.grouped?root.periods:[]
            Row {required property var modelData;required property int index;spacing:5
                Rectangle {width:12;height:12;color:Qt.hsla(0,0,root.periods.length===1?0.48:0.36+0.26*index/(root.periods.length-1),1)}
                Label {textFormat:Text.PlainText;text:(index+1)+". "+modelData.label+" · "+modelData.start+" – "+modelData.end}
            }
        }
    }
}
