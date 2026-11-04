import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    id:root
    required property var result
    required property var period
    required property int periodIndex
    required property string graph
    signal drill(int row)
    readonly property bool negative: result.rows.some(r=>r.cells[periodIndex].value<0)
    readonly property bool empty: !result.rows.some(r=>r.cells[periodIndex].value!==0)
    Layout.preferredWidth:360
    Label {text:root.period.label;font.bold:true;Layout.alignment:Qt.AlignHCenter}
    Label {text:root.period.start+" – "+root.period.end;Layout.alignment:Qt.AlignHCenter}
    Label {text:root.result.currency+" "+root.result.totals[root.periodIndex];font.bold:true;Layout.alignment:Qt.AlignHCenter}
    Label {visible:root.graph==="pie" && root.negative;text:"This period has negative amounts. Choose Bar to display them.";wrapMode:Text.Wrap;Layout.fillWidth:true}
    Label {visible:root.empty;text:"No amounts in this period.";Layout.alignment:Qt.AlignHCenter}
    Label {visible:root.graph==="bar";text:"Shared scale: "+root.result.minimumLabel+" to "+root.result.maximumLabel;Layout.alignment:Qt.AlignHCenter}
    Canvas {
        id:plot;Layout.fillWidth:true;Layout.preferredHeight:260
        visible:!root.empty && !(root.graph==="pie" && root.negative)
        property var chartData:root.result
        property string kind:root.graph
        onChartDataChanged:requestPaint()
        onKindChanged:requestPaint()
        onWidthChanged:requestPaint()
        onVisibleChanged:requestPaint()
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
                for(let i=0;i<rows.length;i++) {
                    const value=rows[i].cells[root.periodIndex].value,edge=y(value)
                    ctx.fillStyle=Qt.hsla(rows[i].hue/360,0.60,0.48,1)
                    const barWidth=Math.max(1,Math.min(48,step*0.75))
                    ctx.fillRect(12+i*step+(step-barWidth)/2,Math.min(edge,baseline),barWidth,Math.abs(edge-baseline))
                }
            }
        }
    }
    Repeater {
        model:root.result.rows
        delegate:ItemDelegate {
            required property var modelData
            required property int index
            Layout.fillWidth:true
            text:modelData.name+"  "+modelData.cells[root.periodIndex].amount
            icon.color:Qt.hsla(modelData.hue/360,0.60,0.48,1)
            contentItem:RowLayout {
                Rectangle {width:12;height:12;color:Qt.hsla(modelData.hue/360,0.60,0.48,1)}
                Label {text:modelData.name;elide:Text.ElideRight;Layout.fillWidth:true}
                Label {text:modelData.cells[root.periodIndex].amount}
            }
            onClicked:root.drill(index)
            ToolTip.visible:hovered
            ToolTip.text:modelData.name+": "+root.result.currency+" "+modelData.cells[root.periodIndex].amount
        }
    }
    Item {Layout.fillHeight:true}
}
