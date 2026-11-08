#import <AppKit/AppKit.h>

// Exercise Qt's actual Cocoa application-menu relocation, not the QML signal.
bool triggerNativeAbout() {
    NSMenu* main = [NSApp mainMenu];
    if (!main || [main numberOfItems] == 0) return false;
    NSMenu* application = [[main itemAtIndex:0] submenu];
    [application update];
    for (NSInteger index = 0; index < [application numberOfItems]; ++index) {
        NSMenuItem* item = [application itemAtIndex:index];
        if ([[item title] hasPrefix:@"About "] && [item isEnabled]) {
            [application performActionForItemAtIndex:index];
            return true;
        }
    }
    return false;
}
