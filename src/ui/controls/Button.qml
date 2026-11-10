import QtQuick.Controls.Basic as Basic

// The Basic button keeps the same fill when it cannot be used. Fade the
// whole control so an unavailable action is visibly different from one
// that can be used. An instance opacity binding replaces this one.
// Calendar day cells combine their month fade with this same 0.4 factor.
Basic.Button {
    opacity: enabled ? 1 : 0.4
}
