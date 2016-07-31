#include "axlib.h"
#include <vector>

#define internal static
#define local_persist static
#define CGSDefaultConnection _CGSDefaultConnection()

typedef int CGSConnectionID;
extern "C" CGSConnectionID _CGSDefaultConnection(void);
extern "C" CGError CGSGetOnScreenWindowCount(const CGSConnectionID CID, CGSConnectionID TID, int *Count);
extern "C" CGError CGSGetOnScreenWindowList(const CGSConnectionID CID, CGSConnectionID TID, int Count, int *List, int *OutCount);

internal ax_state *AXState;
internal carbon_event_handler *Carbon;
internal std::map<pid_t, ax_application> *AXApplications;
internal std::map<CGDirectDisplayID, ax_display> *AXDisplays;
internal CGPoint Cursor;

internal inline AXUIElementRef
AXLibSystemWideElement()
{
    local_persist AXUIElementRef AXLibSystemWideElement;
    local_persist dispatch_once_t OnceToken;

    dispatch_once(&OnceToken, ^{
        AXLibSystemWideElement = AXUIElementCreateSystemWide();
    });

    return AXLibSystemWideElement;
}

internal CGPoint
GetCursorPos()
{
    CGEventRef Event = CGEventCreate(NULL);
    CGPoint Cursor = CGEventGetLocation(Event);
    CFRelease(Event);

    return Cursor;
}

internal bool
IsElementBelowCursor(CGRect *WindowRect)
{
    if(Cursor.x >= WindowRect->origin.x &&
       Cursor.x <= WindowRect->origin.x + WindowRect->size.width &&
       Cursor.y >= WindowRect->origin.y &&
       Cursor.y <= WindowRect->origin.y + WindowRect->size.height)
        return true;

    return false;
}

/* NOTE(koekeishiya): Does AXLib already have an ax_application struct for the given process id (?) .*/
internal bool
AXLibIsApplicationCached(pid_t PID)
{
    std::map<pid_t, ax_application>::iterator It = AXApplications->find(PID);
    return It != AXApplications->end();
}

/* NOTE(koekeishiya): Returns a pointer to the ax_application struct corresponding to the given process id. */
ax_application *AXLibGetApplicationByPID(pid_t PID)
{
    return AXLibIsApplicationCached(PID) ? &(*AXApplications)[PID] : NULL;
}

/* NOTE(koekeishiya): Returns a pointer to the ax_application struct that is the current active application. */
ax_application *AXLibGetFocusedApplication()
{
    AXUIElementRef Ref = (AXUIElementRef) AXLibGetWindowProperty(AXLibSystemWideElement(), kAXFocusedApplicationAttribute);

    if(Ref)
    {
        pid_t PID;
        AXUIElementGetPid(Ref, &PID);
        CFRelease(Ref);

        return AXLibGetApplicationByPID(PID);
    }

    return NULL;
}

/* NOTE(koekeishiya): Returns a pointer to the ax_window struct that is the current focused window of an application. */
ax_window *AXLibGetFocusedWindow(ax_application *Application)
{
    AXUIElementRef Ref = (AXUIElementRef) AXLibGetWindowProperty(Application->Ref, kAXFocusedWindowAttribute);
    if(Ref)
    {
        uint32_t WID = AXLibGetWindowID(Ref);
        CFRelease(Ref);

        ax_window *Window = AXLibFindApplicationWindow(Application, WID);
        return Window;
    }

    return NULL;
}

/* NOTE(koekeishiya): The passed ax_window will now become the focused window of OSX. If the
                      application corresponding to this window is not active, it will be activated. */
void AXLibSetFocusedWindow(ax_window *Window)
{
    AXLibSetWindowProperty(Window->Ref, kAXMainAttribute, kCFBooleanTrue);
    AXLibSetWindowProperty(Window->Ref, kAXFocusedAttribute, kCFBooleanTrue);
    AXUIElementPerformAction(Window->Ref, kAXRaiseAction);
    SetFrontProcessWithOptions(&Window->Application->PSN, kSetFrontProcessFrontWindowOnly);

    /* NOTE(koekeishiya): Calling SetFrontProcessWithOptions is faster than performing this sequence of functions.
    if(!AXLibIsApplicationActive(Window->Application))
        AXLibActivateApplication(Window->Application);
    */
}

/* NOTE(koekeishiya): Returns a vector of all windows that we currently know about. This fuction is probably not necessary. */
std::vector<ax_window *> AXLibGetAllKnownWindows()
{
    std::vector<ax_window *> Windows;
    std::map<pid_t, ax_application>::iterator It;
    for(It = AXApplications->begin(); It != AXApplications->end(); ++It)
    {
        ax_application *Application = &It->second;
        std::map<uint32_t, ax_window*>::iterator WIt;
        for(WIt = Application->Windows.begin(); WIt != Application->Windows.end(); ++WIt)
            Windows.push_back(WIt->second);
    }

    return Windows;
}

internal inline bool
AXLibArrayContains(int *WindowList, int WindowCount, uint32_t WindowID)
{
    while(WindowCount > 0)
    {
        if(WindowList[--WindowCount] == WindowID)
            return true;
    }

    return false;
}

/* NOTE(koekeishiya): Returns a list of pointer to ax_window structs containing all windows currently visible,
                     filtering by their associated kAXWindowRole and kAXWindowSubrole. */
std::vector<ax_window *> AXLibGetAllVisibleWindows()
{
    std::vector<ax_window *> Windows;

    /* NOTE(koekeishiya): Is it necessary to actually decide how many windows are on the screen.
                          Can we just pass an estimated high enough number such as 200 (?) */
    int WindowCount = 0;
    CGError Error = CGSGetOnScreenWindowCount(CGSDefaultConnection, 0, &WindowCount);
    if(Error == kCGErrorSuccess)
    {
        /* NOTE(koekeishiya): This function seems to be pretty expensive.. Is CGWindowListCopyWindowInfo faster (?) */
        int WindowList[WindowCount];
        Error = CGSGetOnScreenWindowList(CGSDefaultConnection, 0, WindowCount, WindowList, &WindowCount);
        if(Error == kCGErrorSuccess)
        {
            std::map<pid_t, ax_application>::iterator It;
            for(It = AXApplications->begin(); It != AXApplications->end(); ++It)
            {
                ax_application *Application = &It->second;
                if(!AXLibIsApplicationHidden(Application))
                {
                    std::map<uint32_t, ax_window*>::iterator WIt;
                    for(WIt = Application->Windows.begin(); WIt != Application->Windows.end(); ++WIt)
                    {
                        ax_window *Window = WIt->second;
                        /* NOTE(koekeishiya): If a window is minimized, the ArrayContains check should fail
                                              if(!AXLibIsWindowMinimized(Window->Ref)) */

                        if((AXLibArrayContains(WindowList, WindowCount, Window->ID)) &&
                           (AXLibIsWindowStandard(Window) || AXLibIsWindowCustom(Window)) &&
                           (!AXLibHasFlags(Window, AXWindow_Floating)))
                        {
                            Windows.push_back(Window);
                        }
                    }
                }
            }
        }
    }

    return Windows;
}

/* NOTE(koekeishiya): Returns the window id of the window below the cursor. */
#define CONTEXT_MENU_LAYER 101
uint32_t AXLibGetWindowBelowCursor()
{
    uint32_t Result = 0;
    CGWindowListOption WindowListOption = kCGWindowListOptionOnScreenOnly |
                                          kCGWindowListExcludeDesktopElements;
    Cursor = GetCursorPos();

    CFArrayRef WindowList = CGWindowListCopyWindowInfo(WindowListOption, kCGNullWindowID);
    if(WindowList)
    {
        CFIndex WindowCount = CFArrayGetCount(WindowList);
        for(std::size_t Index = 0; Index < WindowCount; ++Index)
        {
            uint32_t WindowID;
            uint32_t WindowLayer;
            CGRect WindowRect = {};
            CFNumberRef CFWindowNumber;
            CFNumberRef CFWindowLayer;
            CFDictionaryRef CFWindowBounds;
            CFDictionaryRef Elem = (CFDictionaryRef)CFArrayGetValueAtIndex(WindowList, Index);
            CFWindowNumber = (CFNumberRef) CFDictionaryGetValue(Elem, CFSTR("kCGWindowNumber"));
            CFWindowLayer = (CFNumberRef) CFDictionaryGetValue(Elem, CFSTR("kCGWindowLayer"));
            CFWindowBounds = (CFDictionaryRef) CFDictionaryGetValue(Elem, CFSTR("kCGWindowBounds"));
            CFNumberGetValue(CFWindowNumber, kCFNumberSInt32Type, &WindowID);
            CFNumberGetValue(CFWindowLayer, kCFNumberSInt32Type, &WindowLayer);
            CFRelease(CFWindowNumber);
            CFRelease(CFWindowLayer);
            if(CFWindowBounds)
            {
                CGRectMakeWithDictionaryRepresentation(CFWindowBounds, &WindowRect);
            }

            CFStringRef CFOwner = (CFStringRef) CFDictionaryGetValue(Elem, CFSTR("kCGWindowOwnerName"));
            CFStringRef CFName = (CFStringRef) CFDictionaryGetValue(Elem, CFSTR("kCGWindowName"));

            if(((CFOwner && (CFStringCompare(CFOwner, CFSTR("Dock"), 0) == kCFCompareEqualTo)) &&
                (CFName && (CFStringCompare(CFName, CFSTR("LPSpringboard"), 0) == kCFCompareEqualTo))) ||
                (WindowLayer == CONTEXT_MENU_LAYER))
            {
                CFRelease(WindowList);
                return 0;
            }

            if(CFOwner && CFStringCompare(CFOwner, CFSTR("kwm-overlay"), 0) == kCFCompareEqualTo)
                continue;

            if(IsElementBelowCursor(&WindowRect))
            {
                Result = WindowID;
                break;
            }
        }

        CFRelease(WindowList);
    }

    return Result;
}

/* NOTE(koekeishiya): Update state of known applications and their windows, stored inside the ax_state passed to AXLibInit(..). */
void AXLibRunningApplications()
{
    std::map<pid_t, std::string> List = SharedWorkspaceRunningApplications();

    std::map<pid_t, std::string>::iterator It;
    for(It = List.begin(); It != List.end(); ++It)
    {
        pid_t PID = It->first;
        if(!AXLibIsApplicationCached(PID))
        {
            std::string Name = It->second;
            (*AXApplications)[PID] = AXLibConstructApplication(PID, Name);
            AXLibInitializeApplication(&(*AXApplications)[PID]);
        }
        else
        {
            AXLibAddApplicationWindows(&(*AXApplications)[PID]);
        }
    }
}

/* NOTE(koekeishiya): This function is responsible for initializing internal variables used by AXLib, and must be
                      called before using any of the provided functions!  In addition to this, it will also
                      populate the display and running applications map in the ax_state struct.  */
void AXLibInit(ax_state *State)
{
    AXState = State;
    AXApplications = &AXState->Applications;
    AXDisplays = &AXState->Displays;
    Carbon = &AXState->Carbon;
    AXUIElementSetMessagingTimeout(AXLibSystemWideElement(), 1.0);
    AXLibInitializeCarbonEventHandler(Carbon, AXApplications);
    SharedWorkspaceInitialize(AXApplications);
    AXLibInitializeDisplays(AXDisplays);
    AXLibRunningApplications();
}
