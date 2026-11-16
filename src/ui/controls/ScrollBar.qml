import QtQuick
import QtQuick.Controls.Basic as Basic

// The Basic bar stays present and takes presses after its thumb fades.
// Leave the control out until there is something to scroll.
Basic.ScrollBar {
    visible: policy !== Basic.ScrollBar.AlwaysOff && size > 0 && size < 0.999
}
