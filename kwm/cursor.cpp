#include "cursor.h"
#include "window.h"
#include "node.h"
#include "tree.h"
#include "space.h"
#include "border.h"
#include "axlib/axlib.h"

#define internal static
extern std::map<std::string, space_info> WindowTree;
extern ax_state AXState;
extern ax_application *FocusedApplication;
extern ax_window *MarkedWindow;
extern kwm_settings KWMSettings;
extern kwm_border MarkedBorder;

internal bool DragMoveWindow = false;

internal inline CGPoint
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

EVENT_CALLBACK(Callback_AXEvent_MouseMoved)
{
    FocusWindowBelowCursor();
}

EVENT_CALLBACK(Callback_AXEvent_LeftMouseDown)
{
    DEBUG("AXEvent_LeftMouseDown");
    if((FocusedApplication && FocusedApplication->Focus) &&
        IsWindowBelowCursor(FocusedApplication->Focus))
    {
        DragMoveWindow = true;
    }
}

EVENT_CALLBACK(Callback_AXEvent_LeftMouseUp)
{
    /* TODO(koekeishiya): Can we simplify some of the error checking going on (?) */
    DEBUG("AXEvent_LeftMouseUp");

    if(DragMoveWindow)
    {
        DragMoveWindow = false;

        ax_window *FocusedWindow = FocusedApplication->Focus;
        if(!FocusedWindow || !MarkedWindow || (MarkedWindow == FocusedWindow))
        {
            ClearMarkedWindow();
            return;
        }

        ax_display *Display = AXLibWindowDisplay(FocusedWindow);
        if(!Display)
        {
            ClearMarkedWindow();
            return;
        }

        space_info *SpaceInfo = &WindowTree[Display->Space->Identifier];
        tree_node *TreeNode = GetTreeNodeFromWindowIDOrLinkNode(SpaceInfo->RootNode, FocusedWindow->ID);
        if(TreeNode)
        {
            tree_node *NewFocusNode = GetTreeNodeFromWindowID(SpaceInfo->RootNode, MarkedWindow->ID);
            if(NewFocusNode)
            {
                SwapNodeWindowIDs(TreeNode, NewFocusNode);
            }
        }

        ClearMarkedWindow();
    }
}

EVENT_CALLBACK(Callback_AXEvent_LeftMouseDragged)
{
    DEBUG("AXEvent_LeftMouseDragged");
    CGPoint *Cursor = (CGPoint *) Event->Context;

    if(DragMoveWindow)
    {
        uint32_t WindowID = AXLibGetWindowBelowCursor();
        if(WindowID != 0)
        {
            ax_window *Window = GetWindowByID(WindowID);
            if(Window)
            {
                if(AXLibHasFlags(Window, AXWindow_Floating))
                {
                    double X = Cursor->x - Window->Size.width / 2;
                    double Y = Cursor->y - Window->Size.height / 2;
                    AXLibSetWindowPosition(Window->Ref, X, Y);
                }
                else
                {
                    MarkedWindow = Window;
                    UpdateBorder(&MarkedBorder, MarkedWindow);
                }
            }
        }
    }

    free(Cursor);
}

void MoveCursorToCenterOfWindow(ax_window *Window)
{
    if((HasFlags(&KWMSettings, Settings_MouseFollowsFocus)) &&
       (!IsWindowBelowCursor(Window)))
    {
        CGWarpMouseCursorPosition(CGPointMake(Window->Position.x + Window->Size.width / 2,
                                              Window->Position.y + Window->Size.height / 2));
    }
}

void MoveCursorToCenterOfFocusedWindow()
{
    if(FocusedApplication && FocusedApplication->Focus)
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
