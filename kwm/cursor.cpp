#include "cursor.h"
#include "axlib/axlib.h"
#include "space.h"

#define internal static
#define local_persist static

extern ax_state AXState;
extern ax_application *FocusedApplication;
extern kwm_settings KWMSettings;

internal CGPoint
GetCursorPos()
{
    CGEventRef Event = CGEventCreate(NULL);
    CGPoint Cursor = CGEventGetLocation(Event);
    CFRelease(Event);

    return Cursor;
}

internal bool
IsWindowBelowCursor(ax_window *Window)
{
    CGPoint Cursor = GetCursorPos();
    if(Cursor.x >= Window->Position.x &&
       Cursor.x <= Window->Position.x + Window->Size.width &&
       Cursor.y >= Window->Position.y &&
       Cursor.y <= Window->Position.y + Window->Size.height)
        return true;

    return false;
}

void MoveCursorToCenterOfWindow(ax_window *Window)
{
    if(HasFlags(&KWMSettings, Settings_MouseFollowsFocus))
    {
        CGWarpMouseCursorPosition(CGPointMake(Window->Position.x + Window->Size.width / 2,
                                              Window->Position.y + Window->Size.height / 2));
    }
}

void MoveCursorToCenterOfFocusedWindow()
{
    if(HasFlags(&KWMSettings, Settings_MouseFollowsFocus) &&
       FocusedApplication &&
       FocusedApplication->Focus)
        MoveCursorToCenterOfWindow(FocusedApplication->Focus);
}


void FocusWindowBelowCursor()
{
    ax_application *Application = AXLibGetFocusedApplication();
    ax_window *FocusedWindow = NULL;
    if(Application)
    {
        FocusedWindow = Application->Focus;
        if(FocusedWindow && IsWindowBelowCursor(FocusedWindow))
            return;
    }

    uint32_t WindowID = AXLibGetWindowBelowCursor();
    if(WindowID == 0)
        return;

   std::map<pid_t, ax_application>::iterator It;
    for(It = AXState.Applications.begin(); It != AXState.Applications.end(); ++It)
    {
        ax_application *Application = &It->second;
        ax_window *Window = AXLibFindApplicationWindow(Application, WindowID);
        if(Window)
        {
            if((AXLibIsWindowStandard(Window)) ||
               (AXLibIsWindowCustom(Window)))
            {
                if(Application == Window->Application)
                {
                    if(FocusedWindow != Window)
                        AXLibSetFocusedWindow(Window);
                }
                else
                {
                    AXLibSetFocusedWindow(Window);
                }
            }
            return;
        }
    }
}

EVENT_CALLBACK(Callback_AXEvent_MouseMoved)
{
    if(!AXLibIsSpaceTransitionInProgress())
        FocusWindowBelowCursor();
}

