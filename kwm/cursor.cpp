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
IsCursorInsideRect(double X, double Y, double Width, double Height)
{
    CGPoint Cursor = GetCursorPos();
    if(Cursor.x >= X &&
       Cursor.x <= X + Width &&
       Cursor.y >= Y &&
       Cursor.y <= Y + Height)
        return true;

    return false;
}

EVENT_CALLBACK(Callback_AXEvent_MouseMoved)
{
    FocusWindowBelowCursor();
}

EVENT_CALLBACK(Callback_AXEvent_LeftMouseDown)
{
    if((FocusedApplication && FocusedApplication->Focus) &&
       (!IsCursorInsideRect(FocusedApplication->Focus->Position.x,
                            FocusedApplication->Focus->Position.y,
                            FocusedApplication->Focus->Size.width,
                            FocusedApplication->Focus->Size.height)))
    {
        DEBUG("AXEvent_LeftMouseDown");
        DragMoveWindow = true;
    }
}

EVENT_CALLBACK(Callback_AXEvent_LeftMouseUp)
{
    /* TODO(koekeishiya): Can we simplify some of the error checking going on (?) */
    if(DragMoveWindow)
    {
        DEBUG("AXEvent_LeftMouseUp");
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
    CGPoint *Cursor = (CGPoint *) Event->Context;

    if(DragMoveWindow)
    {
        DEBUG("AXEvent_LeftMouseDragged");

        ax_window *FocusedWindow = NULL;
        if(FocusedApplication)
            FocusedWindow = FocusedApplication->Focus;

        if((FocusedWindow) &&
           (AXLibHasFlags(FocusedWindow, AXWindow_Floating)))
        {
            double X = Cursor->x - FocusedWindow->Size.width / 2;
            double Y = Cursor->y - FocusedWindow->Size.height / 2;
            AXLibSetWindowPosition(FocusedWindow->Ref, X, Y);
        }
        else
        {
            uint32_t WindowID = AXLibGetWindowBelowCursor();
            if(WindowID != 0)
            {
                ax_window *Window = GetWindowByID(WindowID);
                if(Window)
                {
                    MarkedWindow = Window;
                    UpdateBorder(&MarkedBorder, MarkedWindow);
                }
            }
        }
    }

    free(Cursor);
}

void MoveCursorToCenterOfTreeNode(tree_node *Node)
{
    if((HasFlags(&KWMSettings, Settings_MouseFollowsFocus)) &&
       (!IsCursorInsideRect(Node->Container.X, Node->Container.Y,
                            Node->Container.Width, Node->Container.Height)))
    {
        CGWarpMouseCursorPosition(CGPointMake(Node->Container.X + Node->Container.Width / 2,
                                              Node->Container.Y + Node->Container.Height / 2));
    }
}

void MoveCursorToCenterOfLinkNode(link_node *Link)
{
    if((HasFlags(&KWMSettings, Settings_MouseFollowsFocus)) &&
       (!IsCursorInsideRect(Link->Container.X, Link->Container.Y,
                            Link->Container.Width, Link->Container.Height)))
    {
        CGWarpMouseCursorPosition(CGPointMake(Link->Container.X + Link->Container.Width / 2,
                                              Link->Container.Y + Link->Container.Height / 2));
    }
}

void MoveCursorToCenterOfWindow(ax_window *Window)
{
    if((HasFlags(&KWMSettings, Settings_MouseFollowsFocus)) &&
       (!IsCursorInsideRect(Window->Position.x, Window->Position.y,
                            Window->Size.width, Window->Size.height)))
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
         if((FocusedWindow) && (IsCursorInsideRect(FocusedWindow->Position.x, FocusedWindow->Position.y,
                                                   FocusedWindow->Size.width, FocusedWindow->Size.height)))
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
