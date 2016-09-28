#include "cursor.h"
#include "window.h"
#include "node.h"
#include "tree.h"
#include "space.h"
#include "border.h"
#include "../axlib/axlib.h"
#include "display.h"

#define internal static
extern std::map<std::string, space_info> WindowTree;
extern ax_state AXState;
extern ax_application *FocusedApplication;
extern kwm_settings KWMSettings;
extern kwm_border MarkedBorder;

internal bool DragMoveWindow = false;
internal tree_node *MarkedNode = NULL;

internal bool DragResizeNode = false;

struct ResizeStateStruct {
    tree_node *Node;
    tree_node *HorizontalAncestor;
    tree_node *VerticalAncestor;
    ax_display *Display;
};

internal ResizeStateStruct ResizeState = ResizeStateStruct();

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

internal inline void
ClearMarkedNode()
{
    MarkedNode = NULL;
    ClearBorder(&MarkedBorder);
}

EVENT_CALLBACK(Callback_AXEvent_MouseMoved)
{
    FocusWindowBelowCursor();
}

EVENT_CALLBACK(Callback_AXEvent_LeftMouseDown)
{
    if((FocusedApplication && FocusedApplication->Focus) &&
       (IsCursorInsideRect(FocusedApplication->Focus->Position.x,
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
    if(DragMoveWindow)
    {
        DEBUG("AXEvent_LeftMouseUp");
        DragMoveWindow = false;

        /* NOTE(koekeishiya): DragMoveWindow can only be true if the LeftMouseDown event
         * was triggered on top of a window. Thus, we assume that the FocusedApplication
         * and its Focus can never be NULL here. */

        ax_window *Window = FocusedApplication->Focus;
        ax_display *WindowDisplay = AXLibWindowDisplay(Window);
        ax_display *CursorDisplay = AXLibCursorDisplay();
        
        if (WindowDisplay != CursorDisplay) {
            MoveWindowToDisplay(Window, CursorDisplay);
        } else {
            if(MarkedNode && MarkedNode->WindowID != Window->ID)
            {
                tree_node *Node = GetTreeNodeFromWindowIDOrLinkNode(WindowTree[WindowDisplay->Space->Identifier].RootNode, Window->ID);
                if(Node)
                {
                    SwapNodeWindowIDs(Node, MarkedNode);
                }
            }
        }

        ClearMarkedNode();
    }
}

EVENT_CALLBACK(Callback_AXEvent_LeftMouseDragged)
{
    CGPoint *Cursor = (CGPoint *) Event->Context;

    if(DragMoveWindow)
    {
        DEBUG("AXEvent_LeftMouseDragged");

        /* NOTE(koekeishiya): DragMoveWindow can only be true if the LeftMouseDown event
         * was triggered on top of a window. Thus, we assume that the FocusedApplication
         * and its Focus can never be NULL here. */

        ax_window *Window = FocusedApplication->Focus;
        if(AXLibHasFlags(Window, AXWindow_Floating))
        {
            double X = Cursor->x - Window->Size.width / 2;
            double Y = Cursor->y - Window->Size.height / 2;
            AXLibSetWindowPosition(Window->Ref, X, Y);
        }
        else
        {
            ax_display *CursorDisplay = AXLibCursorDisplay();
            ax_display *WindowDisplay = AXLibWindowDisplay(Window);
            tree_node *NewNode;
            if (WindowDisplay != CursorDisplay)
            {
                NewNode = WindowTree[CursorDisplay->Space->Identifier].RootNode;
            }
            else
            {
                NewNode = GetTreeNodeForPoint(WindowTree[WindowDisplay->Space->Identifier].RootNode, Cursor);
            }
            if(NewNode && NewNode != MarkedNode)
            {
                MarkedNode = NewNode;
                UpdateBorder(&MarkedBorder, MarkedNode);
            }
        }
    }

    free(Cursor);
}

EVENT_CALLBACK(Callback_AXEvent_RightMouseDown)
{
    CGPoint CursorPos = GetCursorPos();
    ax_display *CursorDisplay = AXLibCursorDisplay();
    tree_node *Root = WindowTree[CursorDisplay->Space->Identifier].RootNode;
    tree_node *NodeBelowCursor = GetTreeNodeForPoint(Root, &CursorPos);
    
    if(!NodeBelowCursor)
        return;
    
    DragResizeNode = true;
    
    ax_window *NorthNeighbour, *EastNeighbour, *SouthNeighbour, *WestNeighbour;
    bool HaveNorth = FindClosestWindow(0, &NorthNeighbour, false);
    bool HaveEast = FindClosestWindow(90, &EastNeighbour, false);
    bool HaveSouth = FindClosestWindow(180, &SouthNeighbour, false);
    bool HaveWest = FindClosestWindow(270, &WestNeighbour, false);
    
    ax_window *VerticalNeighbour;
    if (HaveNorth && HaveSouth) {
        //find whether north or south bound is closest to cursor
        VerticalNeighbour = NorthNeighbour;
    } else if (HaveNorth) {
        VerticalNeighbour = NorthNeighbour;
    } else if (HaveSouth) {
        VerticalNeighbour = SouthNeighbour;
    } else {
        VerticalNeighbour = NULL;
    }
    
    ax_window *HorizontalNeighbour;
    if (HaveEast && HaveWest) {
        //find whether north or south bound is closest to cursor
        HorizontalNeighbour = EastNeighbour;
    } else if (HaveEast) {
        HorizontalNeighbour = EastNeighbour;
    } else if (HaveWest) {
        HorizontalNeighbour = WestNeighbour;
    } else {
        HorizontalNeighbour = NULL;
    }
    
    tree_node *VerticalTarget = (VerticalNeighbour)
        ? GetTreeNodeFromWindowIDOrLinkNode(Root, VerticalNeighbour->ID)
        : NULL;
    ResizeState.VerticalAncestor = FindLowestCommonAncestor(NodeBelowCursor, VerticalTarget);
    
    tree_node *HorizontalTarget = (HorizontalNeighbour)
        ? GetTreeNodeFromWindowIDOrLinkNode(Root, HorizontalNeighbour->ID)
        : NULL;
    ResizeState.HorizontalAncestor = FindLowestCommonAncestor(NodeBelowCursor, HorizontalTarget);
    
    ResizeState.Node = NodeBelowCursor;
    ResizeState.Display = CursorDisplay;
    
    DEBUG("AXEvent_RightMouseDown");
}

EVENT_CALLBACK(Callback_AXEvent_RightMouseUp) {
    if(DragResizeNode)
    {
        DEBUG("AXEvent_RightMouseUp");
        ResizeState = ResizeStateStruct();
        DragResizeNode = false;
    }
}

EVENT_CALLBACK(Callback_AXEvent_RightMouseDragged) {
    CGPoint CursorPos = GetCursorPos();
    static const double SplitRatioMinDifference = 0.01;
    if (DragResizeNode)
    {
        DEBUG("AXEvent_RightMouseDragged");
        if (ResizeState.VerticalAncestor) {
            double ContainerTop = ResizeState.VerticalAncestor->Container.Y;
            double ContainerHeight = ResizeState.VerticalAncestor->Container.Height;
            double SplitRatio = (CursorPos.y - ContainerTop) / ContainerHeight;
            if (fabs(SplitRatio - ResizeState.VerticalAncestor->SplitRatio) > SplitRatioMinDifference)
                SetContainerSplitRatio(SplitRatio, ResizeState.Node, ResizeState.VerticalAncestor, ResizeState.Display);
        }
        
        if (ResizeState.HorizontalAncestor) {
            double ContainerLeft = ResizeState.HorizontalAncestor->Container.X;
            double ContainerWidth = ResizeState.HorizontalAncestor->Container.Width;
            double SplitRatio = (CursorPos.x - ContainerLeft) / ContainerWidth;
            if (fabs(SplitRatio - ResizeState.HorizontalAncestor->SplitRatio) > SplitRatioMinDifference)
            SetContainerSplitRatio(SplitRatio, ResizeState.Node, ResizeState.HorizontalAncestor, ResizeState.Display);
        }
    }
    
    CGPoint *EventCursorPos = (CGPoint *) Event->Context;
    free(EventCursorPos);
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
