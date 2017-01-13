#import <Cocoa/Cocoa.h>
#import "sharedworkspace.h"
#include "event.h"
#include "axlib.h"

#define internal static

@interface WorkspaceWatcher : NSObject {
}
- (id)init;
@end

internal WorkspaceWatcher *Watcher;

void SharedWorkspaceInitialize()
{
    Watcher = [[WorkspaceWatcher alloc] init];
}

shared_ws_map SharedWorkspaceRunningApplications()
{
    shared_ws_map List;
    for(NSRunningApplication *Application in [[NSWorkspace sharedWorkspace] runningApplications])
    {
        if([Application activationPolicy] == NSApplicationActivationPolicyRegular)
        {
            pid_t PID = Application.processIdentifier;
            std::string Name = "[Unknown]";
            const char *NamePtr = [[Application localizedName] UTF8String];
            if(NamePtr)
                Name = NamePtr;

            List[PID] = Name;
        }
    }

    return List;
}

void SharedWorkspaceActivateApplication(pid_t PID)
{
    NSRunningApplication *Application = [NSRunningApplication runningApplicationWithProcessIdentifier:PID];
    if(Application)
    {
        [Application activateWithOptions:NSApplicationActivateIgnoringOtherApps];
    }
}

bool SharedWorkspaceIsApplicationActive(pid_t PID)
{
    Boolean Result = NO;
    NSRunningApplication *Application = [NSRunningApplication runningApplicationWithProcessIdentifier:PID];
    if(Application)
    {
        Result = [Application isActive];
    }

    return Result == YES;
}

bool SharedWorkspaceIsApplicationHidden(pid_t PID)
{
    Boolean Result = NO;
    NSRunningApplication *Application = [NSRunningApplication runningApplicationWithProcessIdentifier:PID];
    if(Application)
    {
        Result = [Application isHidden];
    }

    return Result == YES;
}

/* NOTE(koekeishiya): Subscribe to necessary notifications from NSWorkspace */
@implementation WorkspaceWatcher
- (id)init
{
    if ((self = [super init]))
    {
       [[[NSWorkspace sharedWorkspace] notificationCenter] addObserver:self
                selector:@selector(activeDisplayDidChange:)
                name:@"NSWorkspaceActiveDisplayDidChangeNotification"
                object:nil];

       [[[NSWorkspace sharedWorkspace] notificationCenter] addObserver:self
                selector:@selector(activeSpaceDidChange:)
                name:NSWorkspaceActiveSpaceDidChangeNotification
                object:nil];

       [[[NSWorkspace sharedWorkspace] notificationCenter] addObserver:self
                selector:@selector(didActivateApplication:)
                name:NSWorkspaceDidActivateApplicationNotification
                object:nil];

       [[[NSWorkspace sharedWorkspace] notificationCenter] addObserver:self
                selector:@selector(didHideApplication:)
                name:NSWorkspaceDidHideApplicationNotification
                object:nil];

       [[[NSWorkspace sharedWorkspace] notificationCenter] addObserver:self
                selector:@selector(didUnhideApplication:)
                name:NSWorkspaceDidUnhideApplicationNotification
                object:nil];

       /* NOTE(koekeishiya): We use the Carbon event system instead for these events.
       [[[NSWorkspace sharedWorkspace] notificationCenter] addObserver:self
                selector:@selector(didLaunchApplication:)
                name:NSWorkspaceDidLaunchApplicationNotification
                object:nil];

       [[[NSWorkspace sharedWorkspace] notificationCenter] addObserver:self
                selector:@selector(didTerminateApplication:)
                name:NSWorkspaceDidTerminateApplicationNotification
                object:nil];
       */
    }

    return self;
}

- (void)dealloc
{
    [[[NSWorkspace sharedWorkspace] notificationCenter] removeObserver:self];
    [super dealloc];
}

- (void)activeDisplayDidChange:(NSNotification *)notification
{
    AXLibConstructEvent(AXEvent_DisplayChanged, NULL, false);
}

- (void)activeSpaceDidChange:(NSNotification *)notification
{
    /* NOTE(koekeishiya): OSX APIs are horrible, so we need to detect which display
                          this event was triggered for. */
    ax_display *MainDisplay = AXLibMainDisplay();
    ax_display *Display = MainDisplay;
    do
    {
        ax_space *PrevSpace = Display->Space;
        Display->Space = AXLibGetActiveSpace(Display);
        Display->PrevSpace = PrevSpace;
        if(Display->Space != Display->PrevSpace)
            break;

        Display = AXLibNextDisplay(Display);
    } while(Display != MainDisplay);

    AXLibConstructEvent(AXEvent_SpaceChanged, Display, false);
}

- (void)didActivateApplication:(NSNotification *)notification
{
    pid_t PID = [[notification.userInfo objectForKey:NSWorkspaceApplicationKey] processIdentifier];
    ax_application_map *Applications = BeginAXLibApplications();
    if(Applications->find(PID) != Applications->end())
    {
        ax_application *Application = (*Applications)[PID];

        if(AXLibHasFlags(Application, AXApplication_PrepIgnoreFocus))
        {
            AXLibClearFlags(Application, AXApplication_PrepIgnoreFocus);
            AXLibAddFlags(Application, AXApplication_IgnoreFocus);
        }

        /* NOTE(koekeishiya): When an application that is already running, but has no open windows, is activated,
                              or a window is deminimized, we receive 'didApplicationActivate' notification first.
                              We have to preserve our insertion point and flag this application for activation at a later point in time. */
        if((!Application->Focus) ||
           (AXLibHasFlags(Application->Focus, AXWindow_Minimized)))
        {
            AXLibAddFlags(Application, AXApplication_Activate);
        }
        else
        {
            pid_t *ApplicationPID = (pid_t *) malloc(sizeof(pid_t));
            *ApplicationPID = Application->PID;
            AXLibConstructEvent(AXEvent_ApplicationActivated, ApplicationPID, false);
        }
    }
    EndAXLibApplications();
}

- (void)didHideApplication:(NSNotification *)notification
{
    pid_t PID = [[notification.userInfo objectForKey:NSWorkspaceApplicationKey] processIdentifier];
    ax_application_map *Applications = BeginAXLibApplications();
    if(Applications->find(PID) != Applications->end())
    {
        ax_application *Application = (*Applications)[PID];

        pid_t *ApplicationPID = (pid_t *) malloc(sizeof(pid_t));
        *ApplicationPID = Application->PID;
        AXLibConstructEvent(AXEvent_ApplicationHidden, ApplicationPID, false);
    }
    EndAXLibApplications();
}

- (void)didUnhideApplication:(NSNotification *)notification
{
    pid_t PID = [[notification.userInfo objectForKey:NSWorkspaceApplicationKey] processIdentifier];
    ax_application_map *Applications = BeginAXLibApplications();
    if(Applications->find(PID) != Applications->end())
    {
        ax_application *Application = (*Applications)[PID];

        pid_t *ApplicationPID = (pid_t *) malloc(sizeof(pid_t));
        *ApplicationPID = Application->PID;
        AXLibConstructEvent(AXEvent_ApplicationVisible, ApplicationPID, false);
    }
    EndAXLibApplications();
}

/* NOTE(koekeishiya): This notification is skipped by many applications and so we use the Carbon event system instead. */
- (void)didLaunchApplication:(NSNotification *)notification { }

/* NOTE(koekeishiya): This notification is skipped by many applications and so we use the Carbon event system instead. */
- (void)didTerminateApplication:(NSNotification *)notification { }

@end
