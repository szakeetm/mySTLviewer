#ifdef __APPLE__
#import <Cocoa/Cocoa.h>
#include "MacMenu.h"

static Uint32 gLoadEventType = 0;

static void postLoadEvent() {
    if (gLoadEventType != 0) {
        SDL_Event e{};
        e.type = gLoadEventType;
        SDL_PushEvent(&e);
    }
}

@interface MenuTarget : NSObject
@end
@implementation MenuTarget
- (void)loadFile:(id)sender {
    postLoadEvent();
}
- (void)terminate:(id)sender {
    [NSApp terminate:sender];
}
@end

void MacMenu_SetLoadEventType(Uint32 type) {
    gLoadEventType = type;
}

void MacMenu_Init() {
    [NSApplication sharedApplication];

    NSMenu *menubar = [[NSMenu alloc] init];
    [NSApp setMainMenu:menubar];

    // App menu (About, Quit) - keep basic behavior
    NSMenuItem *appMenuItem = [[NSMenuItem alloc] init];
    [menubar addItem:appMenuItem];
    NSMenu *appMenu = [[NSMenu alloc] init];
    [appMenuItem setSubmenu:appMenu];

    NSString *appName = [[NSProcessInfo processInfo] processName];
    NSString *quitTitle = [@"Quit " stringByAppendingString:appName];
    NSMenuItem *quitItem = [[NSMenuItem alloc] initWithTitle:quitTitle
                                                       action:@selector(terminate:)
                                                keyEquivalent:@"q"];
    [quitItem setKeyEquivalentModifierMask:NSEventModifierFlagCommand];
    [appMenu addItem:quitItem];

    // File menu
    NSMenuItem *fileMenuItem = [[NSMenuItem alloc] init];
    [menubar addItem:fileMenuItem];
    NSMenu *fileMenu = [[NSMenu alloc] initWithTitle:@"File"];
    [fileMenuItem setSubmenu:fileMenu];

    MenuTarget *target = [[MenuTarget alloc] init];

    NSMenuItem *loadItem = [[NSMenuItem alloc] initWithTitle:@"Load"
                                                       action:@selector(loadFile:)
                                                keyEquivalent:@"o"];
    [loadItem setKeyEquivalentModifierMask:NSEventModifierFlagCommand];
    [loadItem setTarget:target];
    [fileMenu addItem:loadItem];

    NSMenuItem *exitItem = [[NSMenuItem alloc] initWithTitle:@"Exit"
                                                       action:@selector(terminate:)
                                                keyEquivalent:@"w"];
    [exitItem setKeyEquivalentModifierMask:NSEventModifierFlagCommand];
    [exitItem setTarget:target];
    [fileMenu addItem:exitItem];
}
#endif
